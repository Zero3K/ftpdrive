#include "stdafx.h"
#include "tlhelp32.h"
#include "../FtpFS/ntdefs.h"
#include "../AnyFS/AnyFS.h"
#include "shellapi.h"
#include "ftpfswrappers.h"
#include "hookimpl.h"
#include "detours/src/detours.h"
#include "HookSettings.h"
#include "ntapi.h"
#ifdef DBG_LOG
#include "../DebugLog/DebugLog.h"
#endif

POBJECT_ATTRIBUTES NameInitObjectAttributes(const wchar_t *objname, size_t namelen)
 {
 if(namelen==std::wstring::npos)
  namelen=(wcslen(objname)+1)*sizeof(WCHAR);

 PCHAR buf=(PCHAR)malloc(sizeof(OBJECT_ATTRIBUTES)+sizeof(UNICODE_STRING)+namelen);
 POBJECT_ATTRIBUTES out=(POBJECT_ATTRIBUTES)buf;
 out->Length=sizeof(OBJECT_ATTRIBUTES);
 out->RootDirectory=NULL;//tmplt?tmplt->RootDirectory:NULL;
 out->Attributes=OBJ_OPENIF|OBJ_CASE_INSENSITIVE;//tmplt?tmplt->Attributes:OBJ_OPENIF|OBJ_CASE_INSENSITIVE;
 out->SecurityDescriptor=NULL;//tmplt?tmplt->SecurityDescriptor:NULL;
 out->SecurityQualityOfService=NULL;//tmplt?tmplt->SecurityQualityOfService:NULL;
 out->ObjectName=(PUNICODE_STRING)(buf+sizeof(OBJECT_ATTRIBUTES));
 out->ObjectName->Buffer=(PWCHAR)(buf+sizeof(OBJECT_ATTRIBUTES)+sizeof(UNICODE_STRING));
 out->ObjectName->MaximumLength=out->ObjectName->Length=namelen-sizeof(WCHAR);
 memcpy(out->ObjectName->Buffer,objname,namelen);
 return out;
 }

namespace HookImpl
{
/*
typedef struct REAL_CODE_
 {
 PBYTE pbTrampoline;
 PBYTE pbDetour;
 //unsigned char oldCode[32];
 }REAL_CODE;
*/
typedef std::map<PVOID,PVOID> REAL_CODE_MAP;

REAL_CODE_MAP _real_code_map;

void *DetourByPtr(PBYTE realFn,PBYTE newFn)
{
void *out = (void*)DetourFunction(realFn,newFn);
if(_real_code_map.find(realFn)==_real_code_map.end())
 _real_code_map.insert(REAL_CODE_MAP::value_type(out,newFn));
return out;
}

void *DetourByName(HINSTANCE m,char *realFuncName,PBYTE newFn)
{
void *out;
PBYTE realFn=(PBYTE)GetProcAddress(m,realFuncName);
#ifdef DBG_LOG
MyWriteDebugLog("go Detour for %ws",realFuncName);
#endif
if(realFn)
 {
 out = DetourByPtr(realFn,newFn);
 }else
 {
 out = NULL;
#ifdef DBG_LOG
 MyWriteDebugLog("Function %ws not found",realFuncName); 
#endif
 }
return out;
}

void  UnhookAll()
{
for(REAL_CODE_MAP::iterator i=_real_code_map.begin();i!=_real_code_map.end();++i)
 {
 DetourRemove((PBYTE)i->first,(PBYTE)i->second);
 //DWORD dw;
 //WriteProcessMemory(GetCurrentProcess(),i->first,i->second.oldCode,sizeof(REAL_CODE),&dw);
 }
_real_code_map.clear();
}

HINSTANCE GetLibInstance(const TCHAR *dllname)
 {
 HINSTANCE out=(HINSTANCE)GetModuleHandle(dllname);
 if(!out) out= (HINSTANCE)LoadLibrary(dllname);
 return out;
 }
void  HookUnhookAll(bool hook)
{
typedef HANDLE (WINAPI *CreateToolhelp32Snapshotx)(DWORD,DWORD);
typedef DWORD (WINAPI *Thread32Firstx)(HANDLE,LPTHREADENTRY32);
typedef DWORD (WINAPI *Thread32Nextx)(HANDLE,LPTHREADENTRY32);
typedef HANDLE (WINAPI *OpenThreadx)(DWORD,DWORD,DWORD);
HANDLEVECTOR trd_list;

HINSTANCE m_ke=GetLibInstance(TEXT("kernel32.dll"));

CreateToolhelp32Snapshotx extCreateToolhelp32Snapshot=(CreateToolhelp32Snapshotx)GetProcAddress(m_ke,"CreateToolhelp32Snapshot");
Thread32Firstx extThread32First=(Thread32Firstx)GetProcAddress(m_ke,"Thread32First");
Thread32Nextx extThread32Next=(Thread32Nextx)GetProcAddress(m_ke,"Thread32Next");
OpenThreadx extOpenThread=(OpenThreadx)GetProcAddress(m_ke,"OpenThread");
if(m_ke&&extCreateToolhelp32Snapshot&&extThread32First&&extThread32Next&&extOpenThread)
 {
 DWORD pid=g_my_pid,tid=GetCurrentThreadId();
 HANDLE hs=extCreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,pid);
 if(hs!=INVALID_HANDLE_VALUE)
  {
  THREADENTRY32 te;ZeroMemory(&te,sizeof(te));te.dwSize=sizeof(te);
  if(extThread32First(hs,&te))
  do
   {
   if((te.th32ThreadID!=tid)&&(te.th32OwnerProcessID==pid))
    {
    HANDLE htrd=extOpenThread(THREAD_ALL_ACCESS,0,te.th32ThreadID);
    if(htrd)
     {
     trd_list.push_back(htrd);
     SuspendThread(htrd);
     }
    }
   }while(extThread32Next(hs,&te));
  CloseHandle(hs);
  }
 }

if(hook)
 {
 realZwClose=&ZwClose;
 NtDllHookInfo();  
 NtDllHookSections();
 NtDllHookData(); 
 HiLevelHook();
 FileCopyHook();
 }else 
 {
 UnhookAll();
 //realZwClose=&ZwClose;
 }

for(HANDLEVECTOR::iterator i=trd_list.begin();i!=trd_list.end();++i)
 {
 ResumeThread(*i);
 CloseHandle(*i);
 }
}
////////////////////////////////////
void  InitApiHooks()
{
FtpFsWrappers::Init();
HookUnhookAll(true);
}
void  DeinitApiHooks()
{
NtDllHookSectionsDeinit();
HookUnhookAll(false);
}

/////////////
}

