#include "windows.h"
#include "regutils.h"

namespace RegUtils
 {
 void __fastcall SetIntReg(HKEY ky,TCHAR *valname,int val)
  {
  RegSetValueEx(ky,valname,NULL,REG_DWORD,(LPBYTE)&val,sizeof(DWORD));
  }
 
 int __fastcall GetIntReg(HKEY ky,TCHAR *valname,int def)
  {
  int out=def,buff;
  DWORD tip=REG_NONE,datalen=sizeof(DWORD);
  if(RegQueryValueEx(ky,valname,NULL,&tip,(LPBYTE)&buff,&datalen)==ERROR_SUCCESS)
   {
   if((tip==REG_DWORD)&&datalen)out=buff;
   }
  return out;
  }
 
 void __fastcall DlgLoadTextReg(HKEY ky,TCHAR *valname,TCHAR *def, HWND hwnd,int id)
  {
  TCHAR *buff=(TCHAR *)malloc(0x10000*sizeof(TCHAR));
  bool ok=false;
  wcscpy(buff,def);
  DWORD tip=REG_NONE,datalen=0xffff*sizeof(TCHAR);
  if(RegQueryValueEx(ky,valname,NULL,&tip,(LPBYTE)buff,&datalen)==ERROR_SUCCESS)
   { 
   if((tip==REG_SZ)&&datalen)
    {
    buff[datalen/sizeof(TCHAR)]=0;
    ok=1;  
    }
   }
  SetDlgItemText(hwnd,id,ok?buff:def);
  free(buff); 
  }
 
 void __fastcall DlgSaveTextReg(HKEY ky,TCHAR *valname,HWND hwnd,int id)
  {
  TCHAR *buff=(TCHAR *)malloc(0x10000*sizeof(TCHAR));
  if(GetDlgItemText(hwnd,id,buff,0xffff))
   {
   buff[0xffff]=0;
   RegSetValueEx(ky,valname,0,REG_SZ,(LPBYTE)buff,(wcslen(buff)+1)*sizeof(wchar_t));
   }
  free(buff);
  }
 
 
 void __fastcall DlgLoadIntReg(HKEY ky,TCHAR *valname,int def, HWND hwnd,int id)
  {
  int val=GetIntReg(ky,valname,def);
  SetDlgItemInt(hwnd,id,val,FALSE);
  }
 void __fastcall DlgSaveIntReg(HKEY ky,TCHAR *valname,HWND hwnd,int id)
  {
  int val=GetDlgItemInt(hwnd,id,NULL,FALSE);
  SetIntReg(ky,valname,val);
  }
 
 }