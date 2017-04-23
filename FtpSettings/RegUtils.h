namespace RegUtils
 {
 void __fastcall SetIntReg(HKEY ky,TCHAR *valname,int val);
 int __fastcall GetIntReg(HKEY ky,TCHAR *valname,int def);

 void __fastcall DlgLoadTextReg(HKEY ky,TCHAR *valname,TCHAR *def, HWND hwnd,int id);
 void __fastcall DlgSaveTextReg(HKEY ky,TCHAR *valname,HWND hwnd,int id);
 void __fastcall DlgLoadIntReg(HKEY ky,TCHAR *valname,int def, HWND hwnd,int id);
 void __fastcall DlgSaveIntReg(HKEY ky,TCHAR *valname,HWND hwnd,int id);
 
 }