// UI.cpp : Defines the entry point for the application.
//

#define WINVER 0x0400
#include "stdafx.h"
#include "shellapi.h"
#include "stdlib.h"
#include "process.h"
#include "stdio.h"
#include "dbt.h"
#include "commctrl.h"
#include "tlhelp32.h"
#include <string>
#include "../TrafficCounter.h"
#include "../FtpSocks.h"
#include "../../FtpSettings/settings.h"
#include "../../FtpSettings/defs.h"
#include "../../FtpFS/ntdefs.h"
#include "../../AnyFS/AnyFS.h"
#include "../../AnyFS/SomeUsefull.h"
#include "../FtpWorks/ftpworks.h"
#include "../FtpFS.h"
#include "../ErrorHandler.h"
#include "../../Res/English/resource.h"
#include "resource.h"
#include "dialogs.h"
#include "Psapi.h"
#include "../ntdefs.h"
#include "../../APIHooks/NtDllExports.h"

typedef struct _NOTIFYICONDATA5 {
    DWORD cbSize;
    HWND hWnd;
    UINT uID;
    UINT uFlags;
    UINT uCallbackMessage;
    HICON hIcon;
    TCHAR szTip[128];
    DWORD dwState;
    DWORD dwStateMask;
    TCHAR szInfo[256];
    union {
        UINT uTimeout;
        UINT uVersion;
    };
    TCHAR szInfoTitle[64];
    DWORD dwInfoFlags;
    GUID guidItem;
} NOTIFYICONDATA5, *PNOTIFYICONDATA5;

#define NIF5_INFO        0x00000010
//#define TIMER_ID         0x174
static HICON tray_icons[3] = {
	LoadIcon(GetModuleHandle(NULL),MAKEINTRESOURCE (IDI_ICON1)), //default
		LoadIcon(GetModuleHandle(NULL),MAKEINTRESOURCE (IDI_ICON2)), //cache activity
		LoadIcon(GetModuleHandle(NULL),MAKEINTRESOURCE (IDI_ICON3)) //network activyty
};

static int tray_icon_index = 0;
static HINSTANCE g_h_instance = NULL;
static HWND g_hidden_hwnd = NULL;
static unsigned g_main_loop_tid = 0;
HDESK g_cur_desktop = NULL;

#define HK_UNFREEZE		0x158

typedef struct BALLOON_PARAM_
{
	std::wstring Title;
	std::wstring Text;
	DWORD       Flags;
}BALLOON_PARAM;
DWORD TaskBarMsg = RegisterWindowMessage(TEXT("TaskbarCreated"));

void CallEngine(int cmd,DWORD reserved)
{
	typedef void (WINAPI *FtpDriveCallFunc)(int,DWORD);
	static HINSTANCE engine_dll = LoadLibrary(TEXT("FtpDrive.dll"));
	static FtpDriveCallFunc fdcf=(FtpDriveCallFunc)(engine_dll?GetProcAddress(engine_dll,"_CallMe@8"):NULL);
	if(fdcf)
	{ 
		fdcf(cmd,reserved);
	}else
	{
		MessageBox(0,TEXT("FtpDrive.dll is missing or very strange. I can't use it."),TEXT("FtpDrive"),MB_ICONERROR);
	}
}
//////////////////

static DEV_BROADCAST_VOLUME dbv;
DWORD my_pid = GetCurrentProcessId();
std::set<HWND> bad_hwnd;
BOOL CALLBACK EnumWindowsProc(HWND hwnd,LPARAM lParam)
{
	DWORD pid = my_pid;
	GetWindowThreadProcessId(hwnd,&pid);
	if(pid!=my_pid)
	{
		DWORD dw=0;
		if(!SendMessageTimeout(hwnd,WM_DEVICECHANGE,lParam,(long)&dbv,SMTO_ABORTIFHUNG,500,&dw))bad_hwnd.insert(hwnd);
	}
	//SendMessage(hwnd,WM_DEVICECHANGE,lParam,(long)&dbv);
	return TRUE;
}

