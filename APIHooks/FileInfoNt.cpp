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
void *realZwQueryInformationProcess;
void *realZwQueryInformationFile;
void *realZwSetInformationFile;
void *realZwQueryDirectoryFile;
void *realZwQueryAttributesFile;
void *realZwQueryFullAttributesFile;
void *realZwQueryVolumeInformationFile;
void *realZwSetVolumeInformationFile;
void *realZwDeleteFile;
void *realZwQueryEaFile;
void *realZwSetEaFile;
void *realZwNotifyChangeDirectoryFile;
void *realLdrLoadDll;
//////////////////////////////////////

static bool app_path_killcopy = false;
static bool app_path_explorer = false;
static bool app_path_ftpdrive = false;
static bool app_path_avp = false;
static bool app_path_windbg = false;
static TCHAR app_path[MAX_PATH+1] = {0};

void EnsureAppPathAnalyzed()
{
	if (!app_path[0])
	{
		::GetModuleFileName(NULL,&app_path[0],MAX_PATH);
		if (!app_path[0])
		{
			app_path[0]=1;
			app_path[1]=0;
			return;
		}
		app_path[MAX_PATH]=0;
		wcslwr(&app_path[0]);
		app_path_windbg = (wcsstr(&app_path[0],TEXT("\\windbg.exe"))!=NULL); 
		app_path_explorer = (wcsstr(&app_path[0],TEXT("\\explorer.exe"))!=NULL);
		app_path_killcopy = (wcsstr(&app_path[0],TEXT("\\killcopy.exe"))!=NULL);
		app_path_ftpdrive = (wcsstr(&app_path[0],TEXT("\\ftpdrive.exe"))!=NULL);
		app_path_avp = (wcsstr(&app_path[0],TEXT("\\avp.exe"))!=NULL);
		if (!app_path_avp)
			app_path_avp = (wcsstr(&app_path[0],TEXT("\\kav.exe"))!=NULL);
		if (!app_path_avp)
			app_path_avp = (wcsstr(&app_path[0],TEXT("\\kis.exe"))!=NULL);
	}
}

bool IsThisExplorer()
{
	EnsureAppPathAnalyzed();
	return app_path_explorer;
}

bool IsThisKillCopy()
{
	EnsureAppPathAnalyzed();
	return app_path_killcopy;
}

bool IsThisFtpDrive()
{
	EnsureAppPathAnalyzed();
	return app_path_ftpdrive;
}

bool IsThisAVP()
{
	EnsureAppPathAnalyzed();
	return app_path_avp;
}

bool IsThisWinDbg()
{
	EnsureAppPathAnalyzed();
	return app_path_windbg;
}

NTSTATUS __stdcall myZwQueryInformationProcess(HANDLE ProcessHandle,PROCESSINFOCLASS ProcessInformationClass,PVOID ProcessInformation,ULONG ProcessInformationLength,PULONG ReturnLength)
 { 
 NTSTATUS rv=((NTSTATUS(__stdcall*)(HANDLE,PROCESSINFOCLASS,PVOID,ULONG,PULONG))realZwQueryInformationProcess)(ProcessHandle,ProcessInformationClass,ProcessInformation,ProcessInformationLength,ReturnLength);
 if((ProcessInformationClass==ProcessDeviceMap)&&(rv==STATUS_SUCCESS))
  {  
  if(hhook&&HookSettings::IsThisAppAffected()&&(FtpFsDrive[0]>='A')&&(FtpFsDrive[0]<='Z'))
   {
   PPROCESS_DEVICEMAP_INFORMATION devmap=(PPROCESS_DEVICEMAP_INFORMATION)ProcessInformation;
   DWORD bitmask=1;
   for(char ch='A';ch!=FtpFsDrive[0];ch++)bitmask<<=1;
   devmap->Query.DriveMap|=bitmask;
   //devmap->Query.DriveType[FtpFsDrive[0]-'A']=DRIVE_REMOTE;//DRIVE_REMOTE:DRIVE_REMOVABLE;//;//;DRIVE_FIXED
   devmap->Query.DriveType[FtpFsDrive[0]-'A']=IsThisExplorer()?DRIVE_REMOTE:DRIVE_REMOVABLE;//DRIVE_REMOTE:DRIVE_REMOVABLE;//;//;DRIVE_FIXED
   } 
  }
 return rv;
 }
//////////////////////////////////////

unsigned int  FillFileInfo_BASIC(LPANYFS_INFO_REPLY reply,PFILE_BASIC_INFORMATION info,unsigned int len,PIO_STATUS_BLOCK IoStatusBlock)
 { 
 if(len>=sizeof(FILE_BASIC_INFORMATION))
  {
  info->CreationTime=reply->cr_time;
  info->LastAccessTime=reply->ac_time;
  info->LastWriteTime=reply->wr_time;
  info->ChangeTime=reply->wr_time;
  info->FileAttributes=reply->attributes;
  IoStatusBlock->Information=sizeof(FILE_BASIC_INFORMATION);
  IoStatusBlock->Status=STATUS_SUCCESS;
  }else IoStatusBlock->Status=STATUS_BUFFER_OVERFLOW; 
 return IoStatusBlock->Status;
 }

unsigned int  FillFileInfo_STANDARD(LPANYFS_INFO_REPLY reply,PFILE_STANDARD_INFORMATION info,unsigned int len,PIO_STATUS_BLOCK IoStatusBlock)
 { 
 //MyWriteDebugLog("FillFileInfo_STANDARD need %u get %u",sizeof(FILE_STANDARD_INFORMATION),len);
 if(len>=sizeof(FILE_STANDARD_INFORMATION))
  {
  info->AllocationSize=reply->size;
  info->EndOfFile=reply->size;
  info->NumberOfLinks=1;
  info->DeletePending=FALSE; 
  info->Directory=reply->attributes&FILE_ATTRIBUTE_DIRECTORY;
  IoStatusBlock->Information=sizeof(FILE_STANDARD_INFORMATION);
  IoStatusBlock->Status=STATUS_SUCCESS;
  }else IoStatusBlock->Status=STATUS_BUFFER_OVERFLOW;  
 return IoStatusBlock->Status;
 }


