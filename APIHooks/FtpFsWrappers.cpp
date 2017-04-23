#include "stdafx.h"
#include "FtpFsWrappers.h"
#include "../FtpFS/ntdefs.h"
#include "../AnyFS/AnyFS.h"
#include "HookSettings.h"
#include "ntapi.h"
#include "hookimpl.h"
#ifdef DBG_LOG
#include "../DebugLog/DebugLog.h"
#endif


namespace FtpFsWrappers
{
	DWORD TlsFSHandles;
	DWORD TlsFSPipes; 
	
	
	typedef struct FTPFILEWRAPPER_ 
	{
		int fs_handle;
		__int64 pos;
		int refcnt;
		std::wstring dirmask;
	}FTPFILEWRAPPER;
	
	typedef std::map<HANDLE,FTPFILEWRAPPER> FS_HANDLES;
	FS_HANDLES fs_handles;
	
	FastCriticalSection hnd_cs;
	
	void  Init()
	{  
		//InitializeCriticalSection(&hnd_cs);
	}
	
	bool EnterSkipLock()//unsigned int skip_id)
	{
		if (::TlsGetValue(TlsFSHandles))
			return false;
		
		::TlsSetValue(TlsFSHandles,(void*)1);
		hnd_cs.Enter();
		return true;
	}
	
	void LeaveSkipLock()//unsigned int skip_id)
	{
		hnd_cs.Leave();
		::TlsSetValue(TlsFSHandles,NULL);
	}
	
	int  FindFsHandle(HANDLE handle)
	{  
		LastErrorSaver error_saver;
		int out=-1;
		if (EnterSkipLock())
		{
			FS_HANDLES::iterator i=fs_handles.find(handle);
			if(i!=fs_handles.end())out=i->second.fs_handle;
			LeaveSkipLock();
		}
		return out;
	}
	
	HANDLE  CreateWrapperHandle(int fs_handle,HANDLE existing_handle)
	{
		LastErrorSaver error_saver;
		HANDLE out = NULL;
		if(EnterSkipLock())
		{
			if(existing_handle)
				out=existing_handle;
			else
				ZwCreateEvent(&out,EVENT_ALL_ACCESS,NULL,SynchronizationEvent,FALSE);   
			
			FTPFILEWRAPPER ffw;
			ffw.fs_handle=fs_handle;
			ffw.pos=0;
			ffw.refcnt=1;   
			if(existing_handle)
			{
				FS_HANDLES::iterator i=fs_handles.find(existing_handle);
				if(i!=fs_handles.end())
				{
					ffw = i->second;
					fs_handles.erase(i);
				}
			}     
			fs_handles.insert(FS_HANDLES::value_type(out,ffw));      
			LeaveSkipLock();
		}
		return out;
	}
	
	void CloseFSHandle(unsigned int fs_handle)
	{  
		ANYFS_CLOSE_REQUEST req;
		ANYFS_CLOSE_REPLY reply;
		req.rawhdr.code=ANYFS_CMD_CLOSE;
		req.rawhdr.pid=g_my_pid;
		req.handle=fs_handle;
		SendRecvFS(&req,sizeof(req),&reply,sizeof(reply));
	}
	
	bool  CloseWrapperHandle(HANDLE handle)
	{
		LastErrorSaver error_saver;
		int status=0;  
		if(EnterSkipLock())
		{   
			unsigned int fs_handle=0xffffffff;
			FS_HANDLES::iterator i=fs_handles.find(handle);
			if(i!=fs_handles.end())
			{    
				status=(--(i->second).refcnt)?1:2;
				if(status==2)
				{
					fs_handle=i->second.fs_handle;
					fs_handles.erase(i);   
				}
			}
			
			if(status==2)
			{
				CloseFSHandle(fs_handle);
				ZwClose(handle);//HookImpl::real
			}
			LeaveSkipLock();
			if(status==1)HookImpl::SectionsNotifyCloseHandle(handle);
		}
		return status!=0;
	}
	
	bool  IncRefWrapperHandle(HANDLE handle)
	{
		LastErrorSaver error_saver;
		bool out=false;
		if(EnterSkipLock())
		{
			FS_HANDLES::iterator i=fs_handles.find(handle);
			if(i!=fs_handles.end())out=true;
			if(out)(i->second).refcnt++;
			LeaveSkipLock();
		}
		return out;
	}
	
	void  SetHandlePos(HANDLE handle,__int64 pos)
	{
		LastErrorSaver error_saver;
		if(EnterSkipLock())
		{
			FS_HANDLES::iterator i=fs_handles.find(handle);
			if(i!=fs_handles.end())i->second.pos=pos;
			LeaveSkipLock();
		}
	}
	