BOOL CALLBACK EnumDesktopProc(LPTSTR lpszDesktop,LPARAM lParam)
{
	DWORD dwRec = BSM_APPLICATIONS|BSM_ALLCOMPONENTS;
	HDESK dsk = OpenDesktop(lpszDesktop,0,FALSE,GENERIC_ALL);
	if(dsk)
	{
		EnumDesktopWindows(dsk,EnumWindowsProc,lParam);
		CloseDesktop(dsk);
	}
	return TRUE;
}

void InitDbv(char drive)
{
	dbv.dbcv_size = sizeof(DEV_BROADCAST_VOLUME);
	dbv.dbcv_devicetype = DBT_DEVTYP_VOLUME;
	dbv.dbcv_reserved = 0;  
	dbv.dbcv_flags = DBTF_NET;//0;//
	dbv.dbcv_unitmask = 1<<(drive-'A');
}

static HANDLE br_mtx = CreateMutex(NULL,FALSE,NULL);
static char last_drive = 0;
void BroadcastDevChange(char drive)
{ 
	WaitForSingleObject(br_mtx,INFINITE); 
	bad_hwnd.clear();
	char tmp_last_drive = last_drive; 
	last_drive = drive;
	if(tmp_last_drive&&(tmp_last_drive!=drive))
	{
		TCHAR ddname[4]={tmp_last_drive,':',0,0};
		for(int i=0;((i<10)&&DefineDosDevice(DDD_REMOVE_DEFINITION,ddname,NULL));i++);
		InitDbv(tmp_last_drive);
		EnumDesktops(GetProcessWindowStation(),EnumDesktopProc,DBT_DEVICEREMOVECOMPLETE);
		Sleep(2000);
	} 
	if(drive)
	{
		TCHAR ddname[4]={drive,':',0,0};  
		DefineDosDevice(0,ddname,FtpWorks::tempdir.c_str());
		
		for(int i=0;i<50;i++)
		{
			InitDbv(drive);
			EnumDesktops(GetProcessWindowStation(),EnumDesktopProc,DBT_DEVICEARRIVAL);
			Sleep(20);
		}   
	}
	ReleaseMutex(br_mtx);
}
void __cdecl br_thread(void *)
{ 
	BroadcastDevChange(FtpSettings::FtpFsDrive[0]); 
}
//////////////////
void StopEngine()
{
	static bool engine_stopped = false;
	if(!engine_stopped)
	{
		engine_stopped = true;
		CallEngine(4,0);
		CallEngine(2,0);
		BroadcastDevChange(0);
	}
}


void ReloadSettings()
{
	FtpSettings::UpdateSettings();
	
	DWORD par=(unsigned char)FtpSettings::FtpFsDrive[1];
	par<<=8;
	par|=(DWORD)(unsigned char)FtpSettings::FtpFsDrive[0];
	
	CallEngine(4,par);
	CallEngine(5,FtpSettings::adv_flags);
	CallEngine(6,log_level);
	/*
	static HANDLE evnt = OpenEvent(EVENT_ALL_ACCESS,FALSE,FTPDRIVE_SETTINGS_CHANGED_EVENT);
	if(!evnt)evnt = CreateEvent(NULL,TRUE,FALSE,FTPDRIVE_SETTINGS_CHANGED_EVENT);
	if(evnt)
	{
	PulseEvent(evnt);  
	Sleep(10);
	PulseEvent(evnt);  
	}
	*/
}


void ShowPopup(unsigned int Flags,const std::wstring &Title,const std::wstring &Text)
{
	if (g_hidden_hwnd)
	{
		BALLOON_PARAM blp;
		blp.Flags=Flags;
		blp.Text=Text;
		blp.Title=Title;
		SendMessage(g_hidden_hwnd,WM_USER+1214,0,(LPARAM)&blp);
	}
}