unsigned int  FillFileInfo_INTERNAL(LPANYFS_INFO_REPLY reply,PFILE_INTERNAL_INFORMATION info,unsigned int len,PIO_STATUS_BLOCK IoStatusBlock)
 { 
 if(len>=sizeof(FILE_INTERNAL_INFORMATION))
  {
  info->FileId.QuadPart=0; 
  IoStatusBlock->Information=sizeof(FILE_INTERNAL_INFORMATION);
  IoStatusBlock->Status=STATUS_SUCCESS;
  }else IoStatusBlock->Status=STATUS_BUFFER_OVERFLOW;   
 return IoStatusBlock->Status;
 }
unsigned int  FillFileInfo_EA(LPANYFS_INFO_REPLY reply,PFILE_EA_INFORMATION info,unsigned int len,PIO_STATUS_BLOCK IoStatusBlock)
 {
 if(len>=sizeof(FILE_EA_INFORMATION))
  {
  info->EaInformationLength=0;
  IoStatusBlock->Information=sizeof(FILE_EA_INFORMATION);
  IoStatusBlock->Status=STATUS_SUCCESS;
  }else IoStatusBlock->Status=STATUS_BUFFER_OVERFLOW;   
 return IoStatusBlock->Status;
 }
unsigned int  FillFileInfo_ACCESS(LPANYFS_INFO_REPLY reply,PFILE_ACCESS_INFORMATION info,unsigned int len,PIO_STATUS_BLOCK IoStatusBlock)
 {
 if(len>=sizeof(FILE_ACCESS_INFORMATION))
  {
  info->GrantedAccess=GENERIC_READ|GENERIC_EXECUTE;
  IoStatusBlock->Information=sizeof(FILE_ACCESS_INFORMATION);
  IoStatusBlock->Status=STATUS_SUCCESS;
  }else IoStatusBlock->Status=STATUS_BUFFER_OVERFLOW;   
 return IoStatusBlock->Status;
 }
unsigned int  FillFileInfo_POSITION(HANDLE FileHandle,PFILE_POSITION_INFORMATION info,unsigned int len,PIO_STATUS_BLOCK IoStatusBlock)
 {
 if(len>=sizeof(FILE_POSITION_INFORMATION))
  {
  info->CurrentByteOffset.QuadPart=FtpFsWrappers::GetHandlePos(FileHandle);
  IoStatusBlock->Information=sizeof(FILE_POSITION_INFORMATION);
  IoStatusBlock->Status=STATUS_SUCCESS;
  }else IoStatusBlock->Status=STATUS_BUFFER_OVERFLOW;   
 return IoStatusBlock->Status;
 }
unsigned int  FillFileInfo_MODE(PFILE_MODE_INFORMATION info,unsigned int len,PIO_STATUS_BLOCK IoStatusBlock)
 {
 if(len>=sizeof(FILE_MODE_INFORMATION))
  {
  info->Mode=0;
  IoStatusBlock->Information=sizeof(FILE_MODE_INFORMATION);
  IoStatusBlock->Status=STATUS_SUCCESS;
  }else IoStatusBlock->Status=STATUS_BUFFER_OVERFLOW;   
 return IoStatusBlock->Status;
 }
unsigned int  FillFileInfo_ALIGNMENT(PFILE_ALIGNMENT_INFORMATION info,unsigned int len,PIO_STATUS_BLOCK IoStatusBlock)
 {
 if(len>=sizeof(FILE_ALIGNMENT_INFORMATION))
  {
  info->AlignmentRequirement=FILE_BYTE_ALIGNMENT;
  IoStatusBlock->Information=sizeof(FILE_ALIGNMENT_INFORMATION);
  IoStatusBlock->Status=STATUS_SUCCESS;
  }else IoStatusBlock->Status=STATUS_BUFFER_OVERFLOW;   
 return IoStatusBlock->Status;
 }
unsigned int  FillFileInfo_NAME(LPANYFS_INFO_REPLY reply,PFILE_NAME_INFORMATION info,unsigned int len,PIO_STATUS_BLOCK IoStatusBlock)
 {
 reply->file_name[ANYFS_MAX_PATH-1]=0;
 unsigned int namelen=wcslen(reply->file_name);
 if(len>=(sizeof(ULONG)+sizeof(WCHAR)*(1+namelen)))
  {
  info->FileNameLength=namelen*sizeof(WCHAR);
  memcpy(reply->file_name,info->FileName,namelen*sizeof(WCHAR));
  //MultiByteToWideChar(CP_ACP,0,reply->file_name,namelen,info->FileName,namelen);  
  IoStatusBlock->Information=(sizeof(ULONG)+sizeof(WCHAR)*namelen);
  IoStatusBlock->Status=STATUS_SUCCESS;
  }else IoStatusBlock->Status=STATUS_BUFFER_OVERFLOW;   
 return IoStatusBlock->Status;
 }

unsigned int  FillFileInfo_ALL(LPANYFS_INFO_REPLY reply,HANDLE FileHandle,PFILE_ALL_INFORMATION info,unsigned int len,PIO_STATUS_BLOCK IoStatusBlock)
 {   
 IO_STATUS_BLOCK isb={0};
 
 unsigned int remainlen=len;

 IoStatusBlock->Status=FillFileInfo_BASIC(reply,&info->BasicInformation,remainlen,&isb); 
 remainlen-=isb.Information;isb.Information=0; 

 NTSTATUS rv=FillFileInfo_STANDARD(reply,&info->StandardInformation,remainlen,&isb);   
 remainlen-=isb.Information;isb.Information=0;
 if(IoStatusBlock->Status==STATUS_SUCCESS)IoStatusBlock->Status=rv;

 rv=FillFileInfo_INTERNAL(reply,&info->InternalInformation,remainlen,&isb);
 remainlen-=isb.Information;isb.Information=0;
 if(IoStatusBlock->Status==STATUS_SUCCESS)IoStatusBlock->Status=rv;

 rv=FillFileInfo_EA(reply,&info->EaInformation,remainlen,&isb);
 remainlen-=isb.Information;isb.Information=0;
 if(IoStatusBlock->Status==STATUS_SUCCESS)IoStatusBlock->Status=rv;

 rv=FillFileInfo_ACCESS(reply,&info->AccessInformation,remainlen,&isb);
 remainlen-=isb.Information;isb.Information=0;
 if(IoStatusBlock->Status==STATUS_SUCCESS)IoStatusBlock->Status=rv;

 rv=FillFileInfo_POSITION(FileHandle,&info->PositionInformation,remainlen,&isb);
 remainlen-=isb.Information;isb.Information=0;
 if(IoStatusBlock->Status==STATUS_SUCCESS)IoStatusBlock->Status=rv;

 rv=FillFileInfo_MODE(&info->ModeInformation,remainlen,&isb);
 remainlen-=isb.Information;isb.Information=0;
 if(IoStatusBlock->Status==STATUS_SUCCESS)IoStatusBlock->Status=rv;

 rv=FillFileInfo_ALIGNMENT(&info->AlignmentInformation,remainlen,&isb);
 remainlen-=isb.Information;isb.Information=0;
 if(IoStatusBlock->Status==STATUS_SUCCESS)IoStatusBlock->Status=rv;

 rv=FillFileInfo_NAME(reply,&info->NameInformation,remainlen,&isb);
 remainlen-=isb.Information;isb.Information=0;
 if(IoStatusBlock->Status==STATUS_SUCCESS)IoStatusBlock->Status=rv;
 
 IoStatusBlock->Information=len-remainlen;
 
 return IoStatusBlock->Status;
 }

