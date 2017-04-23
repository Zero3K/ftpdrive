#include "StdAfx.h"
#include "Windows.h"
#include "ResHolder.h"
#include "../FtpSettings/settings.h"

void ResHolder::LoadModule(const TCHAR *module)
 {
 TCHAR dll[MAX_PATH+64];
 GetModuleFileName(NULL,dll,MAX_PATH);
 TCHAR *t=dll+wcslen(dll);
 while((t!=dll)&&(*t!='\\'))t--;
 if(*t=='\\')t++;
 wcscpy(t,TEXT("Res\\"));
 wcscat(dll,module);
 wcscat(dll,TEXT(".dll"));
 Module=LoadLibraryEx(dll,NULL,DONT_RESOLVE_DLL_REFERENCES);
 }
ResHolder::ResHolder()
 {
 LoadModule(FtpSettings::interface_lng.c_str());
 if(!Module)LoadModule(TEXT("English"));
 }

ResHolder::~ResHolder()
 {
 if(Module)FreeLibrary(Module);
 }

std::wstring ResHolder::GetString(UINT uID)
 {
 std::vector<TCHAR> buf(1024);
 int x=LoadString(Module,uID,&buf[0],buf.size());
 return std::wstring(&buf[0],x);
 }
