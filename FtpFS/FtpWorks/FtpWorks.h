#ifndef _FTPDRIVE_H_
#define _FTPDRIVE_H_
#include "windows.h"
#include "wininet.h"
#include <SET>
#include <VECTOR>
#include <MAP>
#include <STRING>
#include "../../AnyFS/SomeUsefull.h"
#include "../UNCSupport.h"
#include "../FtpSocks.h"
#include "../NetControl.h"
#include "../../FtpSettings/settings.h"
#include "../../FtpFS/ntdefs.h"
#include "../../APIHooks/ntapi.h"
#include <boost/shared_ptr.hpp>

#define FTPFS_BIG_TIMEOUT              0x493e0//300 seconds
#define FTPFS_SMALL_TIMEOUT            0xea60//60 seconds
#define FTPFS_DATA_CACHE_SIZE          0x100000

#define DEFAULT_CONNECT_DELAY   0xc8//tipical for most ftp servers
#define DEFAULT_TRANSFER_SPEED  0x7a1200//tipical for 100mbit networks

extern char FtpFsDrive[2];
extern HINSTANCE hmod;
typedef std::vector<HANDLE>             HANDLEVECTOR;
typedef std::vector<std::wstring>        STRINGVECTOR;
typedef std::set<std::wstring,wstring_less_no_case>              ICSTRINGSET;

namespace FtpWorks
{

	class FtpFindData
	{
	public:
		inline FtpFindData():
			dwFileAttributes(0),
			nFileSizeHigh(0),
			nFileSizeLow(0)
		{
		}

		inline FtpFindData(const WIN32_FIND_DATAA &data, CPConv *conv):
			dwFileAttributes(data.dwFileAttributes),
			ftCreationTime(data.ftCreationTime),
			ftLastAccessTime(data.ftLastAccessTime),
			ftLastWriteTime(data.ftLastWriteTime),
			nFileSizeHigh(data.nFileSizeHigh),
			nFileSizeLow(data.nFileSizeLow)
		{
			sFileName = conv->Decode(data.cFileName);
			
			if (sFileName.size()>=MAX_PATH)
				sFileName.resize(MAX_PATH-1);
		}

		DWORD dwFileAttributes;
		FILETIME ftCreationTime;
		FILETIME ftLastAccessTime;
		FILETIME ftLastWriteTime;
		DWORD nFileSizeHigh;
		DWORD nFileSizeLow;
		std::wstring sFileName;
	};

	typedef boost::shared_ptr<FtpFindData>		FtpFindDataPtr;



typedef std::vector<FtpFindDataPtr>    FindVector;
typedef std::vector<char>    CHARVECTOR;

extern char our_cur_dir[MAX_PATH+1];
extern std::wstring tempdir;
//extern TrafficCounter ftp_cache_counter;

class FtpFile
{
public:
	inline FtpFile(
		const std::wstring &serv_, 
		const std::wstring &path_, 
		__int64	size_, 
		DWORD	attr_,
		const FILETIME &crtime_,
		const FILETIME &wrtime_,
		const FILETIME &actime_,
		bool	no_cache_write_
		):
		serv(serv_), 
		path(path_), 
		size(size_), 
		attr(attr_),
		crtime(crtime_),
		wrtime(wrtime_),
		actime(actime_),
		no_cache_write(no_cache_write_),
		ip(0),
		pos(0),
		dir_enumerated(false),
		changed(false),
		DataConnectDelay(DEFAULT_CONNECT_DELAY),
		DataTransferSpeed(DEFAULT_TRANSFER_SPEED),
		pasv_mode(false),
		busy_cnt(0),
		conlim(0),
		SuspendAt(0),
		conv(NULL),
		smode(SModeUnsecure),
		enc(EncWIN)
	{
		
	}

	inline virtual ~FtpFile()
	{
		
	}

	std::wstring MakeFilePathName(bool with_backslash);
	std::wstring MakeFileCurDir(bool with_backslash);

	std::wstring serv;
	std::wstring path;
	std::wstring user;
	std::wstring pass;
	std::wstring home;
	unsigned int port;
	std::wstring curdir;
	__int64 size;
	__int64 pos; 
	
	DWORD attr;
	FILETIME crtime;
	FILETIME wrtime;
	FILETIME actime;
	IFtpSocketPtr sc_cmd;
	IFtpSocketPtr sc_data;
	bool pasv_mode;
	
	FindVector dir_children;
	bool       dir_enumerated;
	bool       no_cache_write;
	bool       changed;
	
	DWORD ip;
	
