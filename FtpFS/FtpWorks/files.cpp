#include "stdafx.h"
#include "winsock.h"
#include "process.h"
#include "../TrafficCounter.h"
#include "../../FtpSettings/settings.h"
#include "../../FtpFS/ntdefs.h"
#include "../../APIHooks/ntapi.h"
#include "../../AnyFS/AnyFS.h"
#include "../../AnyFS/SomeUsefull.h"
#include "../../DebugLog/DebugLog.h"
#include "ftpworks.h"

namespace FtpWorks
 {
 typedef std::map<HANDLE,FtpFilePtr>    FILESMAP;
 FILESMAP         files_map; 
 FastCriticalSection files_cs;
 
 void CloseDataSocket(FtpFilePtr file)
 {
 if (file->changed && file->sc_data)
  {
	 if (file->sc_cmd)
		file->sc_cmd->flush();
	 file->sc_data->set_nolinger(false);
	 file->sc_data.reset();
	 if (file->sc_cmd)
	 {
		MyWriteDebugLog("CloseDataSocket: wait transfer confirmation...");
		DWORD resp = RecvResponse(file,file->sc_cmd);
		MyWriteDebugLog("CloseDataSocket: transfer confirmed: %u",resp);
	 }
  }
 else
 {
	 file->sc_data.reset();
 }
 //closesocket(file->sc_data);
 
 }

 void SuspendFileIterator(FILESMAP::iterator &i)
  {
  if (i->second->sc_data)
   {
   CloseDataSocket(i->second);
   }
  if (i->second->sc_cmd)
   {   
   MyWriteDebugLog("SuspendFileIterator CloseCleanFtpConnectionsCache %u",
	   i->second->sc_cmd.get());
   CloseCleanFtpConnectionsCache(i->second);//closesocket(i->second.sc_cmd);
   i->second->sc_cmd.reset();
   }   
  i->second->SuspendAt=0;
  }

 void SuspendNotActiveFiles()
 {
	 MyWriteDebugLog("SuspendNotActiveFiles...");  
	 AutoCriticalSection cslock(files_cs);
	 DWORD tm=::GetTickCount();
	 for(FILESMAP::iterator i=files_map.begin();i!=files_map.end();++i)  
		if((!i->second->busy_cnt)&&(i->second->SuspendAt<tm))
		 {
			 SuspendFileIterator(i);
		 }
  }

 void ForceSuspendNotActiveFilesForIP(DWORD ip, DWORD port)
 {
	 MyWriteDebugLog("ForceSuspendNotActiveFilesForIP ip=%x port=%u...",ip,port);  
	 AutoCriticalSection cslock(files_cs);
	 DWORD tm=GetTickCount();
	 for(FILESMAP::iterator i=files_map.begin();i!=files_map.end();++i)  
		 if(!i->second->busy_cnt && i->second->ip==ip && i->second->port==port)
		 {
			 SuspendFileIterator(i);
		 }
 }

 HANDLE PutOurFile(FtpFilePtr file)
  {  
  file->busy_cnt=0;
  HANDLE out = AllocateHandle();
  AutoCriticalSection cslock(files_cs);
  files_map.insert(FILESMAP::value_type(out,file));
  return out;
  }
 
 bool TryCloseOurFile(HANDLE hFile, bool &needwait)
 {
	 AutoCriticalSection cslock(files_cs);
	 FILESMAP::iterator i=files_map.find(hFile);
	 if (i==files_map.end())
		 return false;
	 if (!i->second->busy_cnt)
	 {
		 if (i->second->sc_data)CloseDataSocket(i->second);
		 if (i->second->sc_cmd)
		 {
			 MyWriteDebugLog("CloseOurFile CloseCleanFtpConnectionsCache %u",i->second->sc_cmd);
			 CloseCleanFtpConnectionsCache(i->second);//closesocket(i->second.sc_cmd); 
		 }     
		 files_map.erase(i); 
		 DeAllocateHandle(hFile);
		 needwait=false;
	 }else 
	 {
		 needwait=true;
	 }
	 return true;
 }

 bool CloseOurFile(HANDLE hFile)
  {
  bool out;
  for(;;)
   {
   bool needwait=false;
   out = TryCloseOurFile(hFile, needwait);
   if (!needwait)break;
   ::Sleep(10);
   } 
  if (!out)
    {
    MyWriteDebugLog("CloseOurFile: Handle %x already closed!",hFile);
    //MessageBox(0,"zz","zz2",0);
    }
  return out;
  }
 /*
 bool ApplyFileSettings(HANDLE hFile,FtpFilePtr file)
  {
  EnterCriticalSection(&files_cs);
  FILESMAP::iterator i=files_map.find(hFile);
  bool out=i!=files_map.end();
  if(out)
   {
   file->busy_cnt=i->second.busy_cnt;
   i->second=file;
   }
  LeaveCriticalSection(&files_cs);
  return out;
  }
 */
 FileLocker::FileLocker(HANDLE hFile,FtpFilePtr &file)
  {
  files_cs.Enter();
  FILESMAP::iterator i=files_map.find(hFile);
  LockSucceed=i!=files_map.end();
  if (LockSucceed)
   {   
   i->second->SuspendAt=GetTickCount()+(1000*FtpSettings::idle_timeout);
   i->second->busy_cnt=i->second->busy_cnt+1;   
   file=i->second;
   _hFile=hFile;    
   }
  files_cs.Leave();
  if (LockSucceed)file->cs.Enter();
  }

 FileLocker::~FileLocker()
  {
  if(LockSucceed)
   {
   AutoCriticalSection cslock(files_cs);
   FILESMAP::iterator i=files_map.find(_hFile);
   if (i!=files_map.end())
    {
    i->second->busy_cnt=i->second->busy_cnt-1;    
    i->second->cs.Leave();
    }
   }
  }
 
 }
