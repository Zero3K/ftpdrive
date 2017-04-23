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
#include "process.h"

#include "../DebugLog/DebugLog.h"
#include "../AnyFS/SomeUsefull.h"
#include "../AnyFS/AnyFS.h"




typedef DWORD (WINAPI *NVGetHostx)(void *Reserved, NVHOST *nvh,DWORD flags);
typedef DWORD (WINAPI *NVGetMetaVarx)(void *Reserved, DWORD HostID,const char *VarName, char *DataBuff,DWORD *BuffSize);
typedef DWORD (WINAPI *NVSetMetaVarx)(void *Reserved, DWORD HostID,const char *VarName, char *DataBuff,DWORD BuffSize);
typedef DWORD (WINAPI *NVGetNetViewStatusx)(void *Reserved);
typedef DWORD (WINAPI *NVSetEventHandlerProcx)(void *proc);

NVGetHostx NVGetHost = NULL;
NVGetMetaVarx NVGetMetaVar = NULL;
NVGetNetViewStatusx NVGetNetViewStatus = NULL;



class FtpSitesList : public IFtpSitesList
{
private:
	HANDLE	_set_mtx; 
	DWORD	_serv_last_ts;
	bool	_list_retrieved;
	volatile bool _in_get_thread;
	DWORD	_next_get_thread;

		
	typedef struct FTPHOST_{
		std::wstring	host;
		std::wstring	user;
		std::wstring	pass;
		std::wstring	home;
		unsigned int	port;
		unsigned int	flags;
		unsigned int	conlim;
		ENCCP			enc;
		FTPSMODE		smode;

		std::wstring	nv_hostname;
		std::wstring	nv_hostip;
		std::wstring	nv_user;
		std::wstring	nv_pass;
		unsigned int	nv_flags;
	}FTPHOST;

	typedef std::map<std::string,std::string,string_less_no_case> STR2STR;
	typedef std::map<std::wstring,FTPHOST,wstring_less_no_case> HOSTSMAP;
	
	HOSTSMAP _hosts_map;
	std::wstring _file;
	bool   _for_edit;
	bool   _from_remote;
	std::string _nv_hostname,_nv_hostip,_nv_user,_nv_pass,_nv_flags;
	
	std::string GetHostMV(DWORD id, const char *mv)
	{
		CHARVECTOR varbuf(256);
		for(;;)
		{
			DWORD bufsz=varbuf.size();
			DWORD r=NVGetMetaVar(0,id,mv,&varbuf[0],&bufsz);
			switch(r)
			{
			case 1:
				return std::string(&varbuf[0],bufsz);
				
			case 0:
				return std::string();
			}
			varbuf.resize(bufsz+64);
		}
	}
	

	void LoadNVHost(
		HOSTSMAP &tmp_map,
		STR2STR &name2val
		)
	{
		DWORD dw_flags=atoi(name2val["nv_flags"].c_str());

		name2val["Name"]=name2val["nv_hostname"];
		name2val["Host"]=name2val["nv_hostip"].empty()?name2val["nv_hostname"]:name2val["nv_hostip"];
		name2val["Mode"]=Mode2Str(dw_flags);
		name2val["Login"]=name2val["nv_user"];
		name2val["Password"]=name2val["nv_pass"];
		
		const std::string &nv_fdrv=name2val["nv_fdrv"];
		if (!nv_fdrv.empty())
		{
			ParseFileContent(nv_fdrv,tmp_map,name2val);
		}
		else if(dw_flags&0x1400)
		{
			InsertHostEntry(tmp_map, name2val, STR2STR());
		}
	}

	bool LoadNVApiByPathName(const TCHAR *fpath, const TCHAR *fname)
	{
		std::wstring pathname;
		if (fpath && *fpath)	
		{
			pathname.assign(fpath);
			if (pathname[pathname.size()-1]!='\\')
				pathname.append((size_t)1,'\\');
		}
		
		pathname.append(fname);
		
		HMODULE lib=::LoadLibrary(pathname.c_str());
		if (!lib)
			return false;

		NVGetHost=(NVGetHostx)GetProcAddress(lib,"NVGetHostA");
		NVGetMetaVar=(NVGetMetaVarx)GetProcAddress(lib,"NVGetMetaVarA");
		NVGetNetViewStatus=(NVGetNetViewStatusx)GetProcAddress(lib,"NVGetNetViewStatus"); 

		if (NVGetHost&&NVGetMetaVar&&NVGetNetViewStatus)
			return true;

		::FreeLibrary(lib);
		return false;
	}