unsigned int  FillFileInfo_ATTRIBUTETAG(LPANYFS_INFO_REPLY reply,HANDLE FileHandle,PFILE_ATTRIBUTE_TAG_INFORMATION info,unsigned int len,PIO_STATUS_BLOCK IoStatusBlock)
 {
 if(len>=(sizeof(FILE_ATTRIBUTE_TAG_INFORMATION)))
  {
  info->FileAttributes=reply->attributes;
  info->ReparseTag=0;
  IoStatusBlock->Information=(sizeof(FILE_ATTRIBUTE_TAG_INFORMATION));
  IoStatusBlock->Status=STATUS_SUCCESS;
  }else IoStatusBlock->Status=STATUS_BUFFER_OVERFLOW;   
 return IoStatusBlock->Status;
 }
unsigned int FillFileInfo_NETWORKOPEN(LPANYFS_INFO_REPLY reply,PFILE_NETWORK_OPEN_INFORMATION info,unsigned int len,PIO_STATUS_BLOCK IoStatusBlock)
 {
 if(len>=(sizeof(FILE_NETWORK_OPEN_INFORMATION)))
  {  
  info->CreationTime=reply->cr_time;
  info->LastAccessTime=reply->ac_time;
  info->LastWriteTime=reply->wr_time;
  info->ChangeTime=reply->wr_time;
  info->AllocationSize=reply->size;
  info->EndOfFile=reply->size;
  info->FileAttributes=reply->attributes;
  IoStatusBlock->Information=(sizeof(FILE_NETWORK_OPEN_INFORMATION));
  IoStatusBlock->Status=STATUS_SUCCESS;
  }else IoStatusBlock->Status=STATUS_BUFFER_OVERFLOW;   
 return IoStatusBlock->Status;
 }

NTSTATUS __stdcall myZwQueryInformationFile(HANDLE FileHandle,PIO_STATUS_BLOCK IoStatusBlock,
                                            PVOID FileInformation,ULONG FileInformationLength,FILE_INFORMATION_CLASS FileInformationClass)
 {
 
 int fs_handle=FtpFsWrappers::FindFsHandle(FileHandle);
 if(fs_handle!=-1)
  { 
  
  ANYFS_INFO_REQUEST req;
  ANYFS_INFO_REPLY reply; 
  req.rawhdr.code=ANYFS_CMD_INFO;
  req.rawhdr.pid=g_my_pid;
  req.handle=fs_handle;
    
  if(FtpFsWrappers::SendRecvFS(&req,sizeof(req), &reply,sizeof(reply)))     
   {
#ifdef DBG_LOG
   MyWriteDebugLog("myZwQueryInformationFile name=%ws class=%u len=%u", reply.file_name,FileInformationClass,FileInformationLength);
#endif
   switch(FileInformationClass)
    {
    case FileBasicInformation:return FillFileInfo_BASIC(&reply,(PFILE_BASIC_INFORMATION)FileInformation,FileInformationLength,IoStatusBlock);
    case FileStandardInformation:return FillFileInfo_STANDARD(&reply,(PFILE_STANDARD_INFORMATION)FileInformation,FileInformationLength,IoStatusBlock);
    case FileInternalInformation:return FillFileInfo_INTERNAL(&reply,(PFILE_INTERNAL_INFORMATION)FileInformation,FileInformationLength,IoStatusBlock);
    case FileEaInformation:return FillFileInfo_EA(&reply,(PFILE_EA_INFORMATION)FileInformation,FileInformationLength,IoStatusBlock);
    case FileAccessInformation:return FillFileInfo_ACCESS(&reply,(PFILE_ACCESS_INFORMATION)FileInformation,FileInformationLength,IoStatusBlock);
    case FileNameInformation:return FillFileInfo_NAME(&reply,(PFILE_NAME_INFORMATION)FileInformation,FileInformationLength,IoStatusBlock);
    case FilePositionInformation:return FillFileInfo_POSITION(FileHandle,(PFILE_POSITION_INFORMATION)FileInformation,FileInformationLength,IoStatusBlock);
    case FileModeInformation:return FillFileInfo_POSITION(&reply,(PFILE_POSITION_INFORMATION)FileInformation,FileInformationLength,IoStatusBlock);
    case FileAlignmentInformation:return FillFileInfo_ALIGNMENT((PFILE_ALIGNMENT_INFORMATION)FileInformation,FileInformationLength,IoStatusBlock);
    case FileAllInformation:return FillFileInfo_ALL(&reply,FileHandle,(PFILE_ALL_INFORMATION)FileInformation,FileInformationLength,IoStatusBlock);
    case FileAttributeTagInformation:return FillFileInfo_ATTRIBUTETAG(&reply,FileHandle,(PFILE_ATTRIBUTE_TAG_INFORMATION)FileInformation,FileInformationLength,IoStatusBlock);
    case FileNetworkOpenInformation:return FillFileInfo_NETWORKOPEN(&reply,(PFILE_NETWORK_OPEN_INFORMATION)FileInformation,FileInformationLength,IoStatusBlock);
    default: 
#ifdef DBG_LOG
     MyWriteDebugLog("NOT_IMPLEMENTED: myZwQueryInformationFile name=%ws class=%u", reply.file_name,FileInformationClass);
#endif
     return IoStatusBlock->Status=STATUS_NOT_SUPPORTED;
    }
   }
  
  }
 return ((NTSTATUS(__stdcall*)(HANDLE,PIO_STATUS_BLOCK,PVOID,ULONG,FILE_INFORMATION_CLASS))realZwQueryInformationFile)(FileHandle,IoStatusBlock,FileInformation,FileInformationLength,FileInformationClass);
 }
