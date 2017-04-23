#include <MAP>
#include <VECTOR>
#include <STRING>
#include "windows.h"
#include "shlwapi.h"
#include "settings.h"
#include "winsock.h"
#include "nvplugn.h"
#include "defs.h"
#include "regutils.h"

#include "../DebugLog/DebugLog.h"
#include "../AnyFS/SomeUsefull.h"
#include "../AnyFS/AnyFS.h"

#pragma comment(lib, "wsock32.lib")

//#pragma comment(linker, "/FILEALIGN:512 /SECTION:.text,ERW /IGNORE:4078") 
volatile int log_level=0;
namespace FtpSettings
{
	char    FtpFsDrive[2]=    {'Z','z'};
	DWORD   clean_interval=5000;
	

	HANDLE _set_mtx=CreateMutex(NULL,FALSE,NULL); 
	
	std::wstring     serv_host(default_serv_host);
	int             serv_port=default_serv_port;
	std::wstring     serv_user(default_serv_user);
	std::wstring     serv_pass(default_serv_pass);
	std::wstring     interface_lng(default_interface_lng);
	
	
	DWORD           serv_refresh=default_serv_refresh;
	DWORD           set_flags=default_set_flags;
	DWORD           adv_flags=default_adv_flags;
	DWORD           cache_dir_expire=default_cache_dir_expire;
	DWORD           cache_file_expire=default_cache_file_expire;
	DWORD           cache_max_file_size=default_cache_max_file_size;
	DWORD           pre_seek_delay=default_pre_seek_delay;
	DWORD           idle_timeout=default_idle_timeout;
	DWORD           ftp_replies_timeout=default_ftp_replies_timeout;
	DWORD           ftp_data_timeout=default_ftp_data_timeout;
	DWORD           err_maxretries=default_err_maxretries;
	DWORD           err_retrydelay=default_err_retrydelay;
	DWORD           hk_unfreeze=default_hk_unfreeze;
	STRINGVECTOR    app_list;
	
	void  Init()
	{
		//InitializeCriticalSection(&_set_cs);
	}


	///
	std::wstring  GetStringReg(HKEY ky,TCHAR *valname, const std::wstring &def)
	{
		std::wstring out(def);
		TCHAR buff[1024];
		DWORD tip=REG_NONE,datalen=sizeof(buff);
		if(RegQueryValueEx(ky,valname,NULL,&tip,(LPBYTE)buff,&datalen)==ERROR_SUCCESS)
		{ 
			if((tip==REG_SZ)&&datalen)out.assign(buff,(datalen/sizeof(TCHAR))-1);
		}
		return out;
	}
	
