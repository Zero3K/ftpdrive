#include "stdafx.h"
#include <process.h>
#include "TrafficCounter.h"
#include "FtpFS.h"
#include "../AnyFS/AnyFS.h"
#include "../FtpFS/FtpWorks/FtpWorks.h"
#include "../FtpSettings/settings.h"
#include "../DebugLog/DebugLog.h"
#include "ErrorHandler.h"
#include "../FtpFS/ntdefs.h"
#include "../APIHooks/ntapi.h"


namespace PidsWatcher
{
	
	typedef std::map<DWORD,DWORDSET> PIDHANDLES;
	PIDHANDLES pid_handles;
	FastCriticalSection _cs;
	
	void RemovePid(DWORD pid)
	{
		AutoCriticalSection cslock(_cs);
		PIDHANDLES::iterator i=pid_handles.find(pid);
		if(i!=pid_handles.end())
		{
			MyWriteDebugLog("pid_watchdog_thread PID %u exited with %u handles left",pid,i->second.size());
			for(DWORDSET::iterator j=i->second.begin();j!=i->second.end();++j)
			{
				FtpWorks::xxxCloseHandle((HANDLE)*j);
			}
			pid_handles.erase(i);
		}
		else MyWriteDebugLog("pid_watchdog_thread PID %u exited with no handles left",pid);
	}

	void __cdecl pid_watchdog_thread(void *p)
	{
		::SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_HIGHEST);
		DWORD pid=(DWORD)p;
		HANDLE prc=OpenProcess(SYNCHRONIZE,FALSE,pid);  
		::WaitForSingleObject(prc,INFINITE);  
		RemovePid(pid);
		::CloseHandle(prc);

		ErrorHandler::ClearAutoActions(pid);
	}

	void HandleOpened(DWORD pid,DWORD fs_handle)
	{
		AutoCriticalSection cslock(_cs);
		PIDHANDLES::iterator i=pid_handles.find(pid);
		if(i==pid_handles.end())
		{
			DWORDSET ds;
			pid_handles.insert(PIDHANDLES::value_type(pid,ds));   
			_beginthread(pid_watchdog_thread,0,(void *)pid);
			i=pid_handles.find(pid);
		}
		i->second.insert(fs_handle);
	}

	void HandleClosed(DWORD pid,DWORD fs_handle)
	{
		AutoCriticalSection cslock(_cs);
		PIDHANDLES::iterator i =pid_handles.find(pid);
		if(i!=pid_handles.end())
		{
			DWORDSET::iterator j=i->second.find(fs_handle);
			if(j!=i->second.end())i->second.erase(j);
		}
	}
	
}

