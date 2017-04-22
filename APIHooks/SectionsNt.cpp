#include "stdafx.h"
#include "excpt.h"
#include "../FtpFS/ntdefs.h"
#include "HookSettings.h"
#include "../FtpSettings/defs.h"
#include "../AnyFS/AnyFS.h"
#include "shellapi.h"
#include "ftpfswrappers.h"
#include "hookimpl.h"
#include "detours/src/detours.h"
#include "NtDllExports.h"
#ifdef DBG_LOG
#include "../DebugLog/DebugLog.h"
#endif

extern volatile DWORD adv_flags;

namespace HookImpl
{
typedef void *(WINAPI *extAddVectoredExceptionHandlerx)(ULONG,void *);
typedef void *(WINAPI *extRemoveVectoredExceptionHandlerx)(void *);

extAddVectoredExceptionHandlerx AddVectoredExceptionHandler=(extAddVectoredExceptionHandlerx)GetProcAddress(GetModuleHandle(TEXT("kernel32.dll")),"AddVectoredExceptionHandler");
extRemoveVectoredExceptionHandlerx RemoveVectoredExceptionHandler=(extRemoveVectoredExceptionHandlerx)GetProcAddress(GetModuleHandle(TEXT("kernel32.dll")),"RemoveVectoredExceptionHandler");
HANDLE hVectoredHandler=NULL;

typedef struct ADDRENTRY_
 { 
 DWORD         size;
 DWORD         protect;
 DWORD         offset;
 std::wstring   file_name;
 HANDLE        file_handle;
 }ADDRENTRY;

typedef struct SECTENTRY_
 {
 LARGE_INTEGER size;
 std::wstring   file_name;
 }SECTENTRY;

typedef std::vector<std::wstring>           STRINGVECTOR;
typedef std::map<HANDLE,SECTENTRY>         SECTSMAP;


typedef std::map<DWORD,ADDRENTRY>          ADDRSMAP;
////
SECTSMAP         sects;
ADDRSMAP         addrs;
void *realZwCreateSection; 
void *realZwOpenSection; 
void *realZwQuerySection; 
void *realZwExtendSection; 
void *realZwMapViewOfSection; 
void *realZwUnmapViewOfSection; 
void *realZwAreMappedFilesTheSame; 
////
std::wstring ExtractPath(const std::wstring &pathname)
 {
 std::wstring path(pathname);
 int x=path.rfind('\\');
 if(x==std::wstring::npos)x=path.rfind('/');
 if(x!=std::wstring::npos)path.resize(x+1);else path.resize(0);
 return path;
 }

void EnsurePathInEnv(const std::wstring &pathname)
 {
 std::wstring path=ExtractPath(pathname);
 if(!path.empty())
  {
  path.resize(path.size()-1);
  std::wstring envpath;
  path.insert(0,L";");
  path.append(1,L';');
  DWORD pathlen=1024;
  std::vector<TCHAR> pathbuf(pathlen);
  for(;;)
   {   
   pathlen=GetEnvironmentVariable(TEXT("PATH"),&pathbuf[0],pathlen);
   if(pathlen<pathbuf.size())
    {
    envpath.assign(&pathbuf[0],pathlen);
    break;
    }
   pathbuf.resize(pathlen);
   }
  if(envpath.find(path)==std::wstring::npos)
   {   
   envpath.append(path);
   SetEnvironmentVariable(TEXT("PATH"),envpath.c_str());
   }
  }
 }

void CopyDllsFor(const std::wstring &pathname)
 { 
 LastErrorSaver error_saver;
 std::wstring path=ExtractPath(pathname);
 //unsigned int dirhandle;
 ANYFS_OPEN_REQUEST openreq;
 ANYFS_OPEN_REPLY openreply;
 openreq.rawhdr.code=ANYFS_CMD_OPEN;
 openreq.rawhdr.pid=g_my_pid;
 openreq.dwCreateDisposition=FILE_OPEN;
 openreq.dwCreateOptions=0;
 openreq.dwDesiredAccess=FILE_LIST_DIRECTORY;
 wcscpy(openreq.filename,path.c_str());
 if((FtpFsWrappers::SendRecvFS(&openreq,sizeof(openreq), &openreply,sizeof(openreply)))&&(openreply.nt_status==STATUS_SUCCESS))
  {  
  STRINGVECTOR dlls;  
  ANYFS_LIST_REQUEST listreq;
  ANYFS_INFO_REPLY listreply; 
  listreq.rawhdr.code=ANYFS_CMD_LIST;
  listreq.rawhdr.pid=g_my_pid;
  listreq.handle=openreply.handle;
  for(listreq.index=0;;listreq.index++)
   {
   if(FtpFsWrappers::SendRecvFS(&listreq,sizeof(listreq), &listreply,sizeof(listreply))&&listreply.nt_status==STATUS_SUCCESS)
    {
    listreply.file_name[ANYFS_MAX_PATH-1]=0;
    if(PathMatchSpec(listreply.file_name,TEXT("*.dll")))
     {     
     std::wstring dllname(path);     
     dllname.append(listreply.file_name);
     dlls.push_back(dllname);    
     }
    }else break;   
   }
  FtpFsWrappers::CloseFSHandle(openreply.handle);
  for(STRINGVECTOR::iterator s=dlls.begin();s!=dlls.end();++s)
   {
   //*s;
   wcsncpy(openreq.filename,s->c_str(),ANYFS_MAX_PATH);
   if((FtpFsWrappers::SendRecvFS(&openreq,sizeof(openreq), &openreply,sizeof(openreply)))&&(openreply.nt_status==STATUS_SUCCESS))
    {  
    ANYFS_TEMP_REQUEST tempreq;
    ANYFS_TEMP_REPLY tempreply; 
    tempreq.rawhdr.code=ANYFS_CMD_TEMP;
    tempreq.rawhdr.pid=g_my_pid;
    tempreq.handle=openreply.handle;
    tempreq.max_size=0xffffffff;
    if(FtpFsWrappers::SendRecvFS(&tempreq,sizeof(tempreq), &tempreply,sizeof(tempreply))) 
     {
     tempreply.file_name[ANYFS_MAX_PATH-1]=0;
     wcslwr(tempreply.file_name);
     std::wstring temppathname(tempreply.file_name);
     EnsurePathInEnv(temppathname);
     }
    FtpFsWrappers::CloseFSHandle(openreply.handle);
    }
   }
  }
 }

NTSTATUS __stdcall myZwCreateSection(OUT PHANDLE SectionHandle,IN ACCESS_MASK DesiredAccess,
                                    IN POBJECT_ATTRIBUTES ObjectAttributes,IN PLARGE_INTEGER SectionSize OPTIONAL,
                                    IN ULONG Protect,IN ULONG Attributes,IN HANDLE FileHandle)
 {
 int fs_handle=FtpFsWrappers::FindFsHandle(FileHandle);
  
 if(fs_handle!=-1)
  {
  LastErrorSaver error_saver;
  NTSTATUS rv;
  if((Attributes&SEC_IMAGE)||(!hVectoredHandler)||(!(adv_flags&FTPS_ADV_USEGUARDPAGES)))
   {   
   ANYFS_TEMP_REQUEST req;
   ANYFS_TEMP_REPLY reply; 
   req.rawhdr.code=ANYFS_CMD_TEMP;
   req.rawhdr.pid=g_my_pid;
   req.handle=fs_handle;
   req.max_size=SectionSize?SectionSize->LowPart:0xffffffff;
   
#ifdef DBG_LOG   
   MyWriteDebugLog("myZwCreateSection point 0 SectionSize=%i",req.max_size);
#endif
   if(!FtpFsWrappers::SendRecvFS(&req,sizeof(req), &reply,sizeof(reply))) 
    return STATUS_DATA_ERROR;
   if(reply.nt_status!=STATUS_SUCCESS)return reply.nt_status;
   
   reply.file_name[ANYFS_MAX_PATH-1]=0;
   
#ifdef DBG_LOG   
   MyWriteDebugLog("myZwCreateSection point 1 status %x name='%ws'",(DWORD)reply.nt_status,reply.file_name);
#endif
   
   if(reply.nt_status!=STATUS_SUCCESS)return reply.nt_status;    
   std::wstring file_name(reply.file_name);
   if(file_name[0]!=L'\\')file_name.insert(0,TEXT("\\??\\"));
   
   
   HANDLE TmpFileHandle=NULL;  
   rv=RealCreateFile(&TmpFileHandle,Attributes&SEC_IMAGE?FILE_READ_DATA|FILE_EXECUTE:FILE_READ_DATA,file_name,FILE_OPEN);
#ifdef DBG_LOG   
   MyWriteDebugLog("myZwCreateSection point 2 name='%ws' status=%x",reply.file_name,rv);
#endif
   if(rv==STATUS_SUCCESS)
    {
    if(Protect==PAGE_READWRITE)Protect=PAGE_WRITECOPY;else
    if(Protect==PAGE_EXECUTE_READWRITE)Protect=PAGE_EXECUTE_WRITECOPY;
     
     if(Attributes&SEC_IMAGE)
      {
      ANYFS_INFO_REQUEST inforeq;
      ANYFS_INFO_REPLY inforeply; 
      inforeq.rawhdr.code=ANYFS_CMD_INFO;
      inforeq.rawhdr.pid=g_my_pid;
      inforeq.handle=fs_handle;
      if(FtpFsWrappers::SendRecvFS(&inforeq,sizeof(inforeq), &inforeply,sizeof(inforeply))
       &&(inforeply.nt_status==STATUS_SUCCESS))CopyDllsFor(std::wstring(inforeply.file_name));
      }
     rv=((NTSTATUS(__stdcall*)(PHANDLE,ACCESS_MASK,POBJECT_ATTRIBUTES,PLARGE_INTEGER,ULONG,ULONG,HANDLE)
      )realZwCreateSection)(SectionHandle,DesiredAccess,ObjectAttributes,SectionSize,Protect,Attributes,TmpFileHandle); 
     //realZwClose(TmpFileHandle);     
    }
   }else
   {
   rv=STATUS_SUCCESS;
   LARGE_INTEGER SecSize;
   if(SectionSize)
    {
    SecSize=*SectionSize;
    }
   ANYFS_INFO_REQUEST req;
   ANYFS_INFO_REPLY reply; 
   req.rawhdr.code=ANYFS_CMD_INFO;
   req.rawhdr.pid=g_my_pid;
   req.handle=fs_handle;    
   if(FtpFsWrappers::SendRecvFS(&req,sizeof(req), &reply,sizeof(reply)))     
    {
    if((!SectionSize) || (SecSize.QuadPart>reply.size.QuadPart))SecSize=reply.size;
    }else rv=STATUS_DATA_ERROR;
    
   if(rv==STATUS_SUCCESS)
    {
    if(FtpFsWrappers::IncRefWrapperHandle(FileHandle))
     *SectionHandle=FileHandle;
    else
     rv=STATUS_DATA_ERROR;
    }
#ifdef DBG_LOG   
   MyWriteDebugLog("myZwCreateSection point 2 name='%ws' status=%x",reply.file_name,rv);
#endif
    //rv=((NTSTATUS(__stdcall*)(PHANDLE,ACCESS_MASK,POBJECT_ATTRIBUTES,PLARGE_INTEGER,ULONG,ULONG,HANDLE)
     //)realZwCreateSection)(SectionHandle,DesiredAccess,ObjectAttributes,&SecSize,Protect,Attributes,NULL); 
   if(rv==STATUS_SUCCESS) 
    {
    if(FtpFsWrappers::EnterSkipLock())
     {
     //EnterCriticalSection(&sect_cs);
     SECTENTRY se;
     se.file_name=reply.file_name;    
     se.size=SecSize;
     sects.insert(SECTSMAP::value_type(*SectionHandle,se));
     //LeaveCriticalSection(&sect_cs)
     FtpFsWrappers::LeaveSkipLock();
     }
    }
   }
   
  
#ifdef DBG_LOG
  MyWriteDebugLog("myZwCreateSection point 4 DesiredAccess=%x Protect=%x Handle=%x status=%x",DesiredAccess,Protect,*SectionHandle,rv);
#endif
  return rv;
  }
 
 return ((NTSTATUS(__stdcall*)(PHANDLE,ACCESS_MASK,POBJECT_ATTRIBUTES,PLARGE_INTEGER,ULONG,ULONG,HANDLE)
  )realZwCreateSection)(SectionHandle,DesiredAccess,ObjectAttributes,SectionSize,Protect,Attributes,FileHandle); 
 }

void SectionsNotifyCloseHandle(HANDLE handle)
 {
 //EnterCriticalSection(&sect_cs); 
 LastErrorSaver error_saver;
 if(FtpFsWrappers::EnterSkipLock())
  {
  SECTSMAP::iterator i=sects.find(handle);
  if(i!=sects.end())sects.erase(i);
  FtpFsWrappers::LeaveSkipLock();
  }
 //LeaveCriticalSection(&sect_cs);
 }

NTSTATUS __stdcall myZwMapViewOfSection(IN HANDLE SectionHandle,IN HANDLE ProcessHandle,
                  IN OUT PVOID *BaseAddress,IN ULONG ZeroBits,
                  IN ULONG CommitSize,IN OUT PLARGE_INTEGER SectionOffset OPTIONAL,
                  IN OUT PULONG ViewSize,IN SECTION_INHERIT InheritDisposition,
                  IN ULONG AllocationType,IN ULONG Protect)
 {
 NTSTATUS out;
 std::wstring file_name;
 ULONG MapSize=0;
 //EnterCriticalSection(&sect_cs);
 LastErrorSaver error_saver;
 if(FtpFsWrappers::EnterSkipLock())
  {
  SECTSMAP::iterator i=sects.find(SectionHandle);
  if(i!=sects.end())
   {
   file_name=i->second.file_name;  
   if(ViewSize)
    {
    if((*ViewSize==0)||(*ViewSize>i->second.size.LowPart))
     MapSize=*ViewSize=i->second.size.LowPart;
    else
     MapSize=*ViewSize;
    }else 
    {
    MapSize=i->second.size.LowPart;    
    }   
   } 
 //LeaveCriticalSection(&sect_cs);
  FtpFsWrappers::LeaveSkipLock();
  }
 
 
 if(!file_name.empty())
  {
  CommitSize=MapSize;
#ifdef DBG_LOG  
  MyWriteDebugLog("myZwMapViewOfSection Handle=%x SectionOffset=%i ViewSize=%i CommitSize=%i File='%ws'",(DWORD)SectionHandle,(int)(SectionOffset?SectionOffset->LowPart:0xffffffff),(int)(ViewSize?*ViewSize:0xffffffff),(int)CommitSize,file_name.c_str());
#endif
 
  if(MapSize%4096)
   {
   MapSize+=4096-(MapSize%4096);
   }
  *BaseAddress=VirtualAlloc(NULL,MapSize,MEM_COMMIT,PAGE_READWRITE|PAGE_GUARD);
  out=*BaseAddress?STATUS_SUCCESS:STATUS_ACCESS_DENIED;
  
  if(!FtpFsWrappers::IncRefWrapperHandle(SectionHandle))
   {
   if(*BaseAddress)VirtualFree(*BaseAddress,0,MEM_RELEASE);   
   out=STATUS_ACCESS_DENIED;
   }
  

  if(out==STATUS_SUCCESS)
   {
   if(FtpFsWrappers::EnterSkipLock())
    {
    //EnterCriticalSection(&sect_cs);
    ADDRENTRY ae;
    ae.file_name=file_name;
    ae.protect=Protect;
    ae.size=MapSize;
    ae.offset=SectionOffset?SectionOffset->LowPart:0;
    ae.file_handle=SectionHandle;
    addrs.insert(ADDRSMAP::value_type((DWORD)*BaseAddress,ae));
    //LeaveCriticalSection(&sect_cs);
    FtpFsWrappers::LeaveSkipLock();
    }
   }
  }else
  {
  out=((NTSTATUS(__stdcall*)(HANDLE,HANDLE,PVOID *,ULONG,ULONG,PLARGE_INTEGER,PULONG,SECTION_INHERIT,ULONG,ULONG)
   )realZwMapViewOfSection)(SectionHandle,ProcessHandle,BaseAddress,ZeroBits,CommitSize,SectionOffset,ViewSize,InheritDisposition,AllocationType,Protect);
  }
 return out;
 }

NTSTATUS __stdcall myZwUnmapViewOfSection(IN HANDLE ProcessHandle,IN PVOID BaseAddress)
 {
 HANDLE OurWrapperHandle=NULL;
 LastErrorSaver error_saver;
 if(FtpFsWrappers::EnterSkipLock())
  {
  //EnterCriticalSection(&sect_cs);
  ADDRSMAP::iterator i=addrs.find((DWORD)BaseAddress);
  if(i!=addrs.end())
   {
   OurWrapperHandle=i->second.file_handle;
   addrs.erase(i);  
   }
  //LeaveCriticalSection(&sect_cs);
  FtpFsWrappers::LeaveSkipLock();
  }
 
 if(OurWrapperHandle)
  {
  FtpFsWrappers::CloseWrapperHandle(OurWrapperHandle);
  VirtualFree(BaseAddress,0,MEM_RELEASE);
  SetProcessWorkingSetSize(GetCurrentProcess(),-1,-1);
  return STATUS_SUCCESS;
  }
 return ((NTSTATUS(__stdcall*)(HANDLE,PVOID))realZwUnmapViewOfSection)(ProcessHandle,BaseAddress);
 }

LONG WINAPI PagingVectoredHandler(struct _EXCEPTION_POINTERS *ExceptionInfo)
{

if((ExceptionInfo->ExceptionRecord->ExceptionCode!=EXCEPTION_GUARD_PAGE)
   ||(ExceptionInfo->ExceptionRecord->ExceptionFlags&EXCEPTION_NONCONTINUABLE))
 {
 return EXCEPTION_CONTINUE_SEARCH;
 }
ULONG addr=ExceptionInfo->ExceptionRecord->ExceptionInformation[1];
if(addr%4096)addr=4096*(addr/4096);

HANDLE file_handle=NULL;
LARGE_INTEGER offset;

//EnterCriticalSection(&sect_cs);
LastErrorSaver error_saver;
if(!FtpFsWrappers::EnterSkipLock())return EXCEPTION_CONTINUE_SEARCH;

//ADDRSMAP::iterator i=addrs.upper_bound(addr);
for(ADDRSMAP::iterator i=addrs.begin();i!=addrs.end();++i)
 {
 ULONG base=(ULONG)i->first;
 if((base<=addr)&&((base+i->second.size)>=addr))
  {
  file_handle=i->second.file_handle;
  offset.QuadPart=(__int64)i->second.offset+(__int64)(addr-base);
  break;
  }
 } 
//LeaveCriticalSection(&sect_cs);
FtpFsWrappers::LeaveSkipLock();

if(file_handle)
 {
 IO_STATUS_BLOCK IoStatusBlock; 
 ZwReadFile(file_handle,NULL,NULL,NULL,&IoStatusBlock,(void *)addr,4096,&offset,NULL);//ANYFS_OPT_NO_CACHE_WRITE);
 return EXCEPTION_CONTINUE_EXECUTION;
 }else
 {
 return EXCEPTION_CONTINUE_SEARCH;
 }
}

void SectionsEnsureContent(ULONG_PTR addr, SIZE_T size)
{
	LastErrorSaver error_saver;
	if(FtpFsWrappers::EnterSkipLock())
	{
		bool need_ensure=false;
		for(ADDRSMAP::iterator i=addrs.begin();i!=addrs.end();++i)
		{
			ULONG base=(ULONG)i->first;
			if((base<=addr)&&((base+i->second.size)>=addr))
			{
				need_ensure=true;
				break;
			}
		} 
		FtpFsWrappers::LeaveSkipLock();
		if(need_ensure)
		{
			for(PCHAR p=(PCHAR)addr,e=(PCHAR)(addr+size);p<e;p+=4096)
			{
				volatile char c=*p;
			}
		}
	}
}

void  NtDllHookSections()
 {
 //InitializeCriticalSection(&sect_cs);
 hVectoredHandler = AddVectoredExceptionHandler?AddVectoredExceptionHandler(1,PagingVectoredHandler):NULL; 
 realZwCreateSection = DetourByPtr((PBYTE)ZwCreateSection,(PBYTE)&myZwCreateSection); 
 realZwMapViewOfSection = DetourByPtr((PBYTE)ZwMapViewOfSection,(PBYTE)&myZwMapViewOfSection); 
 realZwUnmapViewOfSection = DetourByPtr((PBYTE)ZwUnmapViewOfSection,(PBYTE)&myZwUnmapViewOfSection); 
 
 /*realZwOpenSection = DetourByName(m,"ZwOpenSection",(PBYTE)&myZwOpenSection); 
 realZwQuerySection = DetourByName(m,"ZwQuerySection",(PBYTE)&myZwQuerySection); 
 realZwExtendSection = DetourByName(m,"ZwExtendSection",(PBYTE)&myZwExtendSection); 
 realZwAreMappedFilesTheSame = DetourByName(m,"ZwAreMappedFilesTheSame",(PBYTE)&myZwAreMappedFilesTheSame); 
 */ 
 }


void  NtDllHookSectionsDeinit()
 {
 if(hVectoredHandler&&RemoveVectoredExceptionHandler)RemoveVectoredExceptionHandler(hVectoredHandler);
 }

}