	__int64  GetHandlePos(HANDLE handle)
	{
		LastErrorSaver error_saver;
		__int64 out=-1;
		if(EnterSkipLock())
		{
			FS_HANDLES::iterator i=fs_handles.find(handle);
			if(i!=fs_handles.end())out=i->second.pos;
			LeaveSkipLock();
		}
		return out;
	}
	
	void  SetDirMask(HANDLE handle,const std::wstring &dirmask)
	{
		LastErrorSaver error_saver;
		if(EnterSkipLock())
		{
			FS_HANDLES::iterator i=fs_handles.find(handle);
			if(i!=fs_handles.end())i->second.dirmask=dirmask;
			LeaveSkipLock();
		}
	}
	
	std::wstring  GetDirMask(HANDLE handle)
	{
		LastErrorSaver error_saver;
		std::wstring out;
		if(EnterSkipLock())
		{
			FS_HANDLES::iterator i=fs_handles.find(handle);
			if(i!=fs_handles.end())out=i->second.dirmask;
			LeaveSkipLock();
		}
		return out;
	}
	//
	
	
	
	std::wstring GetSessionObjectName(PWCHAR name)
	{
		typedef BOOL (__stdcall *tProcessIdToSessionId)(DWORD,PDWORD);
		static tProcessIdToSessionId pProcessIdToSessionId=
			(tProcessIdToSessionId)::GetProcAddress(::GetModuleHandle(TEXT("kernel32.dll")),"ProcessIdToSessionId");
		DWORD sess=0;
		if (!pProcessIdToSessionId)
			pProcessIdToSessionId = (tProcessIdToSessionId)::GetProcAddress(::GetModuleHandle(TEXT("kernel32.dll")),"ProcessIdToSessionId");
		
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
	
#define FTPDRIVE_PIPE_NTPATH TEXT("\\Device\\NamedPipe\\")##PIPENAME_FTPFS
#define PIPE_WAIT_NAME TEXT("\\DosDevices\\pipe\\")
	
	static POBJECT_ATTRIBUTES PipeWaitAttributes=NameInitObjectAttributes(PIPE_WAIT_NAME);
	static POBJECT_ATTRIBUTES PipeObjectAttributes=NameInitObjectAttributes(GetSessionObjectName(FTPDRIVE_PIPE_NTPATH));
	static POBJECT_ATTRIBUTES MutantObjectAttributes=NameInitObjectAttributes(GetSessionObjectName(FTPDRIVE_ACTIVE_MUTANT));
	static std::wstring	PipeWaitPipeName(GetSessionObjectName(PIPENAME_FTPFS));
	
	
	
	
	
	NTSTATUS OpenFtpDrivePipe(ULONG nTimeOut, HANDLE &out)
	{
		HANDLE Handle;
		IO_STATUS_BLOCK IoStatusBlock;
		
		NTSTATUS s = HookImpl::realZwOpenFile(
			&Handle,
			(ACCESS_MASK)FILE_READ_ATTRIBUTES | SYNCHRONIZE,
			PipeWaitAttributes,
			&IoStatusBlock,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			FILE_SYNCHRONOUS_IO_NONALERT
			);
		
		if(NT_ERROR(s))
			return s;
		
		DWORD WaitPipeLength =
			sizeof(FILE_PIPE_WAIT_FOR_BUFFER)  + PipeWaitPipeName.size()*sizeof(WCHAR);
		
		PFILE_PIPE_WAIT_FOR_BUFFER WaitPipe = (PFILE_PIPE_WAIT_FOR_BUFFER)malloc(WaitPipeLength);
		WaitPipe->Timeout.QuadPart =- (LONGLONG)UInt32x32To64( 10 * 1000, nTimeOut );
		WaitPipe->TimeoutSpecified = TRUE;
		WaitPipe->NameLength = PipeWaitPipeName.size()*sizeof(WCHAR);
		memcpy(WaitPipe->Name,PipeWaitPipeName.c_str(),WaitPipe->NameLength+sizeof(WCHAR));
		
		s = HookImpl::realZwFsControlFile(Handle,
			NULL, NULL, NULL,
			&IoStatusBlock,
			FSCTL_PIPE_WAIT,// IoControlCode
			WaitPipe,       // Buffer for data to the FS
			WaitPipeLength - sizeof(WCHAR), NULL, 0);
		
		free(WaitPipe);
		
		HookImpl::realZwClose(Handle);
		if(NT_ERROR(s))
			return s;
		
		
		Handle=NULL;
		s=HookImpl::realZwOpenFile(&Handle,
			SYNCHRONIZE|GENERIC_READ|GENERIC_WRITE,PipeObjectAttributes,
			&IoStatusBlock,0,FILE_SYNCHRONOUS_IO_NONALERT);
		if(NT_ERROR(s))
			return s;
		
		out = Handle;
		return (out == NULL)?STATUS_OBJECT_NAME_NOT_FOUND:STATUS_SUCCESS;
	}
	
	
	HANDLE AquireThreadPipe()
	{   
		HANDLE out = ::TlsGetValue(TlsFSPipes);
		if (out)
			return out;
		
		HANDLE mtx;
		if (NT_ERROR(ZwOpenMutant(&mtx,MUTANT_QUERY_STATE,MutantObjectAttributes)))
			return NULL;
		HookImpl::realZwClose(mtx);
		
		for(int i=0;i<100;i++)
		{
			NTSTATUS s = OpenFtpDrivePipe(50, out);
			if (NT_ERROR(s))
			{
				if (s==STATUS_ACCESS_DENIED)
					return NULL;
			}
			else
			{    
				DWORD pid=g_my_pid;
				IO_STATUS_BLOCK isb={0};
				HookImpl::realZwWriteFile(out,NULL,NULL,NULL,&isb,&pid,sizeof(DWORD),NULL,NULL);
				::TlsSetValue(TlsFSPipes,out);
				break;
			}
			Sleep(10);
		}
		
#ifdef DBG_LOG  
		if(!out)MyWriteDebugLog("AquireThreadPipe failed, error %u",GetLastError());
#endif
		return out;
	}
	
	void InvalidateThreadPipe()
	{  
		HANDLE hPipe = ::TlsGetValue(TlsFSPipes);
		if (hPipe)
		{
			::TlsSetValue(TlsFSPipes,NULL);
			HookImpl::realZwClose(hPipe);
		}  
	}
	
	bool  SendFS(void *buf,unsigned int buflen)
	{
		LastErrorSaver error_saver;
		bool out=false; 
		NTSTATUS rv=0xffffffff;
		HANDLE hPipe=AquireThreadPipe();
		DWORD dw=0;
		if(hPipe)
		{   
			IO_STATUS_BLOCK isb={0};
			NTSTATUS rv=HookImpl::realZwWriteFile(hPipe,NULL,NULL,NULL,&isb,buf,buflen,NULL,NULL);
			out=((rv==STATUS_SUCCESS)&&(isb.Information==buflen));
			if(!out)InvalidateThreadPipe();    
#ifdef DBG_LOG   
			MyWriteDebugLog("SendFS buflen=%u dw=%u out=%u status=%x",buflen,dw,(int)out,rv);  
#endif
		}
#ifdef DBG_LOG  
		else MyWriteDebugLog("SendFS AquireThreadPipe failed");  
#endif
		return out;
	}
	
	bool  RecvFS(void *buf,int buflen)
	{
		LastErrorSaver error_saver;
		bool out=false;
		HANDLE hPipe=AquireThreadPipe();
		DWORD dw=0;  
		if (hPipe)
		{   
#ifdef DBG_LOG   
			MyWriteDebugLog("RecvFS enter buflen=%u",buflen);
#endif
			IO_STATUS_BLOCK isb={0};
			NTSTATUS rv=HookImpl::realZwReadFile(hPipe,NULL,NULL,NULL,&isb,buf,buflen,NULL,NULL);  
			out=((rv==STATUS_SUCCESS)&&(isb.Information==(unsigned int)buflen)); 
			if(!out)InvalidateThreadPipe();
#ifdef DBG_LOG   
			MyWriteDebugLog("RecvFS buflen=%u dw=%u out=%u status=%x",buflen,dw,(int)out,rv);
#endif
		}
#ifdef DBG_LOG
		else MyWriteDebugLog("RecvFS AquireThreadPipe failed");
#endif
		return out;
	}
	
	bool  SendRecvFS(void *sendbuf,int sendlen, void *recvbuf,int recvlen)
	{
		bool out=false;
		if(SendFS(sendbuf,sendlen))
		{  
			if((!recvlen)||RecvFS(recvbuf,recvlen))out=true;
		}
		return out;
	}
	
	//////////////////////
	bool ProcessAttach()
	{
		TlsFSHandles = ::TlsAlloc();
		if (TlsFSHandles==TLS_OUT_OF_INDEXES)
			return false;
		
		TlsFSPipes = ::TlsAlloc();
		if (TlsFSPipes==TLS_OUT_OF_INDEXES)
		{
			::TlsFree(TlsFSHandles);
			return false;
		}
		return true;
	}
	
	void ProcessDetach()
	{
		::TlsFree(TlsFSHandles);
		::TlsFree(TlsFSPipes);
	}
	
	void ThreadAttach()
	{
		::TlsSetValue(TlsFSHandles,NULL);  
		::TlsSetValue(TlsFSPipes,NULL);    
	}
		
	void  ThreadDetach()
	{
		InvalidateThreadPipe();
	}

 }

