#include "stdafx.h"
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

namespace HookImpl
{
void *realMoveFileWithProgressW;

BOOL __stdcall myMoveFileWithProgressW(
  LPCWSTR lpExistingFileName,
  LPCWSTR lpNewFileName,
  LPPROGRESS_ROUTINE lpProgressRoutine,
  LPVOID lpData,
  DWORD dwFlags
)
 {
 if(!hhook || !HookSettings::IsOurRootDrive(lpExistingFileName) || !lpNewFileName || !HookSettings::IsOurRootDrive(lpNewFileName))
  return ((NTSTATUS(__stdcall*)(LPCWSTR,LPCWSTR,LPPROGRESS_ROUTINE,LPVOID,DWORD))realMoveFileWithProgressW)(lpExistingFileName,lpNewFileName,lpProgressRoutine,lpData,dwFlags); 

 WCHAR curdir[MAX_PATH+2]={0};
 GetCurrentDirectoryW(MAX_PATH,curdir);
 wcscat(curdir,TEXT("\\"));
 
 std::wstring strFrom(lpExistingFileName),strTo(lpNewFileName);
 if(strFrom.find(L':')==std::wstring::npos)
  strFrom.insert(0,curdir);
 if(strTo.find(L':')==std::wstring::npos)
  strTo.insert(0,curdir);

 size_t sl1=strFrom.rfind(L'\\'),sl2=strTo.rfind(L'\\');  
 if( (sl1!=std::wstring::npos)&&(sl2!=std::wstring::npos))
  {
  if(!wcsicmp(strFrom.substr(0,sl1).c_str(),strTo.substr(0,sl2).c_str()))
   return ((NTSTATUS(__stdcall*)(LPCWSTR,LPCWSTR,LPPROGRESS_ROUTINE,LPVOID,DWORD))realMoveFileWithProgressW)(lpExistingFileName,lpNewFileName,lpProgressRoutine,lpData,dwFlags); 
  }


 //MessageBoxA(0,lpProgressRoutine?"zz1":"zz2","zz",0);
 BOOL bCancel=FALSE;
 BOOL out=CopyFileExW(lpExistingFileName,lpNewFileName,lpProgressRoutine,
  lpData,&bCancel,(dwFlags&MOVEFILE_REPLACE_EXISTING)?0:COPY_FILE_FAIL_IF_EXISTS);
 if(out)
  out=DeleteFileW(lpExistingFileName);
 return out;
 }
void FileCopyHook()
 { 
 //realCopyFileExW = DetourByPtr((PBYTE)CopyFileExW,(PBYTE)&myCopyFileExW);
 realMoveFileWithProgressW = DetourByPtr((PBYTE)MoveFileWithProgressW,(PBYTE)&myMoveFileWithProgressW);
 }
}