	FastCriticalSection cs;
	int busy_cnt;
	DWORD conlim;
	DWORD SuspendAt;
	DWORD DataConnectDelay;//msec
	DWORD DataTransferSpeed;//bytes per second
	CPConv *conv;
	FTPSMODE smode;
	ENCCP enc;
};

typedef boost::shared_ptr<FtpFile>		FtpFilePtr;


class CmdBuf
{
private:
	char _buf[256];

public:
	static bool send(FtpFilePtr &file, IFtpSocketPtr &sock, const char *str, int str_len = -1);
	static inline bool send(FtpFilePtr &file, const char *str, int str_len = -1)
	{
		return send(file, file->sc_cmd, str, str_len);
	}
	bool __cdecl sendf(FtpFilePtr &file, IFtpSocketPtr &sock, const char *format, ...);
	bool __cdecl sendf(FtpFilePtr &file, const char *format, ...);
};

inline void SplitPath(const std::wstring &s, STRINGVECTOR &v)
  {
  for(size_t i=0,j=0,k=s.size();i<k;i++)
   {
   if((s[i]=='/')||(s[i]=='\\'))
    {
    if(i>j)v.push_back(s.substr(j,i-j));
    j=i+1;
    }
   }
  }
 /*
void SplitPath(const T &s, std::vector<T> &v)
  {
  for(size_t i=0,j=0,k=s.size();i<k;i++)
   {
   if((s[i]=='/')||(s[i]=='\\'))
    {
    if(i>j)v.push_back(s.substr(j,i-j));
    j=i+1;
    }
   }
  }
*/
//////////////////////////////////////////////////////////////////////////////////
void DelDirTree(const std::wstring &path);

DWORD RecvResponse(FtpFilePtr file,IFtpSocketPtr sc);
bool rawFtpEnum(HANDLE hFile, FtpFilePtr file, std::vector<char> &buf);//assume that command connection ready!
__int64 rawGetSize(HANDLE hFile, FtpFilePtr file, const std::wstring &fname);//assume that command connection ready!
bool IsSymbolicLinkDir(const std::wstring &path);

NTSTATUS xxxOpenFile(const std::wstring &strFileName,HANDLE &out,DWORD dwDesiredAccess,DWORD dwCreateDisposition,DWORD CreateOptions, bool RecursedDirDontCheckPath = false);
NTSTATUS xxxCloseHandle(HANDLE h);

void xxxGetFileInfo(HANDLE hFile, ANYFS_INFO_REPLY &info);
void xxxPrepareTempFile(HANDLE hFile, unsigned int max_size, ANYFS_TEMP_REPLY &reply); 

NTSTATUS xxxReadFile(HANDLE hFile,LPVOID lpBuffer,PLARGE_INTEGER liPointer,DWORD nNumberOfBytesToRead,LPDWORD lpNumberOfBytesRead);
NTSTATUS xxxWriteFile(HANDLE hFile,LPVOID lpBuffer,PLARGE_INTEGER liPointer, DWORD nNumberOfBytesToProcess,LPDWORD lpNumberOfBytesReady);
NTSTATUS xxxRenameFile(HANDLE hFile,const std::wstring &strNewName);
NTSTATUS xxxGetDirChild(HANDLE hFile, unsigned int index, unsigned int pid, FtpFindDataPtr &wfd);
unsigned int xxxPathEnforce(const std::wstring &strPath);
void ForceCleanCaches();
void InitFtpWorks();
void InaccessibleClean();
//////////////////////////////////////////////////////////////////////////////////
std::string FormatIPPort(DWORD ip, unsigned int port);

IFtpSocketPtr OpenFtpConnection(FtpFilePtr file);

void CloseCleanFtpConnectionsCache(FtpFilePtr file = FtpFilePtr(), 
								   bool forced = false);

void FtpEnumRefreshFile(HANDLE hFile, FtpFilePtr file);

void AddFileToDirCache(FtpFilePtr file, const FtpFindData &wfd);
void CleanFileFromDirCache(FtpFilePtr file);
void CleanEnumCache(bool forced = false);
bool FtpEnum(const std::wstring &strFileName, FindVector &out);
bool FtpEnum(HANDLE hFile, FtpFilePtr file, FindVector &out);
HANDLE AllocateHandle();
void DeAllocateHandle(HANDLE h);
HANDLE PutOurFile(FtpFilePtr file);
bool CloseOurFile(HANDLE hFile);
bool SendCwd(FtpFilePtr file,const char *dir);
//bool ApplyFileSettings(HANDLE hFile,FtpFilePtr file);
void SuspendNotActiveFiles();
void ForceSuspendNotActiveFilesForIP(DWORD ip, DWORD port);
bool EnsureCommandConnection(HANDLE hFile,FtpFilePtr file);
bool ChangeFtpDirectory(HANDLE hFile, FtpFilePtr file,const std::wstring &dir);
void CloseDataSocket(FtpFilePtr file);

//////////////////////////////////////////////////////////////////////////////////
std::wstring ExtractFilePath(const std::wstring &strPathName, bool with_slash=true);
std::wstring ExtractFileName(const std::wstring &strPathName);


void CleanFileFromDataCache(FtpFilePtr file);

unsigned int ReadFromCache(FtpFilePtr file,LPVOID lpBuffer,const LARGE_INTEGER *iPointer, DWORD nNumberOfBytesToRead);
void WriteToCache(FtpFilePtr file,LPCVOID lpBuffer,const LARGE_INTEGER *iPointer, DWORD nNumberOfBytesToWrite);
void CleanDataCache(bool forced = false);

class FileLocker
 {
 private:
 HANDLE _hFile;
 public:
 FileLocker(HANDLE hFile,FtpFilePtr &file);
 ~FileLocker();
 bool LockSucceed;
 };
}
#endif