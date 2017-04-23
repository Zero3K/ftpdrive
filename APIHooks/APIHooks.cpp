// APIHooks.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"
#include "stdio.h"
#include "stdlib.h"
#include "tlhelp32.h"
#include "process.h"
#include "shellapi.h"
#include "../FtpSettings/defs.h"
#include "../FtpFS/ntdefs.h"
#include "hookimpl.h"
#include "detours/src/detours.h"
#include "ftpfswrappers.h"
#include "HookSettings.h"


#pragma data_seg(".share") 
volatile HHOOK hhook=NULL;
volatile DWORD adv_flags=default_adv_flags;
volatile int log_level=0;
volatile char FtpFsDrive[2]={'Z','z'};
#pragma data_seg() 
#pragma comment(linker, "/SECTION:.share,RWS") 

#pragma comment(linker, "/FILEALIGN:512 /SECTION:.text,ERW /IGNORE:4078") 
#pragma comment(linker, "/MERGE:.rdata=.text") 
//#pragma comment(linker, "/MERGE:.data=.text") 

volatile void *hack1=&RegOpenKey;//just to ensure link advapi32.dll
//volatile void *hack2=&ShellExecute;//just to ensure link shell32.dll
DWORD g_my_pid = ::GetCurrentProcessId();
HINSTANCE g_my_hmod=NULL; 

LRESULT CALLBACK GetMsgProc(int code,WPARAM wParam,LPARAM lParam)
{
return CallNextHookEx(hhook,code,wParam,lParam);
}

extern "C" _declspec(dllexport) void __stdcall CallMe(int cmd, DWORD par)
{
switch(cmd)
 {
 case 1:
  if(hhook)
   {
   if(hhook!=(HHOOK)0xffffffff)UnhookWindowsHookEx(hhook);
   hhook=NULL;
   }
  hhook=par?(HHOOK)(0xffffffff):SetWindowsHookEx(WH_GETMESSAGE,GetMsgProc,g_my_hmod,0);  
  break;
 
 case 2:
  if(hhook)
   {
   if(hhook!=(HHOOK)0xffffffff)UnhookWindowsHookEx(hhook);
   hhook=NULL;
   }  
  break;
 
 case 3:
  {
  static std::vector<TCHAR> ModPathName(MAX_PATH+1);
  static bool init_pathname = false;
  if(!init_pathname)
   {
   GetModuleFileName(g_my_hmod,&ModPathName[0],MAX_PATH);
   ModPathName[MAX_PATH]=0;
   init_pathname = true;
   }
  DetourContinueProcessWithDllW((HANDLE)par, &ModPathName[0]);
  }break;
 
 case 4:
  FtpFsDrive[0]=(char)(unsigned char)(par&0xff);
  FtpFsDrive[1]=(char)(unsigned char)((par>>8)&0xff);
  break;
 case 5:
  adv_flags=par;
  break;
 case 6:  
  log_level=par;
  break;
 }
}


/*
extern "C" _declspec(dllexport) void __stdcall init()
{
HANDLE hToken;
TOKEN_PRIVILEGES tkp;
OpenProcessToken(GetCurrentProcess(),TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken);
LookupPrivilegeValue(NULL, SE_DEBUG_NAME,&tkp.Privileges[0].Luid);
tkp.PrivilegeCount = 1;
tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
AdjustTokenPrivileges(hToken, FALSE, &tkp, 0,(PTOKEN_PRIVILEGES)NULL, 0);
CallMe(0,0);
MSG msg;
while(GetMessage(&msg,0,0,0))
{
DispatchMessage(&msg);
TranslateMessage(&msg);
}
}
*/

void ProcessCurDir()
 {
 HKEY key=NULL;
 ::RegOpenKey(HKEY_CURRENT_USER,TEXT("Software\\KillSoft\\FtpDrive\\StartupCurDir"),&key);
 if(key)
  {
  TCHAR val[32];
  _itow(g_my_pid,val,16); 
  DWORD sz=1024;
  std::vector<TCHAR> tmp((sz/sizeof(TCHAR))+1);
  DWORD tip=REG_SZ;
  if (::RegQueryValueEx(key,val,NULL,&tip,(LPBYTE)&tmp[0],&sz)==ERROR_SUCCESS)
   {
   tmp[sz/sizeof(TCHAR)]=0;
   ::SetCurrentDirectory(&tmp[0]);
   ::RegDeleteValue(key,val);
   }
  ::RegCloseKey(key);
  }
 }


BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved
					 )
{
	if (ul_reason_for_call==DLL_PROCESS_ATTACH)
		g_my_hmod = (HINSTANCE)hModule; 

	if (HookImpl::IsThisFtpDrive() //hang
	|| HookImpl::IsThisAVP() //crash
	|| HookImpl::IsThisWinDbg())//debug
		return TRUE;
	
	switch (ul_reason_for_call)
	{ 
	case DLL_PROCESS_ATTACH:
		if (!FtpFsWrappers::ProcessAttach())
			return FALSE;
		//ensure we will not be unloaded on unhook
		//this can cause hooked app to crash
		::LoadLibrary(TEXT("FtpDrive.dll"));
		FtpFsWrappers::ThreadAttach();
		HookImpl::InitApiHooks();  
		ProcessCurDir();  
		break;
		
	case DLL_PROCESS_DETACH:
		::SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
		HookImpl::DeinitApiHooks(); 
		FtpFsWrappers::ProcessDetach();
		break; 
		
	case DLL_THREAD_ATTACH:  
		FtpFsWrappers::ThreadAttach();
		break;

	case DLL_THREAD_DETACH:
		FtpFsWrappers::ThreadDetach();  
		break;
	}
	
	return TRUE;
}