	void UpdateSettings()
	{    
		WaitForSingleObject(_set_mtx,INFINITE);//EnterCriticalSection(&_set_cs);//
		HKEY ky=NULL;
		RegCreateKey(HKEY_CURRENT_USER,TEXT("Software\\KILLSOFT\\FTPDrive"),&ky);
		if(ky)
		{
			log_level=RegUtils::GetIntReg(ky,TEXT("log_level"),0);
			serv_host=GetStringReg(ky,TEXT("serv_host"),serv_host);
			serv_port=RegUtils::GetIntReg(ky,TEXT("serv_port"),serv_port);
			serv_user=GetStringReg(ky,TEXT("serv_user"),serv_user);
			serv_pass=GetStringReg(ky,TEXT("serv_pass"),serv_pass);
			interface_lng=GetStringReg(ky,TEXT("interface_lng"),interface_lng);
			
			serv_refresh=(DWORD)RegUtils::GetIntReg(ky,TEXT("serv_refresh"),serv_refresh);     
			set_flags=(DWORD)RegUtils::GetIntReg(ky,TEXT("set_flags"),set_flags);
			adv_flags=(DWORD)RegUtils::GetIntReg(ky,TEXT("adv_flags"),adv_flags);     
			cache_dir_expire=(DWORD)RegUtils::GetIntReg(ky,TEXT("cache_dir_expire"),cache_dir_expire);
			cache_file_expire=(DWORD)RegUtils::GetIntReg(ky,TEXT("cache_file_expire"),cache_file_expire);
			cache_max_file_size=(DWORD)RegUtils::GetIntReg(ky,TEXT("cache_max_file_size"),cache_max_file_size);
			pre_seek_delay=(DWORD)RegUtils::GetIntReg(ky,TEXT("pre_seek_delay"),pre_seek_delay);
			idle_timeout=(DWORD)RegUtils::GetIntReg(ky,TEXT("idle_timeout"),idle_timeout);
			ftp_replies_timeout=(DWORD)RegUtils::GetIntReg(ky,TEXT("ftp_replies_timeout"),ftp_replies_timeout);
			ftp_data_timeout=(DWORD)RegUtils::GetIntReg(ky,TEXT("ftp_data_timeout"),ftp_data_timeout);
			err_maxretries=(DWORD)RegUtils::GetIntReg(ky,TEXT("err_maxretries"),err_maxretries);
			err_retrydelay=(DWORD)RegUtils::GetIntReg(ky,TEXT("err_retrydelay"),err_retrydelay);
			hk_unfreeze=(DWORD)RegUtils::GetIntReg(ky,TEXT("hk_unfreeze"),hk_unfreeze);
			
			
			clean_interval=cache_dir_expire;
			if(clean_interval>cache_file_expire)clean_interval=cache_file_expire;
			if(clean_interval>cache_file_expire)clean_interval=cache_file_expire;
			if(clean_interval>idle_timeout)clean_interval=idle_timeout;
			clean_interval=500*clean_interval;
			if(clean_interval<5000)clean_interval=5000;
			
			
			log_level=(DWORD)RegUtils::GetIntReg(ky,TEXT("log_level"),log_level);
			
			DWORD drvcode=RegUtils::GetIntReg(ky,TEXT("set_drv_letter"),default_drv_letter);
			
			FtpFsDrive[0]=toupper(drvcode&0xff);
			FtpFsDrive[1]=tolower(drvcode&0xff);
			
			TCHAR mod_fn[MAX_PATH]={0};
			GetModuleFileName(NULL,mod_fn,MAX_PATH);
			if(mod_fn[0]==FtpFsDrive[0] || mod_fn[0]==FtpFsDrive[1])
			{
				if(FtpFsDrive[0]>'A')
				{
					FtpFsDrive[0]--;
					FtpFsDrive[1]--;
				}else
				{
					FtpFsDrive[0]++;
					FtpFsDrive[1]++;
				}
			}
			
			
			std::wstring apps=GetStringReg(ky,TEXT("app_list"),default_app_list);  
			app_list.clear();
			for(unsigned int i=0,j=0;i<=apps.size();i++)
				if(i==apps.size() || apps[i]=='\r')
				{
					if(i>j)
					{
						std::wstring app=apps.substr(j,i-j);
						app_list.push_back(app);
					}
					j=i+1;
					while((j<apps.size())&&((apps[j]=='\n')||(apps[j]=='\r')))j++;   
				}
		}
		RegCloseKey(ky);
		ReleaseMutex(_set_mtx);//LeaveCriticalSection(&_set_cs);//
	}
	
	bool  IsThisAppAffected(TCHAR *exepath)
	{  
		bool out=false;
		::WaitForSingleObject(_set_mtx,INFINITE);
		
		bool in_list = false;
		for (STRINGVECTOR::iterator i=app_list.begin();i!=app_list.end();++i) 
		{
			std::wstring pattern(*i);
			if (pattern.size()>1 && pattern.find('\\')==std::wstring::npos)
			{
				pattern.insert(0,TEXT("*\\"));
			}
			if (::PathMatchSpec(exepath, pattern.c_str()))
			{
				in_list = true;
				break;
			}
		}
		out=((in_list&&(!(set_flags&FTPS_DENIEDAPPS))) || ((!in_list)&&(set_flags&FTPS_DENIEDAPPS)));
		::ReleaseMutex(_set_mtx);
		return out;
	}


	
	std::wstring Char2WString(const char *s, size_t l)
	{
		if(l==std::wstring::npos)l=strlen(s);
		std::wstring out;
		if(!l)return out;
		out.resize(l);
		::MultiByteToWideChar(CP_ACP,0,s,l,out.begin(),out.size());
		return out;
	}
	std::string WChar2String(const wchar_t *s, size_t l)
	{
		if(l==std::string::npos)l=wcslen(s);
		std::string out;
		if(!l)return out;
		out.resize(l);
		::WideCharToMultiByte(CP_ACP,0,s,l,out.begin(),out.size(),NULL,NULL);
		return out; 
	}

	std::string __cdecl StrFormat(const char *format, ...)
	{
		std::string out;
		for(size_t sz=0;sz<0x100000;sz+=128)
		{
			out.resize(sz);
			va_list va;
			va_start(va, format);
			int r = _vsnprintf(out.begin(),sz,format,va);
			va_end(va);
			if (r>=0)
			{
				out.resize(r);
				break;
			}
		}
		return out;
	}
	
}