	bool LoadNVApiDll()
	{
		if (NVGetHost&&NVGetMetaVar&&NVGetNetViewStatus)
			return true;

		TCHAR path_buf[MAX_PATH+1];
		if (::GetWindowsDirectory(path_buf,MAX_PATH))
			path_buf[MAX_PATH]=0;
		else
			path_buf[0]=0;
		
		
		if (path_buf[0])
		{
			if (LoadNVApiByPathName(path_buf, TEXT("kpnvapi.dll")))
				return true;
		}
		
		if (LoadNVApiByPathName(NULL, TEXT("kpnvapi.dll")))
			return true;

		if (path_buf[0])
		{
			if (LoadNVApiByPathName(path_buf, TEXT("nvapi.dll")))
				return true;
		}

		if (LoadNVApiByPathName(NULL, TEXT("nvapi.dll")))
			return true;

		return false;
	}

	bool  GetFromLocal()
	{
		HOSTSMAP _tmp_map;
		if (!LoadNVApiDll())
		{
			MyWriteDebugLog("GetFromLocal failed: LoadNVApiDll le=%u\r\n",::GetLastError());
			return false;
		}
		
		if (!NVGetNetViewStatus(NULL))
		{
			MyWriteDebugLog("GetFromLocal failed: NVGetNetViewStatus: no access\r\n");
			return false;
		}

		NVHOST hst; ZeroMemory(&hst,sizeof(hst));
		do
		{   
			hst.id=hst.nextid;hst.nextid=0;
			NVGetHost(0,&hst,0);   
			if(hst.id)
			{       
				std::string v=GetHostMV(hst.id, "ison");
				if(!strcmp(v.c_str(),"on"))
				{
					STR2STR name2val;
					name2val["nv_hostname"]=hst.hostname;
					name2val["nv_hostip"]=hst.hostip;
					name2val["nv_flags"]=GetHostMV(hst.id, "flags");
					name2val["nv_user"]=GetHostMV(hst.id, "user");
					name2val["nv_pass"]=GetHostMV(hst.id, "pass");
					name2val["nv_fdrv"]=GetHostMV(hst.id, "!fdrv");
					LoadNVHost(_tmp_map,name2val);
				}
			}
		}while(hst.nextid);

		WaitForSingleObject(_set_mtx,INFINITE);//EnterCriticalSection(&_set_cs);
		_hosts_map=_tmp_map;
		ReleaseMutex(_set_mtx);//LeaveCriticalSection(&_set_cs);//
		return true;
	}
	
#define HOSTENUM_REQUEST	"\r\nhostenum#ison#flags#!fdrv#"

	int xxxunformatstr(char *str)
	{
		char *trg=str,*src=str;
		while(*src)
		{
			if(*src=='\\')
			{
				src++;
				switch(*src)
				{
				case '1':*trg='\x0D';break;
				case '2':*trg='\x0A';break;
				case '3':*trg='\x00';break;
				case '4':*trg='$';break;
				case '5':*trg='#';break;
				case '6':*trg='=';break;
				case '7':*trg='\\';break;
				case '8':*trg='\x09';break;
				case '9':*trg=(char)32;break;
				}
			}else *trg=*src;
			trg++;src++;
		}
		return trg-str;
	}

	void nv_unformat(std::string &s)
	{
		if (!s.empty())
		{
			int sz = xxxunformatstr(s.begin());
			s.resize(sz);
		}
	}