//////////////////////////////////////
NTSTATUS RenameOrDeleteFile(unsigned int fs_handle, const std::wstring &NewName=std::wstring())
 {
 ANYFS_RENAME_REQUEST req;
 ANYFS_RENAME_REPLY reply; 
 req.rawhdr.code=ANYFS_CMD_RENAME;
 req.rawhdr.pid=g_my_pid;
 req.handle=fs_handle;
 wcsncpy(req.filename,NewName.c_str(),ANYFS_MAX_PATH);
 
 if(FtpFsWrappers::SendRecvFS(&req,sizeof(req), &reply,sizeof(reply)))     
  return reply.nt_status;
 
 return STATUS_DATA_ERROR;
 }

NTSTATUS __stdcall myZwSetInformationFile(HANDLE FileHandle,PIO_STATUS_BLOCK IoStatusBlock,
                                          PVOID FileInformation,ULONG FileInformationLength,FILE_INFORMATION_CLASS FileInformationClass)
 {
 int fs_handle=FtpFsWrappers::FindFsHandle(FileHandle);
 if(fs_handle!=-1)
  {  
  switch(FileInformationClass)
   {
   case FilePositionInformation:
    {     
    FtpFsWrappers::SetHandlePos(FileHandle,((PFILE_POSITION_INFORMATION)FileInformation)->CurrentByteOffset.QuadPart);
    IoStatusBlock->Information=sizeof(FILE_POSITION_INFORMATION);     
    return IoStatusBlock->Status=STATUS_SUCCESS;
    }
   case FileDispositionInformation:
    {
    if( ((PFILE_DISPOSITION_INFORMATION)FileInformation)->DeleteFile)
     {
     IoStatusBlock->Status=RenameOrDeleteFile(fs_handle);
     return IoStatusBlock->Status;
     }
    return IoStatusBlock->Status=STATUS_SUCCESS;
    }
   case FileRenameInformation:
    {
    PFILE_RENAME_INFORMATION info=(PFILE_RENAME_INFORMATION)FileInformation;
    std::wstring pathname;
    if(info->RootDirectory)
     {
     int root=FtpFsWrappers::FindFsHandle(info->RootDirectory);
     if(root!=-1)
      pathname=GetPathByHandle(root);
     if(pathname.empty())
      return IoStatusBlock->Status=STATUS_NOT_SAME_DEVICE;
     }
    if(info->FileNameLength)
     pathname.append(info->FileName,info->FileNameLength/sizeof(WCHAR));
    IoStatusBlock->Status=RenameOrDeleteFile(fs_handle,pathname);

    return IoStatusBlock->Status;
    }
   default: 
    {
#ifdef DBG_LOG    
    MyWriteDebugLog("ACCESS_DENIED: myZwSetInformationFile class=%u", FileInformationClass);
#endif
    IoStatusBlock->Information=0;
    return IoStatusBlock->Status=STATUS_NOT_SUPPORTED;//STATUS_ACCESS_DENIED
    }
   }
  
  }
 return ((NTSTATUS(__stdcall*)(HANDLE,PIO_STATUS_BLOCK,PVOID,ULONG,FILE_INFORMATION_CLASS))realZwSetInformationFile)(FileHandle,IoStatusBlock,FileInformation,FileInformationLength,FileInformationClass);
 }
