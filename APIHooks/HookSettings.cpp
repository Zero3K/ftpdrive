#include "stdafx.h"
#include "../AnyFS/AnyFS.h"
#include "../FtpFS/ntdefs.h"
#include "HookSettings.h"
#include "NtDllExports.h"
#include "hookimpl.h"
#include "FtpFsWrappers.h"
#ifdef DBG_LOG
#include "../DebugLog/DebugLog.h"
#endif

namespace HookSettings
 {
 
 bool  IsOurRootDrive(const wchar_t *path,int pathlen)//,bool checkcurdir)
  {
  bool out=0;
  if(path&&pathlen)
   {
   
   if(pathlen==-1)pathlen = wcslen(path);
   if((pathlen==2)&&(path[1]==':')&&
    ((path[0]==FtpFsDrive[0])||(path[0]==FtpFsDrive[1])))
    return true;

   int i=0;
   while((i<pathlen)&&((path[i]=='\\')||(path[i]=='?')))i++;
   if(((i+1)<pathlen)&&path[i]&&(path[i+1]==':')&&
    ((path[i]==FtpFsDrive[0])||(path[i]==FtpFsDrive[1])))
    {
    out=1;
    } 
   }
  return out;
  }
 bool  IsOurRootDrive(POBJECT_ATTRIBUTES ObjectAttributes)
  { 
  bool out; 
  if(ObjectAttributes&&ObjectAttributes->ObjectName)
   {
   if(ObjectAttributes->RootDirectory&&FtpFsWrappers::FindFsHandle(ObjectAttributes->RootDirectory)!=-1)
    out=1;
   else
    out=IsOurRootDrive(ObjectAttributes->ObjectName->Buffer,ObjectAttributes->ObjectName->Length/2);   
   }else out=0;
  return out;
  }
 bool IsOurRootDrive(const char *path)
  {
  bool out=0;
  if(path&&*path)
   {
   int pathlen=strlen(path);
   if((pathlen==2)&&(path[1]==':')&&
    ((path[0]==FtpFsDrive[0])||(path[0]==FtpFsDrive[1])))
    return true;

   int i=0;
   while((i<pathlen)&&((path[i]=='\\')||(path[i]=='?')))i++;
   if(((i+1)<pathlen)&&path[i]&&(path[i+1]==':')&&
    ((path[i]==FtpFsDrive[0])||(path[i]==FtpFsDrive[1])))
    {
    out=1;
    } 
   }
  return out;
  }

 FastCriticalSection AppAffectedCS;
 volatile DWORD AppAffectedCheckExpire = 0;
 volatile bool AppAffectedResult = 0;
 
 bool IsThisAppAffected()
  {
	 DWORD tm = ::GetTickCount();
	 AppAffectedCS.Enter();
	 bool need_reload = (tm>=AppAffectedCheckExpire);
	 bool out = AppAffectedResult;
	 AppAffectedCS.Leave();
	 
	 if (!need_reload)		 
		 return out;
	 
	 LastErrorSaver error_saver;
	 ANYFS_CHECKAPP_REQUEST req;
	 ANYFS_CHECKAPP_REPLY reply; 
	 req.rawhdr.code=ANYFS_CMD_CHECKAPP;
	 req.rawhdr.pid=g_my_pid;
	 
	 GetModuleFileName(NULL,req.filename,ANYFS_MAX_PATH);
	 req.filename[ANYFS_MAX_PATH-1]=0;
	 if(FtpFsWrappers::SendRecvFS(&req,sizeof(req), &reply,sizeof(reply)))     
	 {
		 out = (reply.affected!=FALSE);
	 }
	 else
	 {
		 out = false;
	 }

	 AppAffectedCS.Enter();
	 AppAffectedResult = out;
	 AppAffectedCheckExpire = tm+1000;
	 AppAffectedCS.Leave();	
	 
	 return out;
  }
 
 bool IsThisAffected(POBJECT_ATTRIBUTES ObjectAttributes)
  {
  if(!IsOurRootDrive(ObjectAttributes))return false;
  return IsThisAppAffected();
  }

 }