static NOTIFYICONDATA5 nid={sizeof(NOTIFYICONDATA5),0};
void InitNidData(HWND hwnd)//NOTIFYICONDATA5 &nid, 
{
	nid.hWnd=hwnd;
	nid.hIcon=tray_icons[tray_icon_index];
	nid.uCallbackMessage=WM_USER+1215;
	nid.uFlags=NIF_ICON|NIF_MESSAGE|NIF_TIP;
	nid.uID=(DWORD)hwnd;
}

void PrepareSizeStrings(__int64 &size, std::wstring &units, ResHolder &ResDll)
{
	if(size>100*1024*1024)
	{
		size/=(1024*1024);
		units=ResDll.GetString(IDS_HINT_MB);
	}else if(size>100*1024)
	{
		size/=(1024);
		units=ResDll.GetString(IDS_HINT_KB);
	}else 
	{
		units=ResDll.GetString(IDS_HINT_B);
	}  
}


void ProcessTrayMenu(HWND hwnd)
{
	ResHolder ResDll;
	HMENU mnu=LoadMenu(ResDll.Module,MAKEINTRESOURCE(IDR_MENU_TRAY));  
	HMENU sub=GetSubMenu(mnu,0);

	
	if(net_control.IsEmpty())
	{
		::EnableMenuItem(sub,5,MF_BYCOMMAND|MF_GRAYED);
	}
	//ModifyMenu(sub,5,MF_BYCOMMAND|MF_POPUP,(UINT)conn_mnu,TEXT(""));

	SetMenuDefaultItem(sub,1,0);
	SetForegroundWindow(hwnd);  
	POINT pnt;GetCursorPos(&pnt);
	int cmd=(int)TrackPopupMenu(sub,TPM_CENTERALIGN|TPM_RETURNCMD|TPM_NONOTIFY,pnt.x,pnt.y,0,hwnd,NULL);
	switch(cmd)
	{
	case 1:PostMessage(hwnd,WM_USER+1215,0,WM_LBUTTONUP);break;
	case 2:ErrorHandler::ClearAutoActions();break;
	case 3:FtpWorks::ForceCleanCaches();break;
	case 4:    
		PostThreadMessage(g_main_loop_tid,WM_QUIT,0,0);
		break;
	case 5:
		net_control.KillAll();
		break;
	}
	DestroyMenu(mnu);
}

void BlockWindow(HWND wnd)
{
	DWORD pid=0;
	::GetWindowThreadProcessId(wnd,&pid);
	if (pid)
		BlockPid(pid);				
}

typedef BOOL (WINAPI *tIsHungAppWindow)(HWND); 
tIsHungAppWindow pIsHungAppWindow = 
	(tIsHungAppWindow)::GetProcAddress(::GetModuleHandle(TEXT("user32")),"IsHungAppWindow");

BOOL CALLBACK UnfreezeEnumWindowsProc(HWND hWnd, LPARAM lParam)
{
	if (::pIsHungAppWindow(hWnd))BlockWindow(hWnd);
	return TRUE;
}

void __cdecl UnFreezeThread(void *)
{
	HDESK dsk = ::OpenInputDesktop(0,FALSE,GENERIC_ALL), olddsk = NULL;
	if (dsk)
	{
		olddsk = ::GetThreadDesktop(::GetCurrentThreadId());
		::SetThreadDesktop(dsk);
	}
	
	HWND fwnd = ::GetForegroundWindow();
	if (fwnd)
		BlockWindow(fwnd);

	if (pIsHungAppWindow)
	{
		if (dsk)
			::EnumDesktopWindows(dsk,UnfreezeEnumWindowsProc,0);
		else
			::EnumWindows(UnfreezeEnumWindowsProc,0);
	}
	
	net_control.KillAll();

	if (dsk)
	{
		::SetThreadDesktop(olddsk);
		::CloseDesktop(dsk);
	}
}