//////////////////////////////////////
NTSTATUS __stdcall myZwQueryVolumeInformationFile(IN HANDLE FileHandle,OUT PIO_STATUS_BLOCK IoStatusBlock,
                                                  OUT PVOID VolumeInformation,IN ULONG VolumeInformationLength,
                                                  IN FS_INFORMATION_CLASS VolumeInformationClass)
 {
 int fs_handle=FtpFsWrappers::FindFsHandle(FileHandle);
 if(fs_handle!=-1)
  {  
  switch(VolumeInformationClass)
   {
   case FileFsVolumeInformation:
    {
    PFILE_FS_VOLUME_INFORMATION info=(PFILE_FS_VOLUME_INFORMATION)VolumeInformation;
    info->VolumeCreationTime.QuadPart=0;
    info->VolumeSerialNumber=814;
    info->VolumeLabelLength=6;
    info->Unknown=0;
    if(VolumeInformationLength>=(sizeof(FILE_FS_VOLUME_INFORMATION)+6))
     {
     memcpy(info->VolumeLabel,L"FTP",8);
     IoStatusBlock->Status=STATUS_SUCCESS;
     IoStatusBlock->Information=sizeof(FILE_FS_VOLUME_INFORMATION)+4;
     }else
     {
     IoStatusBlock->Status=STATUS_BUFFER_OVERFLOW;
     IoStatusBlock->Information=VolumeInformationLength;
     }
    }break;
   case FileFsLabelInformation:
    {
    PFILE_FS_LABEL_INFORMATION info=(PFILE_FS_LABEL_INFORMATION)VolumeInformation;     
    if(VolumeInformationLength>=(sizeof(FILE_FS_LABEL_INFORMATION)+6))
     {
     info->VolumeLabelLength=6;
     memcpy(&info->VolumeLabel,L"FTP",8);
     IoStatusBlock->Status=STATUS_SUCCESS;
     IoStatusBlock->Information=sizeof(FILE_FS_LABEL_INFORMATION)+4;
     }else
     {
     IoStatusBlock->Status=STATUS_BUFFER_OVERFLOW;
     IoStatusBlock->Information=VolumeInformationLength;
     }
    }break;
   case FileFsSizeInformation:
    {
    PFILE_FS_SIZE_INFORMATION info=(PFILE_FS_SIZE_INFORMATION)VolumeInformation;
    info->TotalAllocationUnits.QuadPart=1024*1024;
    info->AvailableAllocationUnits.QuadPart=512*1024;
    info->SectorsPerAllocationUnit=8;
    info->BytesPerSector=512;
    IoStatusBlock->Status=STATUS_SUCCESS;
    IoStatusBlock->Information=sizeof(FILE_FS_SIZE_INFORMATION);
    }break;
   case FileFsDeviceInformation:
    {
    PFILE_FS_DEVICE_INFORMATION info=(PFILE_FS_DEVICE_INFORMATION)VolumeInformation;
    info->DeviceType=FILE_DEVICE_DISK;//FILE_DEVICE_CD_ROM;//FILE_DEVICE_DISK;//
    info->Characteristics=FILE_REMOTE_DEVICE|FILE_DEVICE_IS_MOUNTED;///:FILE_READ_ONLY_DEVICE|FILE_REMOVABLE_MEDIA|FILE_DEVICE_IS_MOUNTED;////FILE_READ_ONLY_DEVICE|
    IoStatusBlock->Status=STATUS_SUCCESS;
    IoStatusBlock->Information=sizeof(FILE_FS_DEVICE_INFORMATION);
    }break;
   case FileFsAttributeInformation:
    {
    PFILE_FS_ATTRIBUTE_INFORMATION info=(PFILE_FS_ATTRIBUTE_INFORMATION)VolumeInformation;
    info->FileSystemFlags=FILE_CASE_SENSITIVE_SEARCH|FILE_CASE_PRESERVED_NAMES;
    info->MaximumComponentNameLength=ANYFS_MAX_PATH-1;
    info->FileSystemNameLength=6;     
    if(VolumeInformationLength>=(sizeof(FILE_FS_ATTRIBUTE_INFORMATION)+6))
     {      
     memcpy(info->FileSystemName,L"FTP",8);
     IoStatusBlock->Status=STATUS_SUCCESS;
     IoStatusBlock->Information=sizeof(FILE_FS_ATTRIBUTE_INFORMATION)+4;
     }else
     {
     IoStatusBlock->Status=STATUS_BUFFER_OVERFLOW;
     IoStatusBlock->Information=VolumeInformationLength;
     }
    }break;
   case FileFsControlInformation:
    {     
    memset(VolumeInformation,0,sizeof(FILE_FS_CONTROL_INFORMATION));
    IoStatusBlock->Status=STATUS_SUCCESS;
    IoStatusBlock->Information=sizeof(FILE_FS_CONTROL_INFORMATION);
    }break;
   case FileFsFullSizeInformation:
    {
    PFILE_FS_FULL_SIZE_INFORMATION info=(PFILE_FS_FULL_SIZE_INFORMATION)VolumeInformation;
    info->TotalQuotaAllocationUnits.QuadPart=1024*1024;
    info->AvailableQuotaAllocationUnits.QuadPart=512*1024;
    info->AvailableAllocationUnits.QuadPart=512*1024;
    info->SectorsPerAllocationUnit=8;
    info->BytesPerSector=512;
    IoStatusBlock->Status=STATUS_SUCCESS;
    IoStatusBlock->Information=sizeof(FILE_FS_FULL_SIZE_INFORMATION);
    }break;
   case FileFsObjectIdInformation:
    {
    memset(VolumeInformation,0,sizeof(FILE_FS_OBJECT_ID_INFORMATION));
    IoStatusBlock->Status=STATUS_SUCCESS;
    IoStatusBlock->Information=sizeof(FILE_FS_OBJECT_ID_INFORMATION);
    }break;
   default:
    IoStatusBlock->Status=STATUS_NOT_SUPPORTED;
   }
  return IoStatusBlock->Status;
  }
 return ((NTSTATUS(__stdcall*)(HANDLE,PIO_STATUS_BLOCK,PVOID,ULONG,FS_INFORMATION_CLASS)
  )realZwQueryVolumeInformationFile)(FileHandle,IoStatusBlock,VolumeInformation,VolumeInformationLength,VolumeInformationClass);
 }
NTSTATUS __stdcall myZwSetVolumeInformationFile(IN HANDLE FileHandle,OUT PIO_STATUS_BLOCK IoStatusBlock,
                                                IN PVOID Buffer,IN ULONG BufferLength,
                                                IN FS_INFORMATION_CLASS VolumeInformationClass)
 {
 int fs_handle=FtpFsWrappers::FindFsHandle(FileHandle);
 if(fs_handle!=-1)
  {  
#ifdef DBG_LOG  
  MyWriteDebugLog("ACCESS_DENIED: myZwSetVolumeInformationFile");
#endif
  return IoStatusBlock->Status=STATUS_ACCESS_DENIED;
  }
  
 return ((NTSTATUS(__stdcall*)(HANDLE,PIO_STATUS_BLOCK,PVOID,ULONG,FS_INFORMATION_CLASS)
  )realZwSetVolumeInformationFile)(FileHandle,IoStatusBlock,Buffer,BufferLength,VolumeInformationClass); 
 }
//////////////////////////////////////
NTSTATUS __stdcall myZwQueryAttributesFile(IN POBJECT_ATTRIBUTES ObjectAttributes,OUT PFILE_BASIC_INFORMATION FileInformation)
 { 
 if(hhook&&HookSettings::IsThisAffected(ObjectAttributes))
  {
  HANDLE FileHandle;
  IO_STATUS_BLOCK isb;
  NTSTATUS rv=WrapperOpenFile(&FileHandle,GENERIC_READ,ObjectAttributes,&isb,FILE_OPEN,0);
  if(rv==STATUS_SUCCESS)
   {
   rv=myZwQueryInformationFile(FileHandle,&isb,FileInformation,sizeof(FILE_BASIC_INFORMATION),FileBasicInformation);
   FtpFsWrappers::CloseWrapperHandle(FileHandle);   
   }
  return rv;
  } 
 return ((NTSTATUS(__stdcall*)(POBJECT_ATTRIBUTES,PFILE_BASIC_INFORMATION)
  )realZwQueryAttributesFile)(ObjectAttributes,FileInformation); 
 }

