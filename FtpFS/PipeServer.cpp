#include "stdafx.h"
#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <process.h> 
#include "TrafficCounter.h"
#include "FtpFS.h"
#include "../FtpSettings/settings.h"
#include "FtpWorks/ftpworks.h"
#include "../DebugLog/DebugLog.h"
#include "PidsWatcher.h"
#include "ErrorHandler.h"
#include "../FtpFS/ntdefs.h"
#include "../APIHooks/NtDllExports.h"

typedef std::map<DWORD,DWORD>		DWORD2DWORD;
DWORD2DWORD g_blocked_pids;
FastCriticalSection g_blocked_cs;

void BlockPid(DWORD pid)
{
	AutoCriticalSection cslock(g_blocked_cs);
	DWORD2DWORD::iterator i=g_blocked_pids.find(pid);
	if (i==g_blocked_pids.end())
	{
		g_blocked_pids.insert(
			DWORD2DWORD::value_type(
			pid,::GetTickCount()+5000));
	}
	else
	{
		i->second=::GetTickCount()+5000;
	}
}

bool IsPidBlocked(DWORD pid)
{
	bool out=false;
	AutoCriticalSection cslock(g_blocked_cs);
	DWORD2DWORD::iterator i=g_blocked_pids.find(pid);
	if (i!=g_blocked_pids.end())
	{
		if(::GetTickCount()<i->second)
		{
			out = true;
		}
		else
		{
			g_blocked_pids.erase(i);
		}
	}
	return out;
}

//////////
unsigned int __stdcall InstanceThread(void *lpvParam) 
 { 
 unsigned char chRequest[ANYFS_BASE_HEADER_MAX_SIZE];  
 HANDLE hPipe = (HANDLE) lpvParam;  
 
 READBUFFER read_buffer;
 
 DWORD cbBytesRead=0;  
 DWORD client_pid=0;
 bool fSuccess = ReadFile( hPipe,&client_pid,sizeof(DWORD),&cbBytesRead,NULL)!=FALSE;  
 if(fSuccess && (cbBytesRead==sizeof(DWORD)))
  {
  MyWriteDebugLog("InstanceThread started for pid %u",client_pid);

  for(;;)
   { 
   bool fSuccess = ReadFile( hPipe,chRequest,ANYFS_BASE_HEADER_MAX_SIZE,&cbBytesRead,NULL)!=FALSE;  
   
   if (! fSuccess || cbBytesRead == 0) 
    {
    MyWriteDebugLog("InstanceThread ReadFile error %u",GetLastError());
    break; 
    }
   
   fSuccess = ProceedRequest(hPipe, (LPANYFS_RAW_REQUEST_HEADER)chRequest, cbBytesRead, client_pid, read_buffer);     

   if(! fSuccess)
    {
    MyWriteDebugLog("InstanceThread ProceedRequest error %u",GetLastError());
    break;
    }
   
   }
  MyWriteDebugLog("InstanceThread exit for pid %u",client_pid);
  }else
  {
  MyWriteDebugLog("InstanceThread failed to get PID, error=%u, cbBytesRead=%u",GetLastError(),cbBytesRead);
  }
 

 
 FlushFileBuffers(hPipe); 
 DisconnectNamedPipe(hPipe); 
 CloseHandle(hPipe); 
 return 0;
 }


#define FTPDRIVE_PIPE_W32PATH TEXT("\\\\.\\pipe\\")##PIPENAME_FTPFS
std::wstring session_pipe_name=GetSessionObjectName(FTPDRIVE_PIPE_W32PATH);
unsigned int __stdcall PipeMainThread(void *) 
 {   
 for (;;) 
  { 
  HANDLE hPipe = CreateNamedPipe(session_pipe_name.c_str(),
   PIPE_ACCESS_DUPLEX,PIPE_TYPE_MESSAGE|PIPE_READMODE_MESSAGE|PIPE_WAIT,
   PIPE_UNLIMITED_INSTANCES,1024,1024,0x7fffffff,NULL);
  
  if (hPipe == INVALID_HANDLE_VALUE) 
   {
   MessageBox(0,TEXT("Failed to create named pipe. Exiting."),
    TEXT("FTPFS error"),MB_ICONERROR);
   return -1;
   }
  
  bool fConnected = ConnectNamedPipe(hPipe, NULL) ? true : (GetLastError() == ERROR_PIPE_CONNECTED); 
  
  if (fConnected) 
   {   
   unsigned dwThreadId;
   HANDLE hThread = (HANDLE)_beginthreadex(NULL,0,InstanceThread,hPipe,0,&dwThreadId); 
   CloseHandle(hThread);
   } else         
   {
   CloseHandle(hPipe); 
   }
  } 
 return 1; 
 } 

HANDLE InitializePipeMainThread() 
 {
	FtpWorks::InitFtpWorks();
	unsigned int dwThreadId;
	return (HANDLE)_beginthreadex(NULL,0,PipeMainThread,NULL,0,&dwThreadId);  
}