	bool  GetFromRemote()
	{    
		HOSTSMAP _tmp_map;
		bool got=false;
		int sc=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
		if(sc!=INVALID_SOCKET)
		{
			sockaddr_in sin={AF_INET,0};
			sin.sin_port=ntohs(FtpSettings::serv_port);
			sin.sin_addr.S_un.S_addr=FtpSettings::inet_addrW(FtpSettings::serv_host.c_str());
			if(connect(sc,(sockaddr *)&sin,sizeof(sin))==0)
			{
				send(sc,FtpSettings::WString2String(FtpSettings::serv_user).c_str(),FtpSettings::serv_user.size(),0);
				send(sc,"#",1,0);
				send(sc,FtpSettings::WString2String(FtpSettings::serv_pass).c_str(),FtpSettings::serv_pass.size(),0);
				send(sc,HOSTENUM_REQUEST,sizeof(HOSTENUM_REQUEST)-1,0);
				if(FtpSettings::set_flags&FTPS_USESTOREDCREDENTIALS)
					send(sc,"user#pass#\r\n",12,0);
				else 
					send(sc,"\r\n",2,0);
				
				std::string strbuf;
				bool fst=1,valid=1;
				
				for(;;)
				{
					if(strbuf.compare("\r\n")==0)
					{
						got=true;
						break;
					}
					char buff[16384];
					int len=recv(sc,buff,16384,0);
					if(len<=0)break;
					strbuf.append(buff,len);
					int x=strbuf.find('$');
					while(x!=std::wstring::npos)
					{
						std::string hstr=strbuf.substr(0,x);
						strbuf.erase(0,x+1);
						int j=0,k=0;
						
						STR2STR name2val;
						std::string ison("?"),fdrv;
						DWORD flgs=7;
						
						for(size_t i=0;i<hstr.size();i++)
							if(hstr[i]=='#')
							{
								std::string ts=hstr.substr(j,i-j);
								if(fst)
								{
									if(ts.compare("hostenum")!=0)valid=false;       
									fst=0;
								}else if(valid)
								{//process host
									switch(k)
									{
									case 0:
										nv_unformat(ts);
										name2val["nv_hostname"]=ts;
										break;
										
									case 1:
										nv_unformat(ts);
										name2val["nv_hostip"]=ts;
										break;
										
									default:
										{        
											int y=ts.find('=');
											if((y!=std::wstring::npos)&&(y<(int)ts.size()))
											{         
												std::string vardata=ts.substr(y+1,ts.size()-y-1);
												ts.resize(y);
												nv_unformat(vardata);
												if(!ts.compare("ison"))
													ison=vardata;
												else if(!ts.compare("flags"))
													name2val["nv_flags"]=vardata;
												else if(!ts.compare("user"))
													name2val["nv_user"]=vardata;
												else if(!ts.compare("pass"))
													name2val["nv_pass"]=vardata;
												else if(!ts.compare("!fdrv"))
													name2val["nv_fdrv"]=vardata;
											}
										}
									}        
									k++;
								}          
								j=i+1;
							}
							
							if(ison.compare("on")==0)
							{
								LoadNVHost(_tmp_map,name2val);
							}     
							x=strbuf.find('$');
					}   
					if(!valid)break;
				}
			}
			closesocket(sc);
		}
		if(got)
		{
			WaitForSingleObject(_set_mtx,INFINITE);//EnterCriticalSection(&_set_cs);//
			_hosts_map=_tmp_map;
			ReleaseMutex(_set_mtx);//LeaveCriticalSection(&_set_cs);//
		}
		return got;
	}
	
	void StrTrim(std::string &s)
	{
		while(!s.empty() && (s[0]==' '||s[0]=='\t'))s.erase(0,1);
		while(!s.empty() && (s[s.size()-1]==' '||s[s.size()-1]=='\t'))s.erase(s.size()-1,1);
	}
	
	std::string GetValue(const STR2STR &name2val, const STR2STR &def_name2val, const char *name, const char *def_val = NULL)
	{
		STR2STR::const_iterator i=name2val.find(name);
		
		if(i==name2val.end())
		{
			i=def_name2val.find(name);
			if(i==def_name2val.end())
				return def_val?def_val:std::string();
		}
		
		return i->second;
	}
	
