
#ifndef _FTP_SETTINGS_H_
#define _FTP_SETTINGS_H_

extern wchar_t ModPathName[MAX_PATH+1];

extern volatile int     log_level;

	
typedef enum ENCCP_{
	EncWIN,
	EncDOS,
	EncUTF8,
	EncKOI8R,
	EncKOI8U
}ENCCP;

typedef enum FTPSMODE_
{
	SModeUnsecure,
	SModeImplicit,
	SModeExplicitSsl,
	SModeExplicitTls
}FTPSMODE;

typedef std::vector<std::wstring>	STRINGVECTOR;
typedef std::vector<char>			CHARVECTOR;

class IFtpSitesList
{
public:
	virtual inline ~IFtpSitesList(){};

	virtual void GetHostList(
		STRINGVECTOR &hlst)=0;

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
		FTPSMODE &smode) = 0;

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
		FTPSMODE smode) = 0;

	virtual void DelHost(const std::wstring &hname) =0;	
	virtual bool SaveTo(const std::wstring &file, bool dont_save_hosts) = 0;
};



namespace FtpSettings
{
IFtpSitesList *CurrentSitesList();
IFtpSitesList *SitesListFromFile(const std::wstring &file);
IFtpSitesList *SitesListFromFile(const std::wstring &file,
								 const std::wstring &nv_hostname, 
								 const std::wstring &nv_hostip,
								 const std::wstring &nv_user,
								 const std::wstring &nv_pass,
								 const std::wstring &nv_flags);
IFtpSitesList *SitesListFromNV(bool remote);
extern char    FtpFsDrive[2];

bool IsThisAppAffected(TCHAR *exepath);
void UpdateSettings();
void Init();

std::string __cdecl StrFormat(const char *format, ...);
std::string WChar2String(const wchar_t *s, size_t l=std::string::npos);
std::wstring Char2WString(const char *s, size_t l=std::wstring::npos);

__inline std::string WString2String(const std::wstring &s)
  {return WChar2String(s.c_str(), s.size());}
__inline std::wstring String2WString(const std::string &s)
  {return Char2WString(s.c_str(), s.size());}

#ifdef _WINSOCKAPI_
__inline DWORD inet_addrW(const wchar_t *s)
  {return inet_addr(WChar2String(s).c_str());}
__inline HOSTENT *gethostbynameW(const wchar_t *s)
  {return gethostbyname(WChar2String(s).c_str());}
#endif
extern DWORD           set_flags;
extern DWORD           adv_flags;
extern DWORD           cache_max_file_size;
extern DWORD           cache_dir_expire;
extern DWORD           cache_file_expire;
extern DWORD           cache_max_file_size;
extern DWORD           pre_seek_delay;
extern DWORD           idle_timeout;
extern DWORD           clean_interval;
extern DWORD           ftp_replies_timeout;
extern DWORD           ftp_data_timeout;
extern DWORD           err_maxretries;
extern DWORD           err_retrydelay;
extern DWORD           serv_last_ts;
extern std::wstring     serv_host;
extern int             serv_port;
extern std::wstring     serv_user;
extern std::wstring     serv_pass;
extern DWORD           serv_refresh;
extern DWORD           hk_unfreeze;

extern std::wstring     interface_lng;
} 
#endif// _FTP_SETTINGS_H_