NTSTATUS __stdcall myZwQueryFullAttributesFile(IN POBJECT_ATTRIBUTES ObjectAttributes,OUT PFILE_NETWORK_OPEN_INFORMATION FileInformation)
 {
 if(hhook&&HookSettings::IsThisAffected(ObjectAttributes))
  {  
  HANDLE FileHandle;
  IO_STATUS_BLOCK isb;
  NTSTATUS rv=WrapperOpenFile(&FileHandle,GENERIC_READ,ObjectAttributes,&isb,FILE_OPEN,0);
  if(rv==STATUS_SUCCESS)
   {
   rv=myZwQueryInformationFile(FileHandle,&isb,FileInformation,sizeof(FILE_NETWORK_OPEN_INFORMATION),FileNetworkOpenInformation);
   FtpFsWrappers::CloseWrapperHandle(FileHandle);   
   }
  return rv;
  }

 return ((NTSTATUS(__stdcall*)(POBJECT_ATTRIBUTES,PFILE_NETWORK_OPEN_INFORMATION)
  )realZwQueryFullAttributesFile)(ObjectAttributes,FileInformation); 
 }
NTSTATUS __stdcall myZwDeleteFile(IN POBJECT_ATTRIBUTES ObjectAttributes)
 {
 if(hhook&&HookSettings::IsThisAffected(ObjectAttributes))
  {
#ifdef DBG_LOG  
  MyWriteDebugLog("ACCESS_DENIED: myZwDeleteFile");
#endif
  return STATUS_ACCESS_DENIED;
  }

 return ((NTSTATUS(__stdcall*)(POBJECT_ATTRIBUTES))realZwDeleteFile)(ObjectAttributes); 
 }

//////////////////////////////////////
//////////////////////////////////////
NTSTATUS FillDirectoryInfo(const ANYFS_INFO_REPLY &reply,OUT PVOID FileInformation,
						   IN ULONG FileInformationLength,
						   IN FILE_INFORMATION_CLASS FileInformationClass, ULONG &RetLen)
{
	size_t namelen = wcslen(reply.file_name);
	unsigned int needsize = namelen*sizeof(WCHAR);
	RetLen = 0;

	switch(FileInformationClass)
    {
	case FileDirectoryInformation:
		{
			if(FileInformationLength<sizeof(FILE_DIRECTORY_INFORMATION))
				return STATUS_INFO_LENGTH_MISMATCH;

			PFILE_DIRECTORY_INFORMATION p=(PFILE_DIRECTORY_INFORMATION)FileInformation;
			p->NextEntryOffset=0;
			p->Unknown=0;
			p->CreationTime=reply.cr_time;
			p->LastAccessTime=reply.ac_time;
			p->LastWriteTime=reply.wr_time;
			p->ChangeTime=reply.wr_time;
			p->EndOfFile=reply.size;
			p->AllocationSize=reply.size;
			p->FileAttributes=reply.attributes;
			p->FileNameLength=needsize;
			
			needsize += sizeof(FILE_BOTH_DIRECTORY_INFORMATION);
			if (FileInformationLength<needsize)
			{
				RetLen = sizeof(FILE_BOTH_DIRECTORY_INFORMATION);
				return STATUS_BUFFER_OVERFLOW;
			}

			memcpy(p->FileName,reply.file_name,p->FileNameLength+sizeof(WCHAR));
			RetLen = needsize;
			return STATUS_SUCCESS;
		}
		
	case FileFullDirectoryInformation:
		{
			if(FileInformationLength<sizeof(FILE_FULL_DIRECTORY_INFORMATION))
				return STATUS_INFO_LENGTH_MISMATCH;

			PFILE_FULL_DIRECTORY_INFORMATION p=(PFILE_FULL_DIRECTORY_INFORMATION)FileInformation;
			p->NextEntryOffset=0;
			p->Unknown=0;
			p->CreationTime=reply.cr_time;
			p->LastAccessTime=reply.ac_time;
			p->LastWriteTime=reply.wr_time;
			p->ChangeTime=reply.wr_time;
			p->EndOfFile=reply.size;
			p->AllocationSize=reply.size;
			p->FileAttributes=reply.attributes;
			p->FileNameLength=needsize;
			p->EaInformationLength=0;

			needsize += sizeof(FILE_FULL_DIRECTORY_INFORMATION);
			if (FileInformationLength<needsize)
			{
				RetLen = sizeof(FILE_FULL_DIRECTORY_INFORMATION);
				return STATUS_BUFFER_OVERFLOW;
			}

			memcpy(p->FileName,reply.file_name,p->FileNameLength+sizeof(WCHAR));
			RetLen = needsize; 
			return STATUS_SUCCESS;
		}
		
    case FileBothDirectoryInformation:
		{
			if (FileInformationLength<sizeof(FILE_BOTH_DIRECTORY_INFORMATION))
				return STATUS_INFO_LENGTH_MISMATCH;
			
			PFILE_BOTH_DIRECTORY_INFORMATION info=(PFILE_BOTH_DIRECTORY_INFORMATION)FileInformation;
			info->NextEntryOffset=0;
			info->Unknown=0;
			info->CreationTime=reply.cr_time;
			info->LastAccessTime=reply.ac_time;
			info->LastWriteTime=reply.wr_time;
			info->ChangeTime=reply.wr_time;
			info->EndOfFile=reply.size;
			info->AllocationSize=reply.size;
			info->FileAttributes=reply.attributes;
			info->EaInformationLength=0;
			info->FileNameLength=needsize;
			info->AlternateNameLength=needsize>24?24:(UCHAR)needsize;
			
			needsize += sizeof(FILE_BOTH_DIRECTORY_INFORMATION);
			if (FileInformationLength<needsize)
			{
				RetLen = sizeof(FILE_BOTH_DIRECTORY_INFORMATION);
				return STATUS_BUFFER_OVERFLOW;
			}

			memcpy(info->FileName,reply.file_name,info->FileNameLength+sizeof(WCHAR));
			memcpy(info->AlternateName,reply.file_name,
				info->AlternateNameLength>=24?24:info->AlternateNameLength+sizeof(WCHAR));

			RetLen = needsize; 
			return STATUS_SUCCESS;
		}
		
	case FileIdBothDirectoryInformation:
		{
			if (FileInformationLength<sizeof(FILE_ID_BOTH_DIRECTORY_INFORMATION))
				return STATUS_INFO_LENGTH_MISMATCH;
			
			PFILE_ID_BOTH_DIRECTORY_INFORMATION info=(PFILE_ID_BOTH_DIRECTORY_INFORMATION)FileInformation;
			info->NextEntryOffset=0;
			info->FileIndex=0;
			info->FileId.QuadPart=0;
			info->CreationTime=reply.cr_time;
			info->LastAccessTime=reply.ac_time;
			info->LastWriteTime=reply.wr_time;
			info->ChangeTime=reply.wr_time;
			info->EndOfFile=reply.size;
			info->AllocationSize=reply.size;
			info->FileAttributes=reply.attributes;
			info->EaInformationLength=0;
			info->FileNameLength=needsize;
			info->AlternateNameLength=needsize>24?24:(UCHAR)needsize;
			
			needsize += sizeof(FILE_ID_BOTH_DIRECTORY_INFORMATION);
			if (FileInformationLength<needsize)
			{
				RetLen = sizeof(FILE_ID_BOTH_DIRECTORY_INFORMATION);
				return STATUS_BUFFER_OVERFLOW;
			}

			memcpy(info->FileName,reply.file_name,info->FileNameLength+sizeof(WCHAR));
			memcpy(info->AlternateName,reply.file_name,
				info->AlternateNameLength>=24?24:info->AlternateNameLength+sizeof(WCHAR));

			RetLen = needsize; 
			return STATUS_SUCCESS;
		}
		
    default:
#ifdef DBG_LOG     
		MyWriteDebugLog("NOT_IMPLEMENTED: myZwQueryDirectoryFile class %u",FileInformationClass);
#else
		;
#endif
		return STATUS_NOT_SUPPORTED;
    }
}