	void InsertHostEntry(HOSTSMAP &tmp_map, const STR2STR &name2val, const STR2STR &def_name2val)
	{
		std::string host(GetValue(name2val, def_name2val, "Host"));
		std::string name(GetValue(name2val, def_name2val, "Name", host.c_str()));

		if(host.empty())
			host=_nv_hostip.empty()?_nv_hostname:_nv_hostip;

		if(host.empty())
			return;
		
		if(name.empty())
			name=_nv_hostname;

		if(name.empty())
			name = host;
		std::string user(GetValue(name2val, def_name2val, "Login"));
		std::string pass(GetValue(name2val, def_name2val, "Password"));

		if(user.empty())
			user = _nv_user;

		if(pass.empty())
			pass = _nv_pass;

		if(user.empty())
		{
			user = "anonymous";
			if(pass.empty())
				pass = "ftpdrive@killprog.com";
		}


		
		std::string mode(GetValue(name2val, def_name2val, "Mode", "Default"));
		std::string enc(GetValue(name2val, def_name2val, "Encoding"));
		std::string smode(GetValue(name2val, def_name2val, "Secure"));
		
		FTPHOST fh;
		fh.host=FtpSettings::String2WString(host);
		fh.user=FtpSettings::String2WString(user);
		fh.pass=FtpSettings::String2WString(pass);
		fh.home=FtpSettings::String2WString(GetValue(name2val, def_name2val, "Home"));
		fh.port=atoi(GetValue(name2val, def_name2val, "Port", "21").c_str());
		fh.conlim=atoi(GetValue(name2val, def_name2val, "ConnectionsLimit").c_str());

		fh.nv_hostname=FtpSettings::String2WString(GetValue(name2val, def_name2val, "nv_hostname",_nv_hostname.c_str()));
		fh.nv_hostip=FtpSettings::String2WString(GetValue(name2val, def_name2val, "nv_hostip",_nv_hostip.c_str()));
		fh.nv_user=FtpSettings::String2WString(GetValue(name2val, def_name2val, "nv_user",_nv_user.c_str()));
		fh.nv_pass=FtpSettings::String2WString(GetValue(name2val, def_name2val, "nv_pass",_nv_pass.c_str()));
		fh.nv_flags=atoi(GetValue(name2val, def_name2val, "nv_flags",_nv_flags.c_str()).c_str());

		fh.flags=Str2Mode(mode);

		if (stricmp(smode.c_str(),"Implicit")==0)
		{
			fh.smode=SModeImplicit;
		}
		else if (stricmp(smode.c_str(),"ExplicitSSL")==0)
		{
			fh.smode=SModeExplicitSsl;
		}
		else if (stricmp(smode.c_str(),"ExplicitTLS")==0)
		{
			fh.smode=SModeExplicitTls;
		}
		else
		{
			fh.smode=SModeUnsecure;
		}

		if (stricmp(enc.c_str(),"KOI8R")==0)
		{
			fh.enc=EncKOI8R;
		}
		else if (stricmp(enc.c_str(),"KOI8U")==0)
		{
			fh.enc=EncKOI8U;
		}
		else if (stricmp(enc.c_str(),"UTF8")==0)
		{
			fh.enc=EncUTF8;
		}
		else if (stricmp(enc.c_str(),"DOS")==0)
		{
			fh.enc=EncDOS;
		}
		else
		{
			fh.enc=EncWIN;
		}
		
		if (fh.flags)
		{
			tmp_map.insert(HOSTSMAP::value_type(FtpSettings::String2WString(name),fh));
		}
	}
	
	void ParseSingleLineContent(const std::string &line, HOSTSMAP &tmp_map, const STR2STR &def_name2val)
	{
		STR2STR name2val;
        std::string hname(line);
        size_t x=hname.rfind(TCHAR('@'));
        if(x!=std::wstring::npos)
		{
			std::string user=hname.substr(0,x);
			hname.erase(0,x+1);
			size_t y=user.find(TCHAR(':'));
			if(y!=std::wstring::npos)
			{
				name2val["Password"]=user.substr(y+1,user.size()-y-1);
				user.resize(y);
			}
			name2val["Login"]=user;
		}
		name2val["Name"]=hname;
		name2val["Host"]=hname;
		InsertHostEntry(tmp_map, name2val, def_name2val);
	}
	