LRESULT CALLBACK HiddenWndProc(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{ 
	static bool CanShowBalloon = true;

	LRESULT ret=1;
	if (uMsg==TCM_NOTIFY)// || (uMsg==WM_TIMER&&wParam==TIMER_ID))
	{ 
		int cur_ico_index;
		if (lParam&TCF_TRANSFER)//(ftp_traffic_counter.GetSpeed()!=0)
		{
			cur_ico_index = 2;
		}
		else if (lParam&TCF_PROCESS)//(ftp_traffic_counter.IsProcessingRequest())
		{
			cur_ico_index = 1;
		}
		else 
		{
			cur_ico_index = 0;
		}
		
		if (cur_ico_index!=tray_icon_index)
		{
			tray_icon_index = cur_ico_index;
			InitNidData(hwnd);
			Shell_NotifyIcon(NIM_MODIFY,(PNOTIFYICONDATA)&nid);
		}
	}
	else if(uMsg==TaskBarMsg)
	{ 
		wcscpy(nid.szTip,TEXT("FtpDrive"));
		InitNidData(hwnd);
		Shell_NotifyIcon(NIM_ADD,(PNOTIFYICONDATA)&nid);
		ret=1;
	}
	else if(uMsg==WM_USER+1214)
	{
		if(CanShowBalloon)
		{
			BALLOON_PARAM *blp=(BALLOON_PARAM *)lParam;
			nid.uFlags=NIF5_INFO; 
			wcsncpy(nid.szInfoTitle,blp->Title.c_str(),64);nid.szInfoTitle[63]=0;
			wcsncpy(nid.szInfo,blp->Text.c_str(),256);nid.szInfo[255]=0;
			nid.dwInfoFlags=blp->Flags;
			nid.uTimeout=3000;
			Shell_NotifyIcon(NIM_MODIFY,(PNOTIFYICONDATA)&nid);
		}
	}else if(uMsg==WM_USER+1215)
	{ 
		if(lParam==WM_MOUSEMOVE)
		{
			DWORD speed = ftp_traffic_counter.GetSpeed();
			__int64 recv_size = ftp_traffic_counter.GetRecvSize();
			__int64 sent_size = ftp_traffic_counter.GetSentSize();
			ResHolder ResDll;
			
			std::wstring speed_units,recv_units,sent_units;
			if(speed>100*1024)
			{
				speed/=1024;   
				speed_units=ResDll.GetString(IDS_HINT_KBPS);
			}else speed_units=ResDll.GetString(IDS_HINT_BPS);
			
			PrepareSizeStrings(recv_size, recv_units, ResDll);
			PrepareSizeStrings(sent_size, sent_units, ResDll);
			
			_snwprintf(nid.szTip,sizeof(nid.szTip),
				ResDll.GetString(IDS_HINT_STATUS).c_str(),
				speed,speed_units.c_str(),
				(DWORD)recv_size,recv_units.c_str(),
				(DWORD)sent_size,sent_units.c_str()
				);
			InitNidData(hwnd);
			Shell_NotifyIcon(NIM_MODIFY,(PNOTIFYICONDATA)&nid);  
		}else if(lParam==WM_LBUTTONUP)
		{
			static bool InShowUI=false;
			if(!InShowUI)
			{
				DisableHotkey();
				InShowUI=true;
				FDUIDialog dlg;
				if(dlg.Show())
				{
					ReloadSettings();
					_beginthread(br_thread,0,NULL);
				}
				InShowUI=false;
				UpdateHotkey();
			}                                      
		}else if(lParam==WM_RBUTTONUP)
		{
			CanShowBalloon = false;
			ProcessTrayMenu(hwnd);
			CanShowBalloon = true;	  
		}
		ret=1;
	}
	else if(uMsg==WM_HOTKEY  && wParam==HK_UNFREEZE)
	{
		_beginthread(UnFreezeThread,0,NULL);
		ret = 1; 
	}
	else 	
	{
		ret = DefWindowProc(hwnd, uMsg, wParam, lParam); 
	}
	return ret; 
} 

void StartEngine()
{
	HANDLE hToken=NULL;
	OpenProcessToken(GetCurrentProcess(),TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken);
	if(hToken)
	{
		TOKEN_PRIVILEGES tkp;
		LookupPrivilegeValue(NULL, SE_DEBUG_NAME,&tkp.Privileges[0].Luid);
		tkp.PrivilegeCount = 1;
		tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
		AdjustTokenPrivileges(hToken, FALSE, &tkp, 0,(PTOKEN_PRIVILEGES)NULL, 0);
	}
	ReloadSettings();
	
	if(FtpSettings::adv_flags&FTPS_ADV_INJECTONTHEFLY)
	{ 
		HMODULE hDll=::LoadLibrary(TEXT("psapi.dll"));
		if(hDll)
		{
			typedef BOOL (__stdcall *tEnumProcesses)(DWORD* lpidProcess,DWORD cb,DWORD* cbNeeded);
			tEnumProcesses pEnumProcesses=(tEnumProcesses)::GetProcAddress(hDll,"EnumProcesses");
			if(pEnumProcesses)
			{
				DWORD cb,buflen=128*sizeof(DWORD);
				LPDWORD buf;
				for(;;)
				{
					cb=buflen;
					buf=(LPDWORD)malloc(buflen);
					memset(buf,0,cb);
					pEnumProcesses(buf,buflen,&cb);
					if(cb<=buflen)break;
					free(buf);
					buflen=cb+128;
				}
				for(DWORD i=0,j=(cb/sizeof(DWORD));i<j;i++)
				{
					if(buf[i])
					{
						HANDLE prc=::OpenProcess(PROCESS_ALL_ACCESS,NULL,buf[i]);
						if(prc)
						{
							CallEngine(3,(DWORD)prc);
							::CloseHandle(prc);
						}
					}
				}
				free(buf);
			}
			::FreeLibrary(hDll);
		}
	}
	
	CallEngine(1,0);
	
	if(hToken)::CloseHandle(hToken);
	_beginthread(br_thread,0,NULL);
}

void DisableHotkey()
{
	UnregisterHotKey(g_hidden_hwnd,HK_UNFREEZE);
}

void UpdateHotkey()
{
	DisableHotkey();
	DWORD vcode = FtpSettings::hk_unfreeze&0xff;
	DWORD mods = 0;
	if ((FtpSettings::hk_unfreeze>>8)&HOTKEYF_ALT)mods|=MOD_ALT;
	if ((FtpSettings::hk_unfreeze>>8)&HOTKEYF_CONTROL)mods|=MOD_CONTROL;
	if ((FtpSettings::hk_unfreeze>>8)&HOTKEYF_EXT)mods|=MOD_SHIFT;

	if (mods&&vcode)
	{
		char zz[128];sprintf(zz,"%x %x %x",FtpSettings::hk_unfreeze,vcode,mods);
		
		::RegisterHotKey(g_hidden_hwnd,HK_UNFREEZE,mods,vcode);
	}

}

unsigned __stdcall ShellNotifyIconThread(void *p)
{
	if (g_cur_desktop)
		::SetThreadDesktop(g_cur_desktop);
	g_hidden_hwnd=CreateWindow(TEXT("FtpDriveHiddenWindow"),
		TEXT("FtpDriveHiddenWindow"),0,0,0,0,0,NULL,NULL,g_h_instance,0);
	PostMessage(g_hidden_hwnd,TaskBarMsg,0,0);
	ftp_traffic_counter.SetTransferMonitor(g_hidden_hwnd);
	MSG msg;
	UpdateHotkey();
	while (GetMessage(&msg,NULL,0,0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	DisableHotkey();
	::Shell_NotifyIcon(NIM_DELETE,(PNOTIFYICONDATA)&nid);
	::DestroyWindow(g_hidden_hwnd);
	g_hidden_hwnd = NULL;
	ftp_traffic_counter.SetTransferMonitor(NULL);
	return 0;
}


void ResetNotifyThread(bool start_new)
{
	static unsigned tid = 0;
	static HANDLE trd = NULL;
	if (tid)
	{
		for(;;)
		{
			::PostThreadMessage(tid,WM_QUIT,0,0);
			if(::WaitForSingleObject(trd,100)!=WAIT_TIMEOUT)break;
		}
		::CloseHandle(trd);
		trd = NULL;
		tid = 0;
	}

	if (start_new)
	{
		trd = (HANDLE)_beginthreadex(NULL,0,ShellNotifyIconThread,NULL,0,&tid);
	}
}

void MainMessageLoop()
{
	g_main_loop_tid = ::GetCurrentThreadId();
	ResetNotifyThread(true);
	HWINSTA wns = GetProcessWindowStation();
	TCHAR evnt_name[MAX_PATH+32];
	evnt_name[0]=evnt_name[MAX_PATH+31]=0;
	DWORD needlen = 0;
	if (!::GetUserObjectInformation(wns,UOI_NAME,evnt_name,MAX_PATH*sizeof(TCHAR),&needlen))
	{
		wcscpy(evnt_name,TEXT("WinSta0_DesktopSwitch"));
	}
	else
	{
		TCHAR *p=wcsrchr(evnt_name,'\\');
		if(p)memmove(evnt_name,p+1,wcslen(p)*sizeof(TCHAR));
		wcscat(evnt_name,TEXT("_DesktopSwitch"));
	}

	HANDLE evnt = ::OpenEvent(SYNCHRONIZE,FALSE,evnt_name);
	if (!evnt)
		evnt = ::CreateEvent(NULL,TRUE,FALSE,NULL);

	for (;;)
    {
        MSG msg ; 
		
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) 
        { 
            if (msg.message == WM_QUIT)  
			{
				ResetNotifyThread(false);
				::CloseHandle(evnt);
				if (g_cur_desktop)
					::CloseDesktop(g_cur_desktop);
				g_main_loop_tid = 0;
				return; 
			}
            DispatchMessage(&msg); 
        }
		
        DWORD result = ::MsgWaitForMultipleObjects(1, &evnt, FALSE, INFINITE, QS_ALLEVENTS); 
		
        if (result == WAIT_OBJECT_0 
		&& (FtpSettings::adv_flags&FTPS_ADV_MULTIDESKSUPPORT))
		{
			HDESK dsk = ::OpenInputDesktop(0,FALSE,GENERIC_ALL);
			if (dsk)
			{
				std::swap(g_cur_desktop,dsk);
				ResetNotifyThread(true);
				if (dsk)
					::CloseDesktop(dsk);
			}
				
		}
    }
}

void RegisterHiddenWindowClass()
{
	WNDCLASS wc={0};
	wc.lpfnWndProc = (WNDPROC)HiddenWndProc; 
	wc.hInstance = g_h_instance; 
	wc.lpszClassName = TEXT("FtpDriveHiddenWindow");
	RegisterClass(&wc);
}

void UiMain(HINSTANCE hInstance)
{
	InitCommonControls();
	g_h_instance = hInstance;

	RegisterHiddenWindowClass();
	StartEngine();
	
	typedef DWORD (WINAPI *spwssx)(HANDLE, int,int);
	spwssx spwss=(spwssx)GetProcAddress(GetModuleHandle(TEXT("kernel32.dll")),"SetProcessWorkingSetSize");
	if(spwss)spwss(GetCurrentProcess(),-1,-1);
	
	MainMessageLoop();

	StopEngine();
	FtpWorks::DelDirTree(FtpWorks::tempdir);
}



