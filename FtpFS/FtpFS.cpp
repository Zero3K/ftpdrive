// FtpFS.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "shlwapi.h"
#include "TrafficCounter.h"
#include "FtpFS.h"
#include "../AnyFS/AnyFS.h"
#include "../FtpFS/FtpWorks/FtpWorks.h"
#include "../FtpFS/ntdefs.h"
#include "../APIHooks/ntapi.h"

#include <string>
#include <process.h> 
#include "../FtpSettings/settings.h"
#include "UI/Dialogs.h"
#pragma comment(lib,"shlwapi.lib")
#pragma comment(linker, "/FILEALIGN:512 /SECTION:.text,ERW /IGNORE:4078") 
//#pragma comment(linker, "/MERGE:.rdata=.text") 
//#pragma comment(linker, "/MERGE:.data=.text") 

HINSTANCE hmod=NULL; 
TCHAR ModPathName[MAX_PATH+1];

std::wstring GetSessionObjectName(PWCHAR name)
 {
 typedef BOOL (__stdcall *tProcessIdToSessionId)(DWORD,PDWORD);
 static tProcessIdToSessionId pProcessIdToSessionId=
  (tProcessIdToSessionId)::GetProcAddress(::GetModuleHandle(TEXT("kernel32.dll")),"ProcessIdToSessionId");
 DWORD sess=0;
 if (pProcessIdToSessionId)
  {
  if(!pProcessIdToSessionId(::GetCurrentProcessId(),&sess))sess=0;
  }
 if (!sess)
  return std::wstring(name);

 WCHAR namebuff[MAX_PATH];
 wsprintfW(namebuff,L"%ws_%x",name,sess);
 return std::wstring(namebuff);
 }

HANDLE OpenMagicMutant(bool can_create)
{
	std::wstring mut_name = GetSessionObjectName(FTPDRIVE_ACTIVE_MUTANT);
	
	HANDLE mtx=NULL;
	OBJECT_ATTRIBUTES oa={sizeof(OBJECT_ATTRIBUTES),0};
	UNICODE_STRING us;
	us.Buffer=(PWCHAR)mut_name.c_str();
	us.Length=mut_name.size()*sizeof(wchar_t);
	us.MaximumLength=us.Length+sizeof(wchar_t);
	oa.ObjectName=&us;
	oa.Attributes=OBJ_OPENIF|OBJ_CASE_INSENSITIVE;
	//MessageBox(0,name,L"zz",0);
	
	if (!NT_ERROR(ZwOpenMutant(&mtx,MUTANT_QUERY_STATE|SYNCHRONIZE,&oa)))
		return mtx;

	if (!can_create)
		return NULL;
	
	if (NT_ERROR(ZwCreateMutant(&mtx,MUTANT_QUERY_STATE|SYNCHRONIZE,&oa,FALSE)))
		return NULL;
	
	return mtx;
}

bool CheckAllowedStartup()
{
	if (GetVersion()&2147483648)
	{
		MessageBox(0,TEXT("Windows 9x family is not supported!"),TEXT("FtpDrive"),MB_ICONERROR);
		return false;
	}
	
	for(;;)
	{
		HANDLE mtx = OpenMagicMutant(false);
		if (!mtx)break;
		ZwClose(mtx);
		
		HWND hidden_hwnd=FindWindow(TEXT("FtpDriveHiddenWindow"),TEXT("FtpDriveHiddenWindow"));
		if (hidden_hwnd)
		{
			PostMessage(hidden_hwnd,WM_USER+1215,0,WM_LBUTTONUP); 
			return false;
		}

		Sleep(100);
	}

	return true;
}

void FSServerMain(HINSTANCE hInstance)
{
	if (!CheckAllowedStartup())
		return;
	
	HANDLE mtx = OpenMagicMutant(true);
	HANDLE hThread = InitializePipeMainThread();
	UiMain(hInstance);
	ZwClose(mtx);
	Sleep(500);
	TerminateThread(hThread,0);
	CloseHandle(hThread);
	ExitProcess(0);
}

std::wstring GetCmdParam(const wchar_t *name)
{
	std::wstring cmdline(GetCommandLine());
	size_t p=cmdline.find(name);
	if(p!=std::wstring::npos)
	{
		p+=wcslen(name);
		size_t e=cmdline.find('\"',p);
		if(e==std::wstring::npos)
			e=cmdline.size();

		return cmdline.substr(p,e-p);
	}
	return std::wstring();
}

int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR     lpCmdLine,
                     int       nCmdShow)
{
	FtpSettings::Init();
	GetModuleFileName(NULL,ModPathName,MAX_PATH+1);
	ModPathName[MAX_PATH]=0;	
	SHDeleteKey(HKEY_CURRENT_USER,TEXT("Software\\KillSoft\\FtpDrive\\StartupCurDir"));
	FtpSettings::UpdateSettings();

	std::wstring edit_file(GetCmdParam(TEXT(" /edit=\"")));
	
	if (edit_file.empty())
	{
		FSServerMain(hInstance);
	}
	else
	{
		std::wstring nv_hostname(GetCmdParam(TEXT(" /nv_hostname=\"")));
		std::wstring nv_hostip(GetCmdParam(TEXT(" /nv_hostip=\"")));
		std::wstring nv_user(GetCmdParam(TEXT(" /nv_user=\"")));
		std::wstring nv_pass(GetCmdParam(TEXT(" /nv_pass=\"")));
		std::wstring nv_flags(GetCmdParam(TEXT(" /nv_flags=\"")));
		SitesEditDialog sed(edit_file,nv_hostname,nv_hostip,nv_user,nv_pass,nv_flags);
		sed.Show();
	}
	return 0;
}