NTSTATUS __stdcall myZwQueryDirectoryFile(IN HANDLE FileHandle,IN HANDLE Event OPTIONAL,IN PIO_APC_ROUTINE ApcRoutine OPTIONAL,
                                          IN PVOID ApcContext OPTIONAL,OUT PIO_STATUS_BLOCK IoStatusBlock,OUT PVOID FileInformation,
                                          IN ULONG FileInformationLength,IN FILE_INFORMATION_CLASS FileInformationClass,
                                          IN BOOLEAN ReturnSingleEntry,IN PUNICODE_STRING FileName OPTIONAL,IN BOOLEAN RestartScan)
{ 
	int fs_handle=FtpFsWrappers::FindFsHandle(FileHandle);
	if(fs_handle==-1)
		return ((NTSTATUS(__stdcall*)(HANDLE,HANDLE,PIO_APC_ROUTINE,PVOID,PIO_STATUS_BLOCK,PVOID,ULONG,FILE_INFORMATION_CLASS,BOOLEAN,PUNICODE_STRING,BOOLEAN)
		)realZwQueryDirectoryFile)(FileHandle,Event,ApcRoutine,ApcContext,IoStatusBlock,FileInformation,FileInformationLength,FileInformationClass,ReturnSingleEntry,FileName,RestartScan); 
	
	
	std::wstring fmask;
	if(FileName&&(FileName->Length>=sizeof(WCHAR)))
	{
		fmask.assign(FileName->Buffer,FileName->Length/sizeof(WCHAR));
		// size_t masklen=FileName->Length/2;
		//std::vector<TCHAR> tmp(1+masklen);
		//WideCharToMultiByte(CP_ACP,0,FileName->Buffer,masklen,&tmp[0],tmp.size(),NULL,NULL);
		for(size_t i=0,j=fmask.size();i<j;i++)
		{
			switch(fmask[i])
			{
			case L'<':fmask[i]=L'*';break;
			case L'"':fmask[i]=L'.';break;
			case L'>':fmask[i]=L'?';break;
			}
		}
		//fmask.assign(&tmp[0],masklen);
		FtpFsWrappers::SetDirMask(FileHandle,fmask);
#ifdef DBG_LOG   
		MyWriteDebugLog("myZwQueryDirectoryFile for mask %ws",fmask.c_str());
#endif
	}else
	{
		fmask=FtpFsWrappers::GetDirMask(FileHandle);
	}
	ANYFS_LIST_REQUEST req;
	ANYFS_INFO_REPLY reply; 
	req.rawhdr.code=ANYFS_CMD_LIST;
	req.rawhdr.pid = g_my_pid;
	req.handle=fs_handle;
	if(RestartScan)
	{   
		req.index=0;
		FtpFsWrappers::SetHandlePos(FileHandle,0);
	}else
	{
		req.index=(unsigned int)FtpFsWrappers::GetHandlePos(FileHandle);
	}
	
	IoStatusBlock->Information=0;
	IoStatusBlock->Status=STATUS_NO_MORE_FILES;  
	
	bool matched=false;
	
	for(;;)
	{
		if (!FtpFsWrappers::SendRecvFS(&req,sizeof(req), &reply,sizeof(reply)))reply.nt_status=STATUS_DATA_ERROR;
		if (reply.nt_status!=STATUS_SUCCESS)break;
		if (fmask.empty()||PathMatchSpec(reply.file_name,fmask.c_str()))
		{
			matched=true;
			break;   
		}
		FtpFsWrappers::SetHandlePos(FileHandle,++req.index);
	}
	
	if (matched&&(reply.nt_status==STATUS_SUCCESS))
	{
		reply.file_name[ANYFS_MAX_PATH-1]=0;
		IoStatusBlock->Status = FillDirectoryInfo(reply, FileInformation, 
			FileInformationLength, FileInformationClass, IoStatusBlock->Information);
	}
	if(IoStatusBlock->Status==STATUS_SUCCESS)
		FtpFsWrappers::SetHandlePos(FileHandle,++req.index);  
#ifdef DBG_LOG  
	MyWriteDebugLog("myZwQueryDirectoryFile handle=%x mask='%ws' status=%x",(DWORD)FileHandle,fmask.c_str(),IoStatusBlock->Status);   
#endif
	if(Event)ZwSetEvent(Event,NULL);
	return IoStatusBlock->Status;
	
}
//////////////////////////////////////
NTSTATUS __stdcall myZwQueryEaFile(IN HANDLE FileHandle,OUT PIO_STATUS_BLOCK IoStatusBlock,
                                OUT PFILE_FULL_EA_INFORMATION Buffer,IN ULONG BufferLength,
                                IN BOOLEAN ReturnSingleEntry,IN PFILE_GET_EA_INFORMATION EaList OPTIONAL,
                                IN ULONG EaListLength,IN PULONG EaIndex OPTIONAL,IN BOOLEAN RestartScan)
 {
 int fs_handle=FtpFsWrappers::FindFsHandle(FileHandle);
 if(fs_handle!=-1)
  {  
  IoStatusBlock->Information=0;
#ifdef DBG_LOG  
  MyWriteDebugLog("myZwQueryEaFile handle='%x'",FileHandle);
#endif
  return IoStatusBlock->Status=STATUS_NOT_SUPPORTED;
  }

 return ((NTSTATUS(__stdcall*)(HANDLE,PIO_STATUS_BLOCK,PFILE_FULL_EA_INFORMATION,ULONG,
                                BOOLEAN,PFILE_GET_EA_INFORMATION,ULONG,PULONG,BOOLEAN)
  )realZwQueryEaFile)(FileHandle,IoStatusBlock,Buffer,BufferLength,ReturnSingleEntry,EaList,EaListLength,EaIndex,RestartScan); 
 }