	void ParseFileContent(const std::string &TmpBuf, HOSTSMAP &tmp_map, const STR2STR &def_name2val)
	{
		STR2STR name2val;
		
		for(DWORD i=0,j=0,e=TmpBuf.size();i<=e;)
		{
			if((i==e)||(TmpBuf[i]=='\r')||(TmpBuf[i]=='\n'))
			{
				bool new_site = false;
				if(i>j)
				{
					std::string line(&TmpBuf[j],i-j);
					size_t p=line.find('=');
					if (p!=std::string::npos)
					{
						std::string name=line.substr(0,p);
						line.erase(0,p+1);
						StrTrim(name);
						StrTrim(line);
						name2val[name]=line;
					}
					else if(line.compare("-"))
					{
						ParseSingleLineContent(line, tmp_map, def_name2val);
					}else
					{
						new_site = true;
					}
				}
				
				if(new_site || i==e)
				{
					if (!name2val.empty())
					{
						InsertHostEntry(tmp_map, name2val, def_name2val);
						name2val.clear();
					}
					if(i==e)break;
				}
				
				while((i<e)&&
					((TmpBuf[i]=='\r')||
					(TmpBuf[i]=='\n')||
					(TmpBuf[i]==' ')||
					(TmpBuf[i]=='\t')))i++;
				j=i;
			}else
			{
				i++;
			}
		}
	}
	
