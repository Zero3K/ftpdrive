#include "stdafx.h"
#include "../FtpFS/ntdefs.h"
#include "../AnyFS/AnyFS.h"
#include "shellapi.h"
#include "ftpfswrappers.h"
#include "hookimpl.h"
#include "detours/src/detours.h"
#include "HookSettings.h"
#ifdef DBG_LOG
#include "../DebugLog/DebugLog.h"
#endif

//#define WNET_PATH	L"\\\\localhost\\ipc$"

namespace HookImpl
{
typedef HMODULE (__stdcall *realLoadLibraryA_t)(char *);
typedef HMODULE (__stdcall *realLoadLibraryW_t)(wchar_t *);
typedef HMODULE (__stdcall *realLoadLibraryExA_t)(char *,HANDLE,DWORD);
typedef HMODULE (__stdcall *realLoadLibraryExW_t)(wchar_t *,HANDLE,DWORD);
typedef BOOL (__stdcall *realFreeLibrary_t)(HMODULE);
//CheckForDriveChanged();
void *realCreateProcessA;
void *realCreateProcessW;
void *realCreateProcessAsUserA;
void *realCreateProcessAsUserW;
void *realShellExecuteA;
void *realShellExecuteW;
void *realShellExecuteExA;
void *realShellExecuteExW;
void *realSHIsFileAvailableOffline;

//void *realWNetGetConnectionA;
void *realWNetGetConnectionW;
void *realWNetGetConnection3W;

realLoadLibraryA_t realLoadLibraryA;
realLoadLibraryW_t realLoadLibraryW;
realLoadLibraryExA_t realLoadLibraryExA;
realLoadLibraryExW_t realLoadLibraryExW;


void StorePidCurDir(unsigned int pid, const TCHAR *curdir)
 {
 HKEY key=NULL;
 RegCreateKey(HKEY_CURRENT_USER,TEXT("Software\\KillSoft\\FtpDrive\\StartupCurDir"),&key);
 if(key)
  {
  TCHAR val[32];_itow(pid,val,16); 
  if(curdir)
   {
   if(HookSettings::IsOurRootDrive(curdir))RegSetValueEx(key,val,NULL,REG_SZ,(LPBYTE)curdir,(wcslen(curdir)+1)*sizeof(TCHAR));
   }else
   {
   TCHAR tmp[MAX_PATH+2];tmp[MAX_PATH+1]=0;
   GetCurrentDirectory(MAX_PATH+1,tmp);
   if(HookSettings::IsOurRootDrive(tmp))RegSetValueEx(key,val,NULL,REG_SZ,(LPBYTE)tmp,(wcslen(tmp)+1)*sizeof(TCHAR));
   }
  RegCloseKey(key);
  }
 }

void StorePidCurDir(unsigned int pid, const char *curdir)
 { 
 if(curdir)
  {
  std::vector<TCHAR> tmp(strlen(curdir)+1);
  MultiByteToWideChar(CP_ACP,0,curdir,-1,&tmp[0],tmp.size());
  StorePidCurDir(pid, &tmp[0]);
  }else StorePidCurDir(pid, (TCHAR *)NULL);
 }

char *InspectCurDir(char *curdir,char *tmpbuf)
 {
 if(!curdir)
  {
  GetCurrentDirectoryA(MAX_PATH,tmpbuf);
  tmpbuf[MAX_PATH-1]=0;
  if( !HookSettings::IsOurRootDrive(tmpbuf))return NULL;
  }

 if((!curdir) || HookSettings::IsOurRootDrive(curdir))
  {
  GetWindowsDirectoryA(tmpbuf,MAX_PATH);
  return tmpbuf;
  }
 return curdir;
 }

wchar_t *InspectCurDir(wchar_t *curdir,wchar_t *tmpbuf)
 {
 if(!curdir)
  {
  GetCurrentDirectoryW(MAX_PATH,tmpbuf);
  tmpbuf[MAX_PATH-1]=0;
  if( !HookSettings::IsOurRootDrive(tmpbuf))return NULL;
  }

 if((!curdir) || HookSettings::IsOurRootDrive(curdir))
  {
  GetWindowsDirectoryW(tmpbuf,MAX_PATH);
  return tmpbuf;
  }
 return curdir;
 }

static volatile bool in_create_process = false;

template<class T, class E, class F>
BOOL myCreateProcess(T *lpApplicationName,T *lpCommandLine,LPSECURITY_ATTRIBUTES lpProcessAttributes,LPSECURITY_ATTRIBUTES lpThreadAttributes,BOOL bInheritHandles,DWORD dwCreationFlags,LPVOID lpEnvironment,T *lpCurrentDirectory,E *lpStartupInfo,LPPROCESS_INFORMATION lpProcessInformation, void *realFn, void *extGetModuleFileName,HANDLE hToken=NULL,bool AsUser=false)
{
if((!hhook)||in_create_process)
 return ((F)realFn)(lpApplicationName,lpCommandLine,lpProcessAttributes,lpThreadAttributes,bInheritHandles,dwCreationFlags,lpEnvironment,lpCurrentDirectory,lpStartupInfo,lpProcessInformation); 

int lerr=GetLastError();
std::vector<T> dllpath(MAX_PATH+1);
((DWORD(__stdcall*)(HMODULE,T *,DWORD))extGetModuleFileName)(g_my_hmod,&dllpath[0],MAX_PATH);
dllpath[MAX_PATH]=0;

std::vector<T> tmpbuf(MAX_PATH);
T *inspected_cur_dir=InspectCurDir(lpCurrentDirectory,&tmpbuf[0]);

in_create_process = true;
BOOL out=DetourCreateProcessWithDll(
lpApplicationName,lpCommandLine,lpProcessAttributes,
lpThreadAttributes,bInheritHandles,dwCreationFlags|CREATE_SUSPENDED,
lpEnvironment,inspected_cur_dir,lpStartupInfo,
lpProcessInformation,&dllpath[0],(F)realFn,NULL,NULL);
in_create_process = false;

if(out)
 {
 StorePidCurDir(lpProcessInformation->dwProcessId,lpCurrentDirectory);
 if(!(dwCreationFlags&CREATE_SUSPENDED))ResumeThread(lpProcessInformation->hThread);
 SetLastError(lerr);
 }

return out;
}

template<class T, class E, class F>
BOOL myCreateProcessAsUser(HANDLE hToken,T *lpApplicationName,T *lpCommandLine,LPSECURITY_ATTRIBUTES lpProcessAttributes,LPSECURITY_ATTRIBUTES lpThreadAttributes,BOOL bInheritHandles,DWORD dwCreationFlags,LPVOID lpEnvironment,T *lpCurrentDirectory,E *lpStartupInfo,LPPROCESS_INFORMATION lpProcessInformation, void *realFn, void *extGetModuleFileName)
{
if((!hhook)||in_create_process)
 return ((F)realFn)(hToken,lpApplicationName,lpCommandLine,lpProcessAttributes,lpThreadAttributes,bInheritHandles,dwCreationFlags,lpEnvironment,lpCurrentDirectory,lpStartupInfo,lpProcessInformation);

int lerr=GetLastError();
std::vector<T> dllpath(MAX_PATH+1);
((DWORD(__stdcall*)(HMODULE,T *,DWORD))extGetModuleFileName)(g_my_hmod,&dllpath[0],MAX_PATH);
dllpath[MAX_PATH]=0;

std::vector<T> tmpbuf(MAX_PATH);
T *inspected_cur_dir=InspectCurDir(lpCurrentDirectory,&tmpbuf[0]);

in_create_process = true;
BOOL out=DetourCreateProcessWithDll(
lpApplicationName,lpCommandLine,lpProcessAttributes,
lpThreadAttributes,bInheritHandles,dwCreationFlags|CREATE_SUSPENDED,
lpEnvironment,inspected_cur_dir,lpStartupInfo,
lpProcessInformation,&dllpath[0],NULL,hToken,(F)realFn);
in_create_process = false;

if(out)
 {
 StorePidCurDir(lpProcessInformation->dwProcessId,lpCurrentDirectory);
 if(!(dwCreationFlags&CREATE_SUSPENDED))ResumeThread(lpProcessInformation->hThread);
 SetLastError(lerr);
 }
return out;
}

BOOL WINAPI myCreateProcessA(char *lpApplicationName,char *lpCommandLine,LPSECURITY_ATTRIBUTES lpProcessAttributes,LPSECURITY_ATTRIBUTES lpThreadAttributes,BOOL bInheritHandles,DWORD dwCreationFlags,LPVOID lpEnvironment,char *lpCurrentDirectory,LPSTARTUPINFOA lpStartupInfo,LPPROCESS_INFORMATION lpProcessInformation)
{
return myCreateProcess<char,STARTUPINFOA,PDETOUR_CREATE_PROCESS_ROUTINEA>(lpApplicationName,lpCommandLine,lpProcessAttributes,lpThreadAttributes,bInheritHandles,dwCreationFlags,lpEnvironment,lpCurrentDirectory,lpStartupInfo,lpProcessInformation,realCreateProcessA,GetModuleFileNameA);
}

BOOL WINAPI myCreateProcessW(wchar_t *lpApplicationName,wchar_t *lpCommandLine,LPSECURITY_ATTRIBUTES lpProcessAttributes,LPSECURITY_ATTRIBUTES lpThreadAttributes,BOOL bInheritHandles,DWORD dwCreationFlags,LPVOID lpEnvironment,wchar_t *lpCurrentDirectory,LPSTARTUPINFOW lpStartupInfo,LPPROCESS_INFORMATION lpProcessInformation)
{
return myCreateProcess<wchar_t,STARTUPINFOW,PDETOUR_CREATE_PROCESS_ROUTINEW>(lpApplicationName,lpCommandLine,lpProcessAttributes,lpThreadAttributes,bInheritHandles,dwCreationFlags,lpEnvironment,lpCurrentDirectory,lpStartupInfo,lpProcessInformation,realCreateProcessW,GetModuleFileNameW);
}

BOOL WINAPI myCreateProcessAsUserA(HANDLE hToken,char *lpApplicationName,char *lpCommandLine,LPSECURITY_ATTRIBUTES lpProcessAttributes,LPSECURITY_ATTRIBUTES lpThreadAttributes,BOOL bInheritHandles,DWORD dwCreationFlags,LPVOID lpEnvironment,char *lpCurrentDirectory,LPSTARTUPINFOA lpStartupInfo,LPPROCESS_INFORMATION lpProcessInformation)
{
return myCreateProcessAsUser<char,STARTUPINFOA,PDETOUR_CREATE_PROCESSLOGON_ROUTINEA>(hToken,lpApplicationName,lpCommandLine,lpProcessAttributes,lpThreadAttributes,bInheritHandles,dwCreationFlags,lpEnvironment,lpCurrentDirectory,lpStartupInfo,lpProcessInformation,realCreateProcessAsUserA,GetModuleFileNameA);
}

BOOL WINAPI myCreateProcessAsUserW(HANDLE hToken,wchar_t *lpApplicationName,wchar_t *lpCommandLine,LPSECURITY_ATTRIBUTES lpProcessAttributes,LPSECURITY_ATTRIBUTES lpThreadAttributes,BOOL bInheritHandles,DWORD dwCreationFlags,LPVOID lpEnvironment,wchar_t *lpCurrentDirectory,LPSTARTUPINFOW lpStartupInfo,LPPROCESS_INFORMATION lpProcessInformation)
{
return myCreateProcessAsUser<wchar_t,STARTUPINFOW,PDETOUR_CREATE_PROCESSLOGON_ROUTINEW>(hToken,lpApplicationName,lpCommandLine,lpProcessAttributes,lpThreadAttributes,bInheritHandles,dwCreationFlags,lpEnvironment,lpCurrentDirectory,lpStartupInfo,lpProcessInformation,realCreateProcessAsUserW,GetModuleFileNameW);
}

bool EnforceServerByPath(const wchar_t *path)
{
	ANYFS_PATHENFORCE_REQUEST req;
	ANYFS_PATHENFORCE_REPLY reply; 
	req.rawhdr.code=ANYFS_CMD_PATHENFORCE;
	req.rawhdr.pid=g_my_pid;
	wcsncpy(req.path,path,ANYFS_MAX_PATH);
	if(FtpFsWrappers::SendRecvFS(&req,sizeof(req), &reply,sizeof(reply)))     
	{
		if(reply.win32_error_code)::SetLastError(reply.win32_error_code);
		return (reply.win32_error_code==0);
	}
	::SetLastError(ERROR_BROKEN_PIPE);
	return false;
}

const char * InspectShellExecPath(const char *path,char *buff,int len)
{
if((!(GetKeyState(VK_CONTROL)&32768))||
   (!HookSettings::IsThisAppAffected())||(!hhook)||
   (!path)||(len<6)||memicmp(path,"ftp://",6))return path;

buff[0]=FtpFsDrive[0];
buff[1]=':';buff[2]='\\';
memcpy(&buff[3],path+6,len-5);
for(char *t=buff;*t;t++)if(*t=='/')*t='\\';
std::vector<TCHAR> wbuff(ANYFS_MAX_PATH);
if(::MultiByteToWideChar(CP_ACP,0,&buff[0],-1,&wbuff[0],wbuff.size()))
{
	wbuff[wbuff.size()-1]=0;
	EnforceServerByPath(&wbuff[0]);
}
return &buff[0];
}
const wchar_t *InspectShellExecPath(const wchar_t *path,wchar_t *buff,int len)
{
if((!(GetKeyState(VK_CONTROL)&32768))||
   (!HookSettings::IsThisAppAffected())||(!hhook)||
   (!path)||(len<6)||memicmp(path,_L("ftp://"),12))return path;

buff[0]=FtpFsDrive[0];
buff[1]=':';buff[2]='\\';
memcpy(&buff[3],path+6,2*(len-5));
for(wchar_t *t=buff;*t;t++)if(*t=='/')*t='\\';
EnforceServerByPath(&buff[0]);
return &buff[0];
}

HINSTANCE WINAPI myShellExecuteA(HWND hwnd,const char *lpOperation,const char *lpFile,const char *lpParameters,const char *lpDirectory,int nShowCmd)
{
int len=lpFile?strlen(lpFile):0;
std::vector<char> buff(1+len);
lpFile=InspectShellExecPath(lpFile,&buff[0],len);
return ((HINSTANCE(__stdcall*)(HWND,const char *,const char *,const char *,const char *,int))realShellExecuteA)(hwnd,lpOperation,lpFile,lpParameters,lpDirectory,nShowCmd);
}
HINSTANCE WINAPI myShellExecuteW(HWND hwnd,const wchar_t *lpOperation,const wchar_t *lpFile,const wchar_t *lpParameters,const wchar_t *lpDirectory,int nShowCmd)
{
int len=lpFile?wcslen(lpFile):0;
std::vector<wchar_t> buff(1+len);
lpFile=InspectShellExecPath(lpFile,&buff[0],len);
return ((HINSTANCE(__stdcall*)(HWND,const wchar_t *,const wchar_t *,const wchar_t *,const wchar_t *,int))realShellExecuteW)(hwnd,lpOperation,lpFile,lpParameters,lpDirectory,nShowCmd);
}

BOOL WINAPI myShellExecuteExA(LPSHELLEXECUTEINFOA lpExecInfo)
{
int len=lpExecInfo->lpFile?strlen(lpExecInfo->lpFile):0;
std::vector<char> buff(1+len);
lpExecInfo->lpFile=InspectShellExecPath(lpExecInfo->lpFile,&buff[0],len);
return ((BOOL(__stdcall*)(LPSHELLEXECUTEINFOA))realShellExecuteExA)(lpExecInfo);
}
BOOL WINAPI myShellExecuteExW(LPSHELLEXECUTEINFOW lpExecInfo)
{
int len=lpExecInfo->lpFile?wcslen(lpExecInfo->lpFile):0;
std::vector<wchar_t> buff(1+len);
lpExecInfo->lpFile=InspectShellExecPath(lpExecInfo->lpFile,&buff[0],len);
return ((BOOL(__stdcall*)(LPSHELLEXECUTEINFOW))realShellExecuteExW)(lpExecInfo);
}
HRESULT WINAPI mySHIsFileAvailableOffline(LPCWSTR pszPath,LPDWORD pdwStatus)
 {
 *pdwStatus=0x0002;
 return S_OK;
 }


///////////////////////////////////////

/*
DWORD __stdcall myWNetGetConnectionW(
  LPCWSTR lpLocalName,
  LPWSTR lpRemoteName,
  LPDWORD lpnLength
)
{
	if (HookSettings::IsOurRootDrive(lpLocalName) 
	&& HookSettings::IsThisAppAffected())
	{
		if (*lpnLength<(sizeof(WNET_PATH)/sizeof(WCHAR)))
		{
			*lpnLength=((sizeof(WNET_PATH)/sizeof(WCHAR))+1);
			return ERROR_MORE_DATA;
		}
		wcscpy(lpRemoteName,WNET_PATH);
		return NO_ERROR;
	}			

	return ((DWORD(__stdcall*)(LPCWSTR,LPWSTR,LPDWORD))realWNetGetConnectionW)(lpLocalName,lpRemoteName,lpnLength);
}

DWORD __stdcall myWNetGetConnection3W(
  LPCWSTR lpLocalName,
  LPCWSTR lpProviderName,
  DWORD dwLevel,
  LPVOID lpBuffer,
  LPDWORD lpBufferSize
)
{
	if (HookSettings::IsOurRootDrive(lpLocalName) 
	&& HookSettings::IsThisAppAffected())
	{
		if(*lpBufferSize==sizeof(DWORD))
		{
			*((PDWORD)lpBuffer)=0;
			return WN_SUCCESS;
		}
	}			

	return ((DWORD(__stdcall*)(LPCWSTR,LPCWSTR,DWORD,LPVOID,LPDWORD))realWNetGetConnection3W)
		(lpLocalName,lpProviderName,dwLevel,lpBuffer,lpBufferSize);
}
*/
static HMODULE m_sh=NULL;
//static HMODULE m_mpr=NULL;
void PreLoadLibrary()
{
	if (m_sh && !GetModuleHandle(TEXT("shell32.dll")))
		m_sh=NULL;
	/*
	if (m_mpr && !GetModuleHandle(TEXT("mpr.dll")))
		m_mpr=NULL;
	*/
}

void CheckShell32DllLoaded()
{
	if( m_sh)
		return;
	
	LastErrorSaver error_saver;

	m_sh=GetModuleHandle(TEXT("shell32.dll"));
	if (!m_sh)
		return;
	
	realShellExecuteA = DetourByName(m_sh,"ShellExecuteA",(PBYTE)&myShellExecuteA);
	realShellExecuteW = DetourByName(m_sh,"ShellExecuteW",(PBYTE)&myShellExecuteW);
	realShellExecuteExA = DetourByName(m_sh,"ShellExecuteExA",(PBYTE)&myShellExecuteExA);
	realShellExecuteExW = DetourByName(m_sh,"ShellExecuteExW",(PBYTE)&myShellExecuteExW);
	
	//realSHIsFileAvailableOffline = DetourByName(m_sh,"SHIsFileAvailableOffline",(PBYTE)&mySHIsFileAvailableOffline);
	
}

/*
void CheckMprDllLoaded()
{
	if (m_mpr)
		return;

	LastErrorSaver error_saver;
	m_mpr=GetModuleHandle(TEXT("mpr.dll"));
	if (!m_mpr)
		return;
	
	realWNetGetConnectionW = DetourByName(m_mpr,"WNetGetConnectionW",(PBYTE)&myWNetGetConnectionW);  		  
	realWNetGetConnection3W = DetourByName(m_mpr,"WNetGetConnection3W",(PBYTE)&myWNetGetConnection3W);  
}
*/

HMODULE WINAPI myLoadLibraryA(char *lpFileName)
{
	PreLoadLibrary();
	HMODULE out=realLoadLibraryA(lpFileName);
	CheckShell32DllLoaded();
	//CheckMprDllLoaded();
	return out;
}
HMODULE WINAPI myLoadLibraryW(wchar_t *lpFileName)
{
	PreLoadLibrary();
	HMODULE out=realLoadLibraryW(lpFileName);
	CheckShell32DllLoaded();
	//CheckMprDllLoaded();
	return out;
}
/////////////////////////////////////////////////
HMODULE WINAPI myLoadLibraryExA(char *lpFileName,HANDLE hFile,DWORD dwFlags)
{
	PreLoadLibrary();
	HMODULE out=realLoadLibraryExA(lpFileName,hFile,dwFlags);
	CheckShell32DllLoaded();
	//CheckMprDllLoaded();
	return out;
}
HMODULE WINAPI myLoadLibraryExW(wchar_t *lpFileName,HANDLE hFile,DWORD dwFlags)
{
	PreLoadLibrary();
	HMODULE out=realLoadLibraryExW(lpFileName,hFile,dwFlags);
	CheckShell32DllLoaded();
	//CheckMprDllLoaded();
	return out;
}
/////////////////////////////////////////////////


void  HiLevelHook()
 {

 realCreateProcessA = DetourByPtr((PBYTE)&CreateProcessA,(PBYTE)&myCreateProcessA);  
 realCreateProcessW = DetourByPtr((PBYTE)&CreateProcessW,(PBYTE)&myCreateProcessW);  
 realLoadLibraryA = (realLoadLibraryA_t)DetourByPtr((PBYTE)&LoadLibraryA,(PBYTE)&myLoadLibraryA);  
 realLoadLibraryW = (realLoadLibraryW_t)DetourByPtr((PBYTE)&LoadLibraryW,(PBYTE)&myLoadLibraryW);  
 realLoadLibraryExA = (realLoadLibraryExA_t)DetourByPtr((PBYTE)&LoadLibraryExA,(PBYTE)&myLoadLibraryExA);  
 realLoadLibraryExW = (realLoadLibraryExW_t)DetourByPtr((PBYTE)&LoadLibraryExW,(PBYTE)&myLoadLibraryExW);  
 realCreateProcessAsUserA = DetourByPtr((PBYTE)&CreateProcessAsUserA,(PBYTE)&myCreateProcessAsUserA);  
 realCreateProcessAsUserW = DetourByPtr((PBYTE)&CreateProcessAsUserW,(PBYTE)&myCreateProcessAsUserW);  

 CheckShell32DllLoaded();
 //CheckMprDllLoaded();
 }


}

