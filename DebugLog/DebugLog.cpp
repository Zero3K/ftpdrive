#include "windows.h"
#include "stdlib.h"
#include "stdio.h"

HANDLE DbgFile=NULL;
HANDLE dbg_mtx=CreateMutex(NULL,FALSE,TEXT("FTPDRIVE_LOG_MUTEX"));

extern volatile int log_level;

void __cdecl MyWriteDebugLog(const char *format,...)
{    
	if(log_level==0)
		return;
	
	int lerr=GetLastError();
	WaitForSingleObject(dbg_mtx,1000);         
	static bool indebuglog=false;   
	if(!indebuglog)
	{
		indebuglog=1;
		if(!DbgFile)
		{
			CreateDirectory(TEXT("C:\\FtpDriveLogs"),NULL); 
			TCHAR fnamepath[64];
			wsprintf(fnamepath,TEXT("C:\\FtpDriveLogs\\log_%u.log"),GetCurrentProcessId());
			DbgFile=CreateFile(fnamepath,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,0,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,0);//|FILE_FLAG_SEQUENTIAL_SCAN
			SetFilePointer(DbgFile,0,0,FILE_END);      
			indebuglog=0;
			MyWriteDebugLog("----------------------------------------------------");
			MyWriteDebugLog("Started logging in '%ws'",GetCommandLine());
			indebuglog=1;
		}
	}
	
	char buf[1028];
	sprintf(buf,"[%u|%u] ",
		::GetTickCount(),
		::GetCurrentThreadId());

	size_t len = strlen(&buf[0]);
	va_list paramList;
	va_start(paramList, format);
	_vsnprintf(&buf[len],1023-len,format,paramList);
	va_end(paramList);
	buf[1024]=0;
	strcat(&buf[0],"\r\n");   
	DWORD dw;WriteFile(DbgFile,&buf[0],strlen(&buf[0]),&dw,0);           
	indebuglog=0;
	
	ReleaseMutex(dbg_mtx);
	SetLastError(lerr);
 }