//////////////////////////////////////
NTSTATUS __stdcall myZwSetEaFile(IN HANDLE FileHandle,OUT PIO_STATUS_BLOCK IoStatusBlock,
                                OUT PFILE_FULL_EA_INFORMATION Buffer,IN ULONG BufferLength)
 {
 int fs_handle=FtpFsWrappers::FindFsHandle(FileHandle);
 if(fs_handle!=-1)
  {  
  IoStatusBlock->Information=0;
#ifdef DBG_LOG  
  MyWriteDebugLog("myZwSetEaFile handle='%x'",FileHandle);
#endif
  return IoStatusBlock->Status=STATUS_NOT_SUPPORTED;
  }

 return ((NTSTATUS(__stdcall*)(HANDLE,PIO_STATUS_BLOCK,PFILE_FULL_EA_INFORMATION,ULONG)
  )realZwSetEaFile)(FileHandle,IoStatusBlock,Buffer,BufferLength); 
 }
//////////////////////////////////////

NTSTATUS __stdcall myZwNotifyChangeDirectoryFile(IN HANDLE FileHandle,IN HANDLE Event OPTIONAL,IN PIO_APC_ROUTINE ApcRoutine OPTIONAL,
                                                 IN PVOID ApcContext OPTIONAL,OUT PIO_STATUS_BLOCK IoStatusBlock,OUT PFILE_NOTIFY_INFORMATION Buffer,
                                                 IN ULONG BufferLength,IN ULONG NotifyFilter,IN BOOLEAN WatchSubtree)
 {
 int fs_handle=FtpFsWrappers::FindFsHandle(FileHandle);
 if(fs_handle!=-1)
  {  
  IoStatusBlock->Information=0;
#ifdef DBG_LOG  
  MyWriteDebugLog("myZwNotifyChangeDirectoryFile handle='%x' ApcRoutine=%x",FileHandle,(DWORD)ApcRoutine);
#endif
  //if(Event)ZwSetEvent(Event,NULL);
  return IoStatusBlock->Status=STATUS_NOT_SUPPORTED;//STATUS_NO_MORE_FILES;//STATUS_SUCCESS;
  }

 return ((NTSTATUS(__stdcall*)(HANDLE,HANDLE,PIO_APC_ROUTINE,PVOID,PIO_STATUS_BLOCK,PFILE_NOTIFY_INFORMATION,ULONG,ULONG,BOOLEAN)
  )realZwNotifyChangeDirectoryFile)(FileHandle,Event,ApcRoutine,ApcContext,IoStatusBlock,Buffer,BufferLength,NotifyFilter,WatchSubtree); 
 }

NTSTATUS __stdcall myLdrLoadDll (IN PWSTR DllPath OPTIONAL,
                                 IN PULONG DllCharacteristics OPTIONAL,
                                 IN PUNICODE_STRING DllName,OUT PVOID *DllHandle)
 {
 WCHAR tmpbuf[MAX_PATH]={0};
 GetCurrentDirectoryW(MAX_PATH,tmpbuf);
 if(tmpbuf[0]&&HookSettings::IsOurRootDrive(tmpbuf,2))
    SetCurrentDirectoryW(L"C:\\");
 else
    tmpbuf[0]=0;
 NTSTATUS out=((NTSTATUS(__stdcall*)(PWSTR,PULONG,PUNICODE_STRING,PVOID *)
                )realLdrLoadDll)(DllPath,DllCharacteristics,DllName,DllHandle); 
 if(tmpbuf[0])SetCurrentDirectoryW(tmpbuf);
 return out;
 }


void  NtDllHookInfo()
 { 
 realZwQueryInformationProcess = DetourByPtr((PBYTE)ZwQueryInformationProcess,(PBYTE)&myZwQueryInformationProcess);
 realZwQueryInformationFile = DetourByPtr((PBYTE)ZwQueryInformationFile,(PBYTE)&myZwQueryInformationFile);
 realZwSetInformationFile = DetourByPtr((PBYTE)ZwSetInformationFile,(PBYTE)&myZwSetInformationFile);
 realZwQueryDirectoryFile = DetourByPtr((PBYTE)ZwQueryDirectoryFile,(PBYTE)&myZwQueryDirectoryFile);
 realZwQueryAttributesFile = DetourByPtr((PBYTE)ZwQueryAttributesFile,(PBYTE)&myZwQueryAttributesFile);
 realZwQueryFullAttributesFile = DetourByPtr((PBYTE)ZwQueryFullAttributesFile,(PBYTE)&myZwQueryFullAttributesFile); 
 realZwQueryEaFile = DetourByPtr((PBYTE)ZwQueryEaFile,(PBYTE)&myZwQueryEaFile); 
 realZwSetEaFile = DetourByPtr((PBYTE)ZwSetEaFile,(PBYTE)&myZwSetEaFile); 
 realZwNotifyChangeDirectoryFile = DetourByPtr((PBYTE)ZwNotifyChangeDirectoryFile,(PBYTE)&myZwNotifyChangeDirectoryFile);  
 realZwQueryVolumeInformationFile = DetourByPtr((PBYTE)ZwQueryVolumeInformationFile,(PBYTE)&myZwQueryVolumeInformationFile); 
 realZwSetVolumeInformationFile = DetourByPtr((PBYTE)ZwSetVolumeInformationFile,(PBYTE)&myZwSetVolumeInformationFile); 
 realZwDeleteFile = DetourByPtr((PBYTE)ZwDeleteFile,(PBYTE)&myZwDeleteFile);   
 realLdrLoadDll = DetourByPtr((PBYTE)LdrLoadDll,(PBYTE)&myLdrLoadDll);
 }
}