	void  GetFromFile(std::wstring path=std::wstring())
	{
		HOSTSMAP tmp_map;
		if (path.empty())
		{
			path.assign(ModPathName);
			int x=path.rfind(TEXT("\\"));
			if(x!=std::wstring::npos)
			{
				path.resize(x+1);
				path.append(TEXT("FtpServList.txt"));
			}else
			{
				path.resize(0);
			}
		}
		if(!path.empty())
		{
			HANDLE f=CreateFile(path.c_str(),GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
			if(f!=INVALID_HANDLE_VALUE)
			{
				DWORD sz=GetFileSize(f,NULL);
				if(sz&&(sz!=0xffffffff))
				{
					std::string TmpBuf;
					TmpBuf.resize(sz);
					DWORD dw=0;
					ReadFile(f,TmpBuf.begin(),sz,&dw,NULL);
					if(dw==sz)
					{
						STR2STR def_name2val;
						ParseFileContent(TmpBuf,tmp_map,def_name2val);
					}
				}
				CloseHandle(f);
			}
		}
		WaitForSingleObject(_set_mtx,INFINITE);//EnterCriticalSection(&_set_cs);//
		_hosts_map = tmp_map;
		ReleaseMutex(_set_mtx);//LeaveCriticalSection(&_set_cs);//
	}
	
public:
	FtpSitesList():
		_set_mtx(CreateMutex(NULL,FALSE,NULL)),
		_serv_last_ts(0),
		_for_edit(false),
		_from_remote(false),
		_in_get_thread(false),
		_next_get_thread(0)
	{	
		
	}
	
	FtpSitesList(const std::wstring &file):
		_set_mtx(CreateMutex(NULL,FALSE,NULL)),
		_serv_last_ts(0),
		_file(file),
		_for_edit(true),
		_from_remote(false),
		_list_retrieved(false),
		_in_get_thread(false),
		_next_get_thread(0)
	{
			
	}

	FtpSitesList(const std::wstring &file,
		const std::wstring &nv_hostname, 
		const std::wstring &nv_hostip,
		const std::wstring &nv_user,
		const std::wstring &nv_pass,
		const std::wstring &nv_flags):
		_set_mtx(CreateMutex(NULL,FALSE,NULL)),
		_serv_last_ts(0),
		_file(file),
		_for_edit(true),
		_from_remote(false),
		_list_retrieved(false),
		_nv_hostname(FtpSettings::WString2String(nv_hostname)),
		_nv_hostip(FtpSettings::WString2String(nv_hostip)),
		_nv_user(FtpSettings::WString2String(nv_user)),
		_nv_pass(FtpSettings::WString2String(nv_pass)),
		_nv_flags(FtpSettings::WString2String(nv_flags)),
		_in_get_thread(false),
		_next_get_thread(0)
	{
			
	}

	FtpSitesList(bool from_remote):
		_set_mtx(CreateMutex(NULL,FALSE,NULL)),
		_serv_last_ts(0),
		_for_edit(true),
		_from_remote(from_remote),
		_list_retrieved(false),
		_in_get_thread(false),
		_next_get_thread(0)
	{		
	}
		
	virtual ~FtpSitesList()
	{
		::CloseHandle(_set_mtx);
	}
	
	void GetHostListBySetup()
	{
		MyWriteDebugLog("GetHostListBySetup: FtpSettings::set_flags=%x\n",
			FtpSettings::set_flags);

		bool got=false;
		if((!got)&&(FtpSettings::set_flags&FTPS_FROMLOCAL))got=GetFromLocal();
		if((!got)&&(FtpSettings::set_flags&FTPS_FROMREMOTE))
		{
			DWORD ctm=GetTickCount();
			got=((ctm<_serv_last_ts)||((ctm-_serv_last_ts)<(1000*FtpSettings::serv_refresh)));
			if(!got)
			{
				got=GetFromRemote();
				if(got)_serv_last_ts=GetTickCount();
			}
		}
		if(!got)GetFromFile();//else SaveToFile();
	}
	
	static void __cdecl GetHostListThread(void *p)
	{
		FtpSitesList *it=(FtpSitesList *)p;
		it->GetHostListBySetup();
		WaitForSingleObject(it->_set_mtx,INFINITE);
		it->_in_get_thread = false;
		it->_next_get_thread = ::GetTickCount()+5000;
		ReleaseMutex(it->_set_mtx);
	}

	virtual void GetHostList(STRINGVECTOR &hlst)
	{
		WaitForSingleObject(_set_mtx,INFINITE);//EnterCriticalSection(&_set_cs);//

		if (!_for_edit)
		{
			if(!_list_retrieved)
			{
				GetHostListBySetup();
			}
			else if(!_in_get_thread)
			{
				if (::GetTickCount()>=_next_get_thread)
				{
					_in_get_thread = true;
					_beginthread(GetHostListThread,0,this);
				}
			}
		}else if(!_list_retrieved)
		{
			if (!_file.empty())
			{
				GetFromFile(_file);
			}
			else if (_from_remote)
			{
				GetFromRemote();
			}
			else
			{
				GetFromLocal();
			}
		}

		_list_retrieved = true;
		
		for(HOSTSMAP::iterator i=_hosts_map.begin();i!=_hosts_map.end();++i)hlst.push_back(i->first); 
		ReleaseMutex(_set_mtx);//LeaveCriticalSection(&_set_cs);//
	}
	
	virtual void GetHostSetup(
		const std::wstring &hname,
		std::wstring &ip,
		DWORD &port,
		std::wstring &user,
		std::wstring &pass, 
		std::wstring &home,
		DWORD &flags,
		DWORD &conlim,
		ENCCP &enc,
		FTPSMODE &smode)
	{
		STRINGVECTOR hlst;
		if(_hosts_map.empty())GetHostList(hlst);
		WaitForSingleObject(_set_mtx,INFINITE);//EnterCriticalSection(&_set_cs);//
		HOSTSMAP::iterator i=_hosts_map.find(hname);
		if(i!=_hosts_map.end())
		{
			user.assign(i->second.user);
			pass.assign(i->second.pass);
			ip.assign(i->second.host);
			home=i->second.home;
			port=i->second.port;
			flags=i->second.flags;
			conlim=i->second.conlim;
			enc=i->second.enc;
			smode=i->second.smode;
		}else
		{
			user.resize(0);
			pass.resize(0);
			ip.resize(0);
			home.resize(0);
			port=21;
			flags=0;
			conlim=0;
			enc=EncWIN;
			smode=SModeUnsecure;
		}
		
		ReleaseMutex(_set_mtx);//LeaveCriticalSection(&_set_cs);//
//		MyWriteDebugLog("Step 1 hname=%ws ip=%ws _for_edit=%i\n",
//			hname.c_str(),ip.c_str(),(int)_for_edit);

		if (!_for_edit)
		{

			if(user.empty())
			{
				user.assign(TEXT("anonymous"));
				pass.assign(TEXT("FTPDrive@killprog.com"));
			}
			if((!flags)||((flags&0x1400)==0x1400))
			{
				flags&=~0x1400;
				flags|=((FtpSettings::set_flags&FTPS_DEFPASSIVE)?0x1000:0x400);
			}
			
//			MyWriteDebugLog("Step 2 hname=%ws ip=%ws _for_edit=%i\n",
//				hname.c_str(),ip.c_str(),(int)_for_edit);
			
			DWORD dw_ip = ip.empty()?0:FtpSettings::inet_addrW(ip.c_str());
			if (dw_ip==0 || dw_ip==INADDR_NONE)
			{
//				MyWriteDebugLog("Step 3 hname=%ws ip=%ws _for_edit=%i\n",
//					hname.c_str(),ip.c_str(),(int)_for_edit);
				
				if(ip.empty())ip = hname;
				HOSTENT *he=FtpSettings::gethostbynameW(ip.c_str());   
				if(he&&(he->h_addrtype==AF_INET))
				{
//					MyWriteDebugLog("Step 4 hname=%ws ip=%ws _for_edit=%i\n",
//						hname.c_str(),ip.c_str(),(int)_for_edit);
					
					in_addr in;in.S_un.S_addr=*((DWORD *)he->h_addr);
					ip.assign(FtpSettings::Char2WString(inet_ntoa(in)));
					WaitForSingleObject(_set_mtx,INFINITE);//EnterCriticalSection(&_set_cs);//
					HOSTSMAP::iterator i=_hosts_map.find(hname);
					if(i!=_hosts_map.end())i->second.host=ip;
					ReleaseMutex(_set_mtx);//LeaveCriticalSection(&_set_cs);//
				}
/*				else
				{
					MyWriteDebugLog("Step 54 err=%u\n",
						WSAGetLastError());
				}
*/			}
		}
	}
	
	virtual void SetHostSetup(
		const std::wstring &hname,
		const std::wstring &host,
		DWORD port,
		const std::wstring &user,
		const std::wstring &pass, 
		const std::wstring &home,
		DWORD flags,
		DWORD conlim,
		ENCCP enc,
		FTPSMODE smode)
	{
		WaitForSingleObject(_set_mtx,INFINITE);//EnterCriticalSection(&_set_cs);//
		HOSTSMAP::iterator i=_hosts_map.find(hname);
		if(i!=_hosts_map.end())
		{
			i->second.user = user;
			i->second.pass = pass;
			i->second.host = host;
			i->second.home = home;
			i->second.port = port;
			i->second.flags = flags;
			i->second.conlim = conlim;
			i->second.enc = enc;
			i->second.smode = smode;
		}
		else
		{
			FTPHOST fh;
			fh.user = user;
			fh.pass = pass;
			fh.host = host;
			fh.home = home;
			fh.port = port;
			fh.flags = flags;
			fh.conlim = conlim;
			fh.enc = enc;
			fh.smode = smode;
			_hosts_map.insert(HOSTSMAP::value_type(hname,fh));
		}
		ReleaseMutex(_set_mtx);//LeaveCriticalSection(&_set_cs);//			
	}
	
	virtual void DelHost(const std::wstring &hname)
	{
		WaitForSingleObject(_set_mtx,INFINITE);//EnterCriticalSection(&_set_cs);//
		HOSTSMAP::iterator i=_hosts_map.find(hname);
		if(i!=_hosts_map.end())
		{
			_hosts_map.erase(i);
		}
		ReleaseMutex(_set_mtx);//LeaveCriticalSection(&_set_cs);//		
	}

	void WriteValue(HANDLE f, const char *name, const std::wstring &value, const std::wstring &nv_value=std::wstring())
	{
		if (nv_value!=value)
		{
			DWORD dw;
			::WriteFile(f,name,strlen(name),&dw,NULL);
			::WriteFile(f,"=",1,&dw,NULL);
			std::string s=FtpSettings::WString2String(value);
			::WriteFile(f,s.c_str(),s.size(),&dw,NULL);
			::WriteFile(f,"\r\n",2,&dw,NULL);
		}
	}
	
	void WriteEndOfSite(HANDLE f)
	{
		DWORD dw;::WriteFile(f,"\r\n-\r\n",5,&dw,NULL);
	}


	std::string Mode2Str(unsigned int flags)
	{
		flags&=0x1400;
		if(!flags)
			return std::string();

		if(flags==0x400)
			return std::string("Active");

		if(flags==0x1000)
			return std::string("Passive");

		return std::string("Default");
	}

	unsigned int Str2Mode(const std::string &s)
	{
		if(stricmp(s.c_str(),"Active")==0)
			return 0x400;

		if(stricmp(s.c_str(),"Passive")==0)
			return 0x1000;

		return 0x1400;
	}

	virtual bool SaveTo(const std::wstring &file, bool dont_save_hosts)
	{
		HANDLE f=::CreateFile(file.c_str(),GENERIC_READ|GENERIC_WRITE,
			FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);

		if (f==INVALID_HANDLE_VALUE)
			return false;
		
		WaitForSingleObject(_set_mtx,INFINITE);
		for(HOSTSMAP::iterator i=_hosts_map.begin();i!=_hosts_map.end();++i)
		{
			std::wstring	nv_host;
			if(i->second.nv_hostip.empty())
				nv_host=i->second.nv_hostname;
			else
				nv_host=i->second.nv_hostip;


			
			WriteValue(f, "Name", i->first);
			if (!dont_save_hosts)
			{
				WriteValue(f, "Host", i->second.host,nv_host);
			}
			if (i->second.port)
			{
				wchar_t wzPort[64];_itow(i->second.port,wzPort,10);
				WriteValue(f, "Port", wzPort);
			}
			WriteValue(f, "Home", i->second.home);
			WriteValue(f, "Login", i->second.user, i->second.nv_user);
			WriteValue(f, "Password", i->second.pass, i->second.nv_pass);
			if(i->second.conlim)
			{
				wchar_t wzLimit[64];_itow(i->second.conlim,wzLimit,10);
				WriteValue(f, "ConnectionsLimit", wzLimit);
			}
			WriteValue(f, "Mode", 
				FtpSettings::String2WString(Mode2Str(i->second.flags)), 
				FtpSettings::String2WString(Mode2Str(i->second.nv_flags)));
			
			switch(i->second.smode)
			{
			case SModeImplicit:
				WriteValue(f, "Secure", L"Implicit");
				break;
				
			case SModeExplicitSsl:
				WriteValue(f, "Secure", L"ExplicitSSL");
				break;

			case SModeExplicitTls:
				WriteValue(f, "Secure", L"ExplicitTLS");
				break;

			case SModeUnsecure:
			default:
				WriteValue(f, "Secure", L"Disable");

			}
			
			switch(i->second.enc)
			{
			case EncKOI8U:
				WriteValue(f, "Encoding", L"KOI8U");
				break;

			case EncKOI8R:
				WriteValue(f, "Encoding", L"KOI8R");
				break;

			case EncUTF8:
				WriteValue(f, "Encoding", L"UTF8");
				break;
			
			case EncDOS:
				WriteValue(f, "Encoding", L"DOS");
				break;
			
			case EncWIN:
			default:
				WriteValue(f, "Encoding", L"WIN");
			}

			WriteEndOfSite(f);
		}
		::CloseHandle(f);
		ReleaseMutex(_set_mtx);
		return true;
	}

};


namespace FtpSettings
{
	FtpSitesList	_current_sites_list;
	IFtpSitesList *CurrentSitesList()
	{
		return (IFtpSitesList *)&_current_sites_list;
	}

	IFtpSitesList *SitesListFromFile(const std::wstring &file)
	{
		return (IFtpSitesList *)new FtpSitesList(file);
	}

	IFtpSitesList *SitesListFromFile(const std::wstring &file,
		const std::wstring &nv_hostname, 
		const std::wstring &nv_hostip,
		const std::wstring &nv_user,
		const std::wstring &nv_pass,
		const std::wstring &nv_flags)
	{
		return (IFtpSitesList *)new FtpSitesList(file, 
			nv_hostname, nv_hostip, nv_user, nv_pass, nv_flags);
	}


	IFtpSitesList *SitesListFromNV(bool remote)
	{
		return (IFtpSitesList *)new FtpSitesList(remote);

	}
}