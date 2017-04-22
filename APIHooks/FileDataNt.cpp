#include "stdafx.h"
#include "../FtpFS/ntdefs.h"
#include "../AnyFS/AnyFS.h"
#include "shellapi.h"
#include "ftpfswrappers.h"

#include "hookimpl.h"
#include "detours/src/detours.h"
#include "HookSettings.h"
#include "NtDllExports.h"
#ifdef DBG_LOG
#include "../DebugLog/DebugLog.h"
#endif

namespace HookImpl
{
realZwClose_t                  realZwClose;
realZwCreateFile_t             realZwCreateFile;
realZwOpenFile_t               realZwOpenFile;
realZwReadFile_t               realZwReadFile;
realZwWriteFile_t              realZwWriteFile;
realZwFsControlFile_t          realZwFsControlFile;
realZwDeviceIoControlFile_t    realZwDeviceIoControlFile;

void *realZwFlushBuffersFile;
void *realZwCancelIoFile;
void *realZwLockFile;
void *realZwUnlockFile;
void *realZwDuplicateObject;

NTSTATUS __stdcall myZwClose(HANDLE Handle)
{
if(FtpFsWrappers::CloseWrapperHandle(Handle))return STATUS_SUCCESS;
return realZwClose(Handle);
}

///////////

NTSTATUS  RealCreateFile(PHANDLE FileHandle,ACCESS_MASK DesiredAccess,const std::wstring &PathName,ULONG CreateDisposition)
 {
 POBJECT_ATTRIBUTES ObjectAttributes=NameInitObjectAttributes(PathName);
 IO_STATUS_BLOCK IoStatusBlock;
 NTSTATUS  out=realZwCreateFile(FileHandle,SYNCHRONIZE|DesiredAccess,ObjectAttributes,&IoStatusBlock,0,FILE_ATTRIBUTE_NORMAL,FILE_SHARE_READ|FILE_SHARE_WRITE,FILE_OPEN_IF,FILE_SYNCHRONOUS_IO_NONALERT,NULL,0);
 NameFreeObjectAttributes(ObjectAttributes);
 return out;
 }

std::wstring GetPathByHandle(int fs_handle)
 {
 std::wstring pathname;
 ANYFS_INFO_REQUEST req;
 ANYFS_INFO_REPLY reply; 
 req.rawhdr.code=ANYFS_CMD_INFO;
 req.rawhdr.pid=g_my_pid;
 req.handle=fs_handle;
 
 if(FtpFsWrappers::SendRecvFS(&req,sizeof(req), &reply,sizeof(reply)))     
  {
  if(reply.nt_status==STATUS_SUCCESS)
   {
   pathname.assign(reply.file_name);
   if(pathname.size()&&(pathname[pathname.size()-1]!=TCHAR('\\')))pathname.append(1,TCHAR('\\'));
   }else pathname.resize(0);
  }
 return pathname;
 }

NTSTATUS  WrapperOpenFile(PHANDLE FileHandle,ACCESS_MASK DesiredAccess,
                                    POBJECT_ATTRIBUTES ObjectAttributes,
                                    PIO_STATUS_BLOCK IoStatusBlock,
                                    ULONG CreateDisposition,ULONG CreateOptions)
 {
 if((!(CreateOptions&FILE_DIRECTORY_FILE))&&
  (CreateDisposition!=FILE_CREATE)&&
  (CreateDisposition!=FILE_SUPERSEDE)&&
  (CreateDisposition!=FILE_OVERWRITE)&&
  (CreateDisposition!=FILE_OVERWRITE_IF)&&
  ((DesiredAccess&FILE_WRITE_ATTRIBUTES)
   ||(DesiredAccess&FILE_WRITE_EA)
   ||(DesiredAccess&FILE_WRITE_DATA)
   ||(DesiredAccess&FILE_DELETE_CHILD)
   ||(DesiredAccess==FILE_ALL_ACCESS)
   ))return IoStatusBlock->Status=STATUS_ACCESS_DENIED;

 if ((CreateOptions&FILE_NO_INTERMEDIATE_BUFFERING) && (!IsThisKillCopy()))
 {
	 CreateOptions&=~(ULONG)FILE_NO_INTERMEDIATE_BUFFERING;
 }

 std::wstring pathname;
 if(ObjectAttributes->RootDirectory)
  {
  int fs_handle=FtpFsWrappers::FindFsHandle(ObjectAttributes->RootDirectory);
  if(fs_handle!=-1)
   pathname=GetPathByHandle(fs_handle);
  }
  

 
   
 //int namelen=ObjectAttributes->ObjectName->Length;
 //if((namelen+1)>ANYFS_MAX_PATH*sizeof(WCHAR))namelen=(ANYFS_MAX_PATH-1)*sizeof(WCHAR);
   
 //std::vector<TCHAR> tmpname(ANYFS_MAX_PATH);
   
 //WideCharToMultiByte(CP_ACP,0,ObjectAttributes->ObjectName->Buffer,ObjectAttributes->ObjectName->Length/2,&tmpname[0],ANYFS_MAX_PATH,NULL,NULL);
 //tmpname[namelen]=0;
 if(ObjectAttributes->ObjectName->Length)
  {
  if((ObjectAttributes->ObjectName->Length>4*sizeof(WCHAR)) && 
   (!memicmp(ObjectAttributes->ObjectName->Buffer,L"\\??\\",4*sizeof(WCHAR))))
   pathname.assign(ObjectAttributes->ObjectName->Buffer,ObjectAttributes->ObjectName->Length/sizeof(WCHAR));
  else
   pathname.append(ObjectAttributes->ObjectName->Buffer,ObjectAttributes->ObjectName->Length/sizeof(WCHAR));
  }
 if((pathname.size()+1)>ANYFS_MAX_PATH)pathname.resize(ANYFS_MAX_PATH-1);

 ANYFS_OPEN_REQUEST req;
 ANYFS_OPEN_REPLY reply;
 req.rawhdr.code=ANYFS_CMD_OPEN;
 req.rawhdr.pid=g_my_pid;
 req.dwCreateDisposition=CreateDisposition;
 req.dwCreateOptions=CreateOptions;
 req.dwDesiredAccess=DesiredAccess; 
 wcscpy(req.filename,pathname.c_str());
 IoStatusBlock->Status=reply.nt_status=STATUS_OBJECT_NAME_NOT_FOUND;

 if(FtpFsWrappers::SendRecvFS(&req,sizeof(req), &reply,sizeof(reply)))
  {  
  if(reply.nt_status==STATUS_SUCCESS)
   {      
   *FileHandle=FtpFsWrappers::CreateWrapperHandle(reply.handle);
   }  
  IoStatusBlock->Status=reply.nt_status;
  IoStatusBlock->Information=reply.nt_info;
  }else
   reply.nt_status=STATUS_DATA_ERROR;
 //MyWriteDebugLog("WrapperOpenFile for %ws CreateDisposition=%x CreateOptions=%x status %x handle %x",req.filename,CreateDisposition,CreateOptions,reply.nt_status,(DWORD)*FileHandle);
 
 /*
 if(pathname.find("Config")!=-1)
  {
  __asm
   {
   int 3
   }
  }
  */
 return reply.nt_status;
 }
NTSTATUS __stdcall myZwCreateFile(OUT PHANDLE FileHandle,IN ACCESS_MASK DesiredAccess,
                               IN POBJECT_ATTRIBUTES ObjectAttributes,
                               OUT PIO_STATUS_BLOCK IoStatusBlock,
                               IN PLARGE_INTEGER AllocationSize OPTIONAL,
                               IN ULONG FileAttributes,IN ULONG ShareAccess,
                               IN ULONG CreateDisposition,IN ULONG CreateOptions,
                               IN PVOID EaBuffer OPTIONAL,IN ULONG EaLength)
{
if(hhook&&HookSettings::IsThisAffected(ObjectAttributes))
 {
 return WrapperOpenFile(FileHandle,DesiredAccess,ObjectAttributes,IoStatusBlock,CreateDisposition,CreateOptions);
 }
return realZwCreateFile(FileHandle,DesiredAccess,ObjectAttributes,IoStatusBlock,AllocationSize,FileAttributes,ShareAccess,CreateDisposition,CreateOptions,EaBuffer,EaLength); 
}
///////////
NTSTATUS __stdcall myZwOpenFile(OUT PHANDLE FileHandle,IN ACCESS_MASK DesiredAccess,
                                IN POBJECT_ATTRIBUTES ObjectAttributes,
                                OUT PIO_STATUS_BLOCK IoStatusBlock,
                                IN ULONG ShareAccess,IN ULONG OpenOptions)
{
if(hhook&&HookSettings::IsThisAffected(ObjectAttributes))
 {
 return WrapperOpenFile(FileHandle,DesiredAccess,ObjectAttributes,IoStatusBlock,FILE_OPEN,OpenOptions);
 }

return realZwOpenFile(FileHandle,DesiredAccess,ObjectAttributes,
                         IoStatusBlock,ShareAccess,OpenOptions);
}

///////////
NTSTATUS __stdcall myZwReadFile(IN HANDLE FileHandle,IN HANDLE Event OPTIONAL,
                                 IN PIO_APC_ROUTINE ApcRoutine OPTIONAL,
                                 IN PVOID ApcContext OPTIONAL,
                                 OUT PIO_STATUS_BLOCK IoStatusBlock,IN PVOID Buffer,
                                 IN ULONG Length,IN PLARGE_INTEGER ByteOffset OPTIONAL,
                                 IN PULONG Key OPTIONAL)
 {
 int fs_handle=FtpFsWrappers::FindFsHandle(FileHandle);
 if(fs_handle!=-1)
  {
  IoStatusBlock->Information=0;
  ANYFS_READ_REQUEST req;
  ANYFS_READ_REPLY_HEADER reply; 
  req.rawhdr.code=ANYFS_CMD_READ;
  req.rawhdr.pid=g_my_pid;
  req.handle=fs_handle;
  req.max_size=Length;
  req.options=0;
  LARGE_INTEGER CurOffset;
  if(ByteOffset)
   {   
   req.offset_hi=CurOffset.HighPart=ByteOffset->HighPart;
   req.offset_low=CurOffset.LowPart=ByteOffset->LowPart;
   }else
   {   
   CurOffset.QuadPart=FtpFsWrappers::GetHandlePos(FileHandle);   
   req.offset_hi=CurOffset.HighPart;
   req.offset_low=CurOffset.LowPart;
   }
    
  if(FtpFsWrappers::SendRecvFS(&req,sizeof(req), &reply,sizeof(reply))&&
   ((!reply.len)||FtpFsWrappers::RecvFS(Buffer,reply.len)))
   {
   IoStatusBlock->Information=reply.len;
   IoStatusBlock->Status=reply.nt_status;
   FtpFsWrappers::SetHandlePos(FileHandle,CurOffset.QuadPart+(unsigned __int64)reply.len);
   }else IoStatusBlock->Status=STATUS_DATA_ERROR;

  
  NTSTATUS out=IoStatusBlock->Status;
  if(Event)ZwSetEvent(Event,NULL);
  return out;
  }

 return realZwReadFile(FileHandle,Event,ApcRoutine,
  ApcContext,IoStatusBlock,Buffer,Length,ByteOffset,Key);
 }
///////////
NTSTATUS __stdcall myZwWriteFile(IN HANDLE FileHandle,IN HANDLE Event OPTIONAL,
                                 IN PIO_APC_ROUTINE ApcRoutine OPTIONAL,
                                 IN PVOID ApcContext OPTIONAL,
                                 OUT PIO_STATUS_BLOCK IoStatusBlock,IN PVOID Buffer,
                                 IN ULONG Length,IN PLARGE_INTEGER ByteOffset OPTIONAL,
                                 IN PULONG Key OPTIONAL)
 {
 int fs_handle=FtpFsWrappers::FindFsHandle(FileHandle);
 if(fs_handle!=-1)
  {
  IoStatusBlock->Information=0;
  ANYFS_WRITE_REQUEST req;
  ANYFS_WRITE_REPLY reply; 
  req.rawhdr.code=ANYFS_CMD_WRITE;
  req.rawhdr.pid=g_my_pid;
  req.handle=fs_handle;
  req.max_size=Length;
  req.options=0;
  LARGE_INTEGER CurOffset;
  if(ByteOffset)
   {   
   req.offset_hi=CurOffset.HighPart=ByteOffset->HighPart;
   req.offset_low=CurOffset.LowPart=ByteOffset->LowPart;
   }else
   {   
   CurOffset.QuadPart=FtpFsWrappers::GetHandlePos(FileHandle);   
   req.offset_hi=CurOffset.HighPart;
   req.offset_low=CurOffset.LowPart;
   }
    
  if(FtpFsWrappers::SendFS(&req,sizeof(req))
     &&FtpFsWrappers::SendFS(Buffer,Length)
     &&FtpFsWrappers::RecvFS(&reply,sizeof(reply)))
   {
   IoStatusBlock->Information=reply.len;
   IoStatusBlock->Status=reply.nt_status;
   FtpFsWrappers::SetHandlePos(FileHandle,CurOffset.QuadPart+(unsigned __int64)reply.len);
   }else IoStatusBlock->Status=STATUS_DATA_ERROR;

  NTSTATUS out=IoStatusBlock->Status;
  if(Event)ZwSetEvent(Event,NULL);
  return out;
  }else
  {
  SectionsEnsureContent((ULONG_PTR)Buffer,Length);
  }

 return realZwWriteFile(FileHandle,Event,ApcRoutine,ApcContext,
  IoStatusBlock,Buffer,Length,ByteOffset,Key);
 }
///////////
NTSTATUS __stdcall myZwFlushBuffersFile(IN HANDLE FileHandle,OUT PIO_STATUS_BLOCK IoStatusBlock)
{
int fs_handle=FtpFsWrappers::FindFsHandle(FileHandle);
if(fs_handle!=-1)return IoStatusBlock->Status=STATUS_SUCCESS;
return ((NTSTATUS(__stdcall*)(HANDLE,PIO_STATUS_BLOCK))realZwFlushBuffersFile)(FileHandle,IoStatusBlock);
}

///////////
NTSTATUS __stdcall myZwCancelIoFile(IN HANDLE FileHandle,OUT PIO_STATUS_BLOCK IoStatusBlock)
{
int fs_handle=FtpFsWrappers::FindFsHandle(FileHandle);
if(fs_handle!=-1)return IoStatusBlock->Status=STATUS_SUCCESS;
return ((NTSTATUS(__stdcall*)(HANDLE,PIO_STATUS_BLOCK))realZwCancelIoFile)(FileHandle,IoStatusBlock);
}
///////////

NTSTATUS __stdcall myZwLockFile(IN HANDLE FileHandle,IN HANDLE Event OPTIONAL,
                                IN PIO_APC_ROUTINE ApcRoutine OPTIONAL,IN PVOID ApcContext OPTIONAL,
                                OUT PIO_STATUS_BLOCK IoStatusBlock,IN PULARGE_INTEGER LockOffset,
                                IN PULARGE_INTEGER LockLength,IN ULONG Key,
                                IN BOOLEAN FailImmediately,IN BOOLEAN ExclusiveLock)
{
int fs_handle=FtpFsWrappers::FindFsHandle(FileHandle);
if(fs_handle!=-1)return IoStatusBlock->Status=STATUS_SUCCESS;//STATUS_ACCESS_DENIED;
return ((NTSTATUS(__stdcall*)(HANDLE,HANDLE,PIO_APC_ROUTINE,PVOID,
                                PIO_STATUS_BLOCK,PULARGE_INTEGER,
                                PULARGE_INTEGER,ULONG,BOOLEAN,BOOLEAN))realZwLockFile)
                                (FileHandle,Event,ApcRoutine,ApcContext,
                                IoStatusBlock,LockOffset,LockLength,Key,
                                FailImmediately,ExclusiveLock);
}
//////////////////////////////////////
NTSTATUS __stdcall myZwUnlockFile(IN HANDLE FileHandle,OUT PIO_STATUS_BLOCK IoStatusBlock,
                                  IN PULARGE_INTEGER LockOffset,IN PULARGE_INTEGER LockLength,IN ULONG Key)
{
int fs_handle=FtpFsWrappers::FindFsHandle(FileHandle);
if(fs_handle!=-1)return IoStatusBlock->Status=STATUS_SUCCESS;//STATUS_ACCESS_DENIED;
return ((NTSTATUS(__stdcall*)(HANDLE,PIO_STATUS_BLOCK,PULARGE_INTEGER,
                            PULARGE_INTEGER,ULONG))realZwUnlockFile)
                            (FileHandle,IoStatusBlock,LockOffset,
                            LockLength,Key);
}

//////////////////////////////////////
NTSTATUS __stdcall myZwDuplicateObject(IN HANDLE SourceProcessHandle,IN HANDLE SourceHandle,
                                      IN HANDLE TargetProcessHandle,OUT PHANDLE TargetHandle OPTIONAL,
                                      IN ACCESS_MASK DesiredAccess,IN ULONG Attributes,IN ULONG Options)
 {
 if(FtpFsWrappers::IncRefWrapperHandle(SourceHandle))
  {
  *TargetHandle=SourceHandle;
  return STATUS_SUCCESS;
  }

 return ((NTSTATUS(__stdcall*)(HANDLE,HANDLE,HANDLE,PHANDLE,ACCESS_MASK,ULONG,ULONG))
  realZwDuplicateObject)(SourceProcessHandle,SourceHandle,TargetProcessHandle,TargetHandle,DesiredAccess,Attributes,Options);
 }

NTSTATUS __stdcall myZwFsControlFile(
                                   IN HANDLE FileHandle,
                                   IN HANDLE Event OPTIONAL,
                                   IN PIO_APC_ROUTINE ApcRoutine OPTIONAL,
                                   IN PVOID ApcContext OPTIONAL,
                                   OUT PIO_STATUS_BLOCK IoStatusBlock,
                                   IN ULONG FsControlCode,
                                   IN PVOID InputBuffer OPTIONAL,
                                   IN ULONG InputBufferLength,
                                   OUT PVOID OutputBuffer OPTIONAL,
                                   IN ULONG OutputBufferLength
                                   )
 {
 int fs_handle=FtpFsWrappers::FindFsHandle(FileHandle);
 if(fs_handle!=-1)
  {
  return STATUS_NOT_SUPPORTED;
  }
 
 return realZwFsControlFile(FileHandle,Event,ApcRoutine,ApcContext,IoStatusBlock,FsControlCode,InputBuffer,InputBufferLength,OutputBuffer,OutputBufferLength);
 }

NTSTATUS __stdcall myZwDeviceIoControlFile(IN HANDLE FileHandle,
										 IN HANDLE Event OPTIONAL,
										 IN PIO_APC_ROUTINE ApcRoutine OPTIONAL,
										 IN PVOID ApcContext OPTIONAL,
										 OUT PIO_STATUS_BLOCK IoStatusBlock,
										 IN ULONG IoControlCode,
										 IN PVOID InputBuffer OPTIONAL,
										 IN ULONG InputBufferLength,
										 OUT PVOID OutputBuffer OPTIONAL,
										 IN ULONG OutputBufferLength
										 )
{
 int fs_handle=FtpFsWrappers::FindFsHandle(FileHandle);
 if(fs_handle!=-1)
  {
  return STATUS_NOT_SUPPORTED;
  }
 
 return realZwDeviceIoControlFile(FileHandle,Event,ApcRoutine,ApcContext,IoStatusBlock,IoControlCode,InputBuffer,InputBufferLength,OutputBuffer,OutputBufferLength);
}


void  NtDllHookData()
 {
 realZwClose = (realZwClose_t)DetourByPtr((PBYTE)ZwClose,(PBYTE)&myZwClose);
 ///
 realZwCreateFile = (realZwCreateFile_t)DetourByPtr((PBYTE)ZwCreateFile,(PBYTE)&myZwCreateFile);
 realZwOpenFile = (realZwOpenFile_t)DetourByPtr((PBYTE)ZwOpenFile,(PBYTE)&myZwOpenFile);
 
 realZwReadFile= (realZwReadFile_t)DetourByPtr((PBYTE)ZwReadFile,(PBYTE)&myZwReadFile);  
 realZwWriteFile = (realZwWriteFile_t)DetourByPtr((PBYTE)ZwWriteFile,(PBYTE)&myZwWriteFile); 
 realZwFlushBuffersFile = DetourByPtr((PBYTE)ZwFlushBuffersFile,(PBYTE)&myZwFlushBuffersFile); 
 realZwCancelIoFile = DetourByPtr((PBYTE)ZwCancelIoFile,(PBYTE)&myZwCancelIoFile); 
 realZwLockFile = DetourByPtr((PBYTE)ZwLockFile,(PBYTE)&myZwLockFile); 
 realZwUnlockFile = DetourByPtr((PBYTE)ZwUnlockFile,(PBYTE)&myZwUnlockFile);  
 realZwDuplicateObject = DetourByPtr((PBYTE)ZwDuplicateObject,(PBYTE)&myZwDuplicateObject);  
 realZwFsControlFile = (realZwFsControlFile_t)DetourByPtr((PBYTE)ZwFsControlFile,(PBYTE)&myZwFsControlFile);  
 realZwDeviceIoControlFile = (realZwDeviceIoControlFile_t)DetourByPtr((PBYTE)ZwDeviceIoControlFile,(PBYTE)&myZwDeviceIoControlFile);  
 
 }

}

