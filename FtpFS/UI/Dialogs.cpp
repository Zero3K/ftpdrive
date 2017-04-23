#include "stdafx.h"
#include "shellapi.h"
#include "stdlib.h"
#include "stdio.h"
#include "commctrl.h"
#include "../../Res/English/resource.h"
#include <string>
#include "dialogs.h"
#include "../../FtpSettings/RegUtils.h"
#include "../../FtpSettings/settings.h"
#include "../../FtpSettings/defs.h"
#include "../ErrorHandler.h"
#include "../SrvReplyLog.h"

extern HDESK g_cur_desktop;


void FDUIDialog::LoadComboLng(HWND hwnd)
{
	TCHAR path[MAX_PATH+64]={0};
	GetModuleFileName(NULL,path,MAX_PATH);
	TCHAR *t=path+wcslen(path);
	while((t!=path)&&(*t!='\\'))t--;
	if(*t=='\\')t++;
	wcscpy(t,TEXT("Res\\*.dll"));
	
	WIN32_FIND_DATA wfd={0};
	HANDLE hFile=FindFirstFile(path,&wfd);
	if(hFile!=INVALID_HANDLE_VALUE)
	{
		do {
			std::wstring name(wfd.cFileName);
			size_t p=name.find('.');
			if(p!=std::wstring::npos)name.resize(p);
			SendMessage(hwnd,CB_ADDSTRING,0,(LPARAM)name.c_str());
		} while(FindNextFile(hFile,&wfd));
		SetWindowText(hwnd,FtpSettings::interface_lng.c_str());
		FindClose(hFile);
	}
	
}
int CALLBACK FDUIDialog::TabGeneralDialogProc(HWND hwndDlg,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	FDUIDialog *dlg=(FDUIDialog *)GetWindowLong(hwndDlg,GWL_USERDATA);
	switch(uMsg)
	{
	case WM_INITDIALOG:
		{
			SetWindowLong(hwndDlg,GWL_USERDATA,lParam);
			dlg=(FDUIDialog *)lParam;
			HKEY key=NULL;
			RegCreateKey(HKEY_CURRENT_USER,TEXT("Software\\KILLSOFT\\FTPDrive"),&key);
			if(key)
			{
				TCHAR drv_str[2];drv_str[1]=0;
				drv_str[0]=(char)(unsigned char)(DWORD)RegUtils::GetIntReg(key,TEXT("set_drv_letter"),default_drv_letter);      
				SendDlgItemMessage(hwndDlg,IDC_EDIT7,EM_LIMITTEXT,1,0);
				SetDlgItemText(hwndDlg,IDC_EDIT7,drv_str);   
				DWORD set_flags=(DWORD)RegUtils::GetIntReg(key,TEXT("set_flags"),default_set_flags);
				RegUtils::DlgLoadTextReg(key,TEXT("serv_host"),default_serv_host,hwndDlg,IDC_EDIT1);
				RegUtils::DlgLoadIntReg(key,TEXT("serv_port"),default_serv_port,hwndDlg,IDC_EDIT2);
				RegUtils::DlgLoadTextReg(key,TEXT("serv_user"),default_serv_user,hwndDlg,IDC_EDIT3);
				RegUtils::DlgLoadTextReg(key,TEXT("serv_pass"),default_serv_pass,hwndDlg,IDC_EDIT4);
				RegUtils::DlgLoadIntReg(key,TEXT("serv_refresh"),default_serv_refresh,hwndDlg,IDC_EDIT5);          
				RegUtils::DlgLoadTextReg(key,TEXT("app_list"),default_app_list,hwndDlg,IDC_EDIT6);  
				CheckDlgButton(hwndDlg,IDC_CHECK2,set_flags&FTPS_DEFPASSIVE?BST_CHECKED:BST_UNCHECKED);
				CheckDlgButton(hwndDlg,IDC_CHECK3,set_flags&FTPS_FROMLOCAL?BST_CHECKED:BST_UNCHECKED);
				CheckDlgButton(hwndDlg,IDC_CHECK4,set_flags&FTPS_FROMREMOTE?BST_CHECKED:BST_UNCHECKED);     
				CheckDlgButton(hwndDlg,IDC_RADIO1,set_flags&FTPS_DENIEDAPPS?BST_CHECKED:BST_UNCHECKED);
				CheckDlgButton(hwndDlg,IDC_RADIO2,set_flags&FTPS_DENIEDAPPS?BST_UNCHECKED:BST_CHECKED);
				LoadComboLng(GetDlgItem(hwndDlg,IDC_COMBO_LNG));
				RegCloseKey(key);
			}
			key=NULL;  
			RegCreateKey(HKEY_CURRENT_USER,TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Run"),&key);
			if(key)
			{
				DWORD tip;
				bool autostart=RegQueryValueEx(key,TEXT("FtpDrive"),NULL,&tip,NULL,NULL)==ERROR_SUCCESS;
				CheckDlgButton(hwndDlg,IDC_CHECK1,autostart?BST_CHECKED:BST_UNCHECKED);
				RegCloseKey(key);
			} 
		}break;
	case WM_COMMAND:
		{
			switch(LOWORD(wParam))
			{
			case IDC_EDITLOCALLIST:
				{
					
					std::wstring path(ModPathName);
					int x=path.rfind('\\');
					if(x!=std::wstring::npos)path.resize(x+1);
					path.append(TEXT("FtpServList.txt"))	;
					SitesEditDialog sed(path);
					sed.Show(hwndDlg);
					//path.insert(0,"notepad.exe ");
					//WinExec(path.c_str(),SW_SHOW);
				}break;
			case IDOK:
				{
					HKEY key=NULL;
					RegCreateKey(HKEY_CURRENT_USER,TEXT("Software\\KILLSOFT\\FTPDrive"),&key);
					if(key)
					{
						TCHAR drv_str[4];
						GetDlgItemText(hwndDlg,IDC_EDIT7,drv_str,3);drv_str[3]=0;
						RegUtils::SetIntReg(key,TEXT("set_drv_letter"),(DWORD)(unsigned char)drv_str[0]);      
						
						DWORD set_flags=0;     
						if(IsDlgButtonChecked(hwndDlg,IDC_CHECK2)==BST_CHECKED)set_flags|=FTPS_DEFPASSIVE;
						if(IsDlgButtonChecked(hwndDlg,IDC_CHECK3)==BST_CHECKED)set_flags|=FTPS_FROMLOCAL;
						if(IsDlgButtonChecked(hwndDlg,IDC_CHECK4)==BST_CHECKED)set_flags|=FTPS_FROMREMOTE;
						if(IsDlgButtonChecked(hwndDlg,IDC_RADIO1)==BST_CHECKED)set_flags|=FTPS_DENIEDAPPS;
						RegUtils::SetIntReg(key,TEXT("set_flags"),(int)set_flags);
						
						RegUtils::DlgSaveTextReg(key,TEXT("serv_host"),hwndDlg,IDC_EDIT1);
						RegUtils::DlgSaveIntReg(key,TEXT("serv_port"),hwndDlg,IDC_EDIT2);
						RegUtils::DlgSaveTextReg(key,TEXT("serv_user"),hwndDlg,IDC_EDIT3);
						RegUtils::DlgSaveTextReg(key,TEXT("serv_pass"),hwndDlg,IDC_EDIT4);
						RegUtils::DlgSaveIntReg(key,TEXT("serv_refresh"),hwndDlg,IDC_EDIT5);  
						RegUtils::DlgSaveTextReg(key,TEXT("app_list"),hwndDlg,IDC_EDIT6);
						RegUtils::DlgSaveTextReg(key,TEXT("interface_lng"),hwndDlg,IDC_COMBO_LNG);  
						RegCloseKey(key);
					}
					key=NULL;  
					RegCreateKey(HKEY_CURRENT_USER,TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Run"),&key);
					if(key)
					{
						if(IsDlgButtonChecked(hwndDlg,IDC_CHECK1)==BST_CHECKED)
						{
							TCHAR buf[MAX_PATH+2];
							GetModuleFileName(NULL,buf,MAX_PATH+1);buf[MAX_PATH+1]=0;
							RegSetValueEx(key,TEXT("FtpDrive"),NULL,REG_SZ,(LPBYTE)buf,(wcslen(buf)+1)*sizeof(TCHAR));
						}else RegDeleteValue(key,TEXT("FtpDrive"));
						RegCloseKey(key);
					}
					EndDialog(hwndDlg,LOWORD(wParam));
				}break;
			case IDCANCEL:
				{
					EndDialog(hwndDlg,LOWORD(wParam));
				}break;   
			}
		}break;
	default: return 0;
	}
	return 1;
 }
 
 
 void FDUIDialog::TabAdvancedUpdateState(HWND hwndDlg)
 {
	 BOOL dir_cache=(IsDlgButtonChecked(hwndDlg,IDC_CHECK1)==BST_CHECKED);
	 BOOL file_cache=(IsDlgButtonChecked(hwndDlg,IDC_CHECK2)==BST_CHECKED);
	 BOOL ftp_errctrl=(IsDlgButtonChecked(hwndDlg,IDC_CHECK6)==BST_CHECKED); 
	 EnableWindow(GetDlgItem(hwndDlg,IDC_EDIT1),dir_cache);
	 EnableWindow(GetDlgItem(hwndDlg,IDC_EDIT2),file_cache);
	 EnableWindow(GetDlgItem(hwndDlg,IDC_EDIT3),file_cache);
	 EnableWindow(GetDlgItem(hwndDlg,IDC_EDIT7),ftp_errctrl);
	 EnableWindow(GetDlgItem(hwndDlg,IDC_EDIT8),ftp_errctrl);
 }
 
 int CALLBACK FDUIDialog::TabAdvancedDialogProc(HWND hwndDlg,UINT uMsg,WPARAM wParam,LPARAM lParam)
 {
	 FDUIDialog *dlg=(FDUIDialog *)GetWindowLong(hwndDlg,GWL_USERDATA);
	 switch(uMsg)
	 {
	 case WM_INITDIALOG:
		 {
			 HKEY key=NULL;
			 RegCreateKey(HKEY_CURRENT_USER,TEXT("Software\\KILLSOFT\\FTPDrive"),&key);
			 if(key)
			 {
				 SetWindowLong(hwndDlg,GWL_USERDATA,lParam);
				 dlg=(FDUIDialog *)lParam;
				 DWORD adv_flags=(DWORD)RegUtils::GetIntReg(key,TEXT("adv_flags"),default_adv_flags);
				 DWORD log_level=(DWORD)RegUtils::GetIntReg(key,TEXT("log_level"),0);
				 CheckDlgButton(hwndDlg,IDC_CHECK1,adv_flags&FTPS_ADV_DIRCACHE?BST_CHECKED:BST_UNCHECKED);
				 CheckDlgButton(hwndDlg,IDC_CHECK2,adv_flags&FTPS_ADV_FILECACHE?BST_CHECKED:BST_UNCHECKED);
				 CheckDlgButton(hwndDlg,IDC_CHECK3,adv_flags&FTPS_ADV_USEGUARDPAGES?BST_CHECKED:BST_UNCHECKED);
				 CheckDlgButton(hwndDlg,IDC_CHECK4,adv_flags&FTPS_ADV_FORWARDBYPASS?BST_CHECKED:BST_UNCHECKED);
				 CheckDlgButton(hwndDlg,IDC_CHECK5,log_level?BST_CHECKED:BST_UNCHECKED);
				 CheckDlgButton(hwndDlg,IDC_CHECK6,adv_flags&FTPS_ADV_IOERRCTRLFTP?BST_CHECKED:BST_UNCHECKED);
				 CheckDlgButton(hwndDlg,IDC_CHECK7,adv_flags&FTPS_ADV_INJECTONTHEFLY?BST_CHECKED:BST_UNCHECKED);    
				 CheckDlgButton(hwndDlg,IDC_CHECK8,adv_flags&FTPS_ADV_MULTIDESKSUPPORT?BST_CHECKED:BST_UNCHECKED);    
				 
				 
				 RegUtils::DlgLoadIntReg(key,TEXT("cache_dir_expire"),default_cache_dir_expire,hwndDlg,IDC_EDIT1);
				 RegUtils::DlgLoadIntReg(key,TEXT("cache_file_expire"),default_cache_file_expire,hwndDlg,IDC_EDIT2);
				 RegUtils::DlgLoadIntReg(key,TEXT("cache_max_file_size"),default_cache_max_file_size,hwndDlg,IDC_EDIT3);
				 RegUtils::DlgLoadIntReg(key,TEXT("ftp_replies_timeout"),default_ftp_replies_timeout,hwndDlg,IDC_EDIT4);    
				 RegUtils::DlgLoadIntReg(key,TEXT("ftp_data_timeout"),default_ftp_data_timeout,hwndDlg,IDC_EDIT9);
				 RegUtils::DlgLoadIntReg(key,TEXT("pre_seek_delay"),default_pre_seek_delay,hwndDlg,IDC_EDIT5);
				 RegUtils::DlgLoadIntReg(key,TEXT("idle_timeout"),default_idle_timeout,hwndDlg,IDC_EDIT6);     
				 RegUtils::DlgLoadIntReg(key,TEXT("err_retrydelay"),default_err_retrydelay,hwndDlg,IDC_EDIT7);    
				 RegUtils::DlgLoadIntReg(key,TEXT("err_maxretries"),default_err_maxretries,hwndDlg,IDC_EDIT8);

				 DWORD hk = RegUtils::GetIntReg(key,TEXT("hk_unfreeze"),default_hk_unfreeze);
				 ::SendDlgItemMessage(hwndDlg,IDC_HK_UNFREEZE,HKM_SETHOTKEY,hk,0);
				 RegCloseKey(key);
			 }
			 TabAdvancedUpdateState(hwndDlg);
		 }break;
	 case WM_COMMAND:
		 {
			 switch(LOWORD(wParam))
			 {
			 case IDOK:
				 {
					 HKEY key=NULL;
					 RegCreateKey(HKEY_CURRENT_USER,TEXT("Software\\KILLSOFT\\FTPDrive"),&key);
					 if(key)
					 {
						 DWORD adv_flags=0,log_level=0;     
						 if(IsDlgButtonChecked(hwndDlg,IDC_CHECK1)==BST_CHECKED)adv_flags|=FTPS_ADV_DIRCACHE;
						 if(IsDlgButtonChecked(hwndDlg,IDC_CHECK2)==BST_CHECKED)adv_flags|=FTPS_ADV_FILECACHE;
						 if(IsDlgButtonChecked(hwndDlg,IDC_CHECK3)==BST_CHECKED)adv_flags|=FTPS_ADV_USEGUARDPAGES;
						 if(IsDlgButtonChecked(hwndDlg,IDC_CHECK4)==BST_CHECKED)adv_flags|=FTPS_ADV_FORWARDBYPASS;
						 if(IsDlgButtonChecked(hwndDlg,IDC_CHECK5)==BST_CHECKED)log_level=1;
						 if(IsDlgButtonChecked(hwndDlg,IDC_CHECK6)==BST_CHECKED)adv_flags|=FTPS_ADV_IOERRCTRLFTP;
						 if(IsDlgButtonChecked(hwndDlg,IDC_CHECK7)==BST_CHECKED)adv_flags|=FTPS_ADV_INJECTONTHEFLY;
						 if(IsDlgButtonChecked(hwndDlg,IDC_CHECK8)==BST_CHECKED)adv_flags|=FTPS_ADV_MULTIDESKSUPPORT;
						 
						 RegUtils::SetIntReg(key,TEXT("adv_flags"),(int)adv_flags);     
						 RegUtils::SetIntReg(key,TEXT("log_level"),(int)log_level);     
						 
						 RegUtils::DlgSaveIntReg(key,TEXT("cache_dir_expire"),hwndDlg,IDC_EDIT1);
						 RegUtils::DlgSaveIntReg(key,TEXT("cache_file_expire"),hwndDlg,IDC_EDIT2);
						 RegUtils::DlgSaveIntReg(key,TEXT("cache_max_file_size"),hwndDlg,IDC_EDIT3);     
						 RegUtils::DlgSaveIntReg(key,TEXT("ftp_replies_timeout"),hwndDlg,IDC_EDIT4);          
						 RegUtils::DlgSaveIntReg(key,TEXT("ftp_data_timeout"),hwndDlg,IDC_EDIT9);     
						 RegUtils::DlgSaveIntReg(key,TEXT("pre_seek_delay"),hwndDlg,IDC_EDIT5);
						 RegUtils::DlgSaveIntReg(key,TEXT("idle_timeout"),hwndDlg,IDC_EDIT6);     
						 RegUtils::DlgSaveIntReg(key,TEXT("err_retrydelay"),hwndDlg,IDC_EDIT7);     
						 RegUtils::DlgSaveIntReg(key,TEXT("err_maxretries"),hwndDlg,IDC_EDIT8);     
						 
						 DWORD hk = ::SendDlgItemMessage(hwndDlg,IDC_HK_UNFREEZE,HKM_GETHOTKEY,0,0);
						 RegUtils::SetIntReg(key,TEXT("hk_unfreeze"),hk);
						 
						 RegCloseKey(key);
					 }
					 EndDialog(hwndDlg,LOWORD(wParam));
				 }break;
			 case IDCANCEL:
				 {
					 EndDialog(hwndDlg,LOWORD(wParam));
				 }break;   
			 case IDC_CHECK1:case IDC_CHECK2:case IDC_CHECK6:
				 {
					 TabAdvancedUpdateState(hwndDlg);
				 }break;
			 }
		 }break;
	 default: return 0;
	 }
	 return 1;
 }
 
 int CALLBACK FDUIDialog::MainDialogProc(HWND hwndDlg,UINT uMsg,WPARAM wParam,LPARAM lParam)
 {
	 FDUIDialog *dlg=(FDUIDialog *)GetWindowLong(hwndDlg,GWL_USERDATA);
	 switch(uMsg)
	 {
	 case WM_INITDIALOG:
		 {
			 std::vector<TCHAR> GenCapBuf(1024),AdvCapBuf(1024),MainCapBuf(1024); 
			 SetWindowLong(hwndDlg,GWL_USERDATA,lParam);
			 dlg=(FDUIDialog *)lParam;
			 wcscpy(&GenCapBuf[0],dlg->_ResDll.GetString(IDS_DLG_GEN).c_str());
			 wcscpy(&AdvCapBuf[0],dlg->_ResDll.GetString(IDS_DLG_ADV).c_str());
			 wsprintf(&MainCapBuf[0],dlg->_ResDll.GetString(IDS_DLG_MAIN).c_str(),TEXT("3.5"));
			 SetWindowText(hwndDlg,&MainCapBuf[0]);
			 SetDlgItemText(hwndDlg,IDC_STATIC_COPYRIGHT,TEXT("(c)2005-2006 Killer{R}, http://www.killprog.com"));
			 TCITEM tci1={0};
			 tci1.mask=TCIF_TEXT;
			 tci1.pszText=&GenCapBuf[0];
			 tci1.iImage=-1;
			 tci1.lParam=0;
			 TCITEM tci2={0};
			 tci2.mask=TCIF_TEXT;
			 tci2.pszText=&AdvCapBuf[0];
			 tci2.iImage=-1;
			 tci2.lParam=0;
			 SendDlgItemMessage(hwndDlg,IDC_TAB1,TCM_INSERTITEM,0,(LPARAM)&tci1);
			 SendDlgItemMessage(hwndDlg,IDC_TAB1,TCM_INSERTITEM,1,(LPARAM)&tci2);
			 dlg->GenTabWnd=CreateDialogParam(dlg->_ResDll.Module,MAKEINTRESOURCE(IDD_GENERAL),GetDlgItem(hwndDlg,IDC_TAB1),TabGeneralDialogProc,(LPARAM)dlg);
			 dlg->AdvTabWnd=CreateDialogParam(dlg->_ResDll.Module,MAKEINTRESOURCE(IDD_ADVANCED),GetDlgItem(hwndDlg,IDC_TAB1),TabAdvancedDialogProc,(LPARAM)dlg);
			 RECT rc={0,0,0,0};
			 SendDlgItemMessage(hwndDlg,IDC_TAB1,TCM_ADJUSTRECT,FALSE,(LPARAM)&rc);  
			 SetWindowPos(dlg->GenTabWnd,0,rc.left,rc.top,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_SHOWWINDOW);
			 SetWindowPos(dlg->AdvTabWnd,0,rc.left,rc.top,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
			 //CenterWindow(hwndDlg);
		 }break;
	 case WM_NOTIFY:
		 {  
			 NMHDR *hdr=(NMHDR *)lParam;
			 if((hdr->idFrom==IDC_TAB1)&&(hdr->code==TCN_SELCHANGE))
			 {   
				 int CurTabIndex=TabCtrl_GetCurSel(GetDlgItem(hwndDlg,IDC_TAB1));
				 if(dlg->OldTabIndex!=CurTabIndex)
				 {     
					 dlg->OldTabIndex=CurTabIndex;
					 switch(CurTabIndex)
					 {
					 case 0:
						 ShowWindow(dlg->AdvTabWnd,SW_HIDE);
						 ShowWindow(dlg->GenTabWnd,SW_SHOW);      
						 break;
					 case 1:
						 ShowWindow(dlg->GenTabWnd,SW_HIDE);
						 ShowWindow(dlg->AdvTabWnd,SW_SHOW);      
						 break;
					 }
				 }   
			 }
		 }break;
	 case WM_COMMAND:
		 {
			 switch(LOWORD(wParam))
			 {
			 case IDOK:case IDCANCEL:
				 {
					 SendMessage(dlg->GenTabWnd,WM_COMMAND,wParam,lParam);
					 SendMessage(dlg->AdvTabWnd,WM_COMMAND,wParam,lParam);
					 EndDialog(hwndDlg,LOWORD(wParam));
				 }
			 }break; 
		 }break;
	 default:return 0;
	 }
	 return 1;
 }
 
 
 FDUIDialog::FDUIDialog()
 {
 }
 
 FDUIDialog::~FDUIDialog()
 {
 }
 
 bool FDUIDialog::Show()
 {
	 OldTabIndex=0;
	 return DialogBoxParam(_ResDll.Module,MAKEINTRESOURCE (IDD_MAIN),NULL,MainDialogProc,(LPARAM)this)==IDOK; 
 }
 
 //////////////////////////////////////////////////////////////////////////////////////
 
 
 std::wstring ErrorDialog::OpCodeToStr(UINT OpCode)
 {
	 ResHolder ResDll;
	 switch(OpCode)
	 {
	 case OPCODE_OPEN:return ResDll.GetString(IDS_OP_OPEN);
	 case OPCODE_ENUM:return ResDll.GetString(IDS_OP_ENUM);
	 case OPCODE_READ:return ResDll.GetString(IDS_OP_READ);
	 case OPCODE_WRITE:return ResDll.GetString(IDS_OP_WRITE);
	 case OPCODE_GETINFO:return ResDll.GetString(IDS_OP_GETINFO);
	 case OPCODE_SETINFO:return ResDll.GetString(IDS_OP_SETINFO);
	 default: return std::wstring(TEXT("?"));
	 }
 }
 ErrorDialog::ErrorDialog(unsigned int OpCode, unsigned int StatusCode,const std::wstring &ProcessName,const std::wstring &FileName,unsigned int Pos,unsigned int BlockSize):
 _OpCode(OpCode),
	 _StatusCode(StatusCode),  
	 _ProcessName(ProcessName),
	 _FileName(FileName),
	 _Pos(Pos),
	 _BlockSize(BlockSize)
 { 
	 _OpStr=OpCodeToStr(OpCode);
 }
 
 ErrorDialog::~ErrorDialog()
 {
	 
 }
 
 int CALLBACK ErrorDialog::ErrorDialogProc(HWND hwndDlg,UINT uMsg,WPARAM wParam,LPARAM lParam)
 {
	 switch(uMsg)
	 {
	 case WM_INITDIALOG:
		 {   
			 ErrorDialog *dlg=(ErrorDialog *)lParam;
			 TCHAR sthex[16];wsprintf(sthex,TEXT("0x%08X"),dlg->_StatusCode);
			 SetDlgItemText(hwndDlg,IDC_EDIT1,dlg->_ProcessName.c_str());
			 SetDlgItemText(hwndDlg,IDC_EDIT2,dlg->_FileName.c_str());
			 SetDlgItemText(hwndDlg,IDC_EDIT3,dlg->_OpStr.c_str());
			 SetDlgItemText(hwndDlg,IDC_EDIT4,sthex);   
			 
			 if(dlg->_Pos!=0xffffffff)
				 SetDlgItemInt(hwndDlg,IDC_EDIT5,dlg->_Pos,FALSE);
				 /*else
			 ShowWindow(GetDlgItem(hwndDlg,IDC_EDIT5),SW_HIDE);*/
			 if(dlg->_BlockSize!=0xffffffff)
				 SetDlgItemInt(hwndDlg,IDC_EDIT6,dlg->_BlockSize,FALSE);
				 /*else    
			 ShowWindow(GetDlgItem(hwndDlg,IDC_EDIT6),SW_HIDE);*/    
			 //CenterWindow(hwndDlg);

			 std::wstring srv_name(dlg->_FileName);
			 size_t p=srv_name.find(TEXT(":\\"));
			 if (p!=std::wstring::npos)
				 srv_name.erase(0,p+2);

			 p=srv_name.find('\\');
			 if (p!=std::wstring::npos)
				 srv_name.resize(p);

			 std::wstring srv_replies(FtpSettings::String2WString(
				 SrvReplyLog::LastReplies(srv_name)));

			 if (!srv_replies.empty())
			 {
				 ::SetDlgItemText(hwndDlg,IDC_CMDLOG,srv_replies.c_str());
				 HWND logwnd = ::GetDlgItem(hwndDlg,IDC_CMDLOG);
				 SCROLLINFO si={sizeof(SCROLLINFO),SIF_RANGE,0};
				 ::GetScrollInfo(logwnd,SB_VERT,&si);
				 //::SetScrollPos(logwnd,SB_VERT,si.nMax,TRUE);
				 while(si.nMax--)
				 {
					 ::SendMessage(logwnd,EM_SCROLL,SB_PAGEDOWN,0);//neccessary
				 }
			 }
		 }break;
		 
	 case WM_COMMAND:
		 {
			 switch(LOWORD(wParam))
			 {
			 case ID_RETRY:EndDialog(hwndDlg,ERR_MODE_RETRY);break;
			 case IDC_RETRY_ALWAYS:EndDialog(hwndDlg,ERR_MODE_MASK_AUTO|ERR_MODE_RETRY);break;
			 case ID_IGNORE:EndDialog(hwndDlg,ERR_MODE_IGNORE);break;
			 case IDD_IGNORE_ALWAS:EndDialog(hwndDlg,ERR_MODE_MASK_AUTO|ERR_MODE_IGNORE);break;
				 //case IDC_DISCARD:EndDialog(hwndDlg,ERR_MODE_DISCARD);break;
			 }
		 }break;
	 default: return 0;
	 }
	 return 1;
 }
 
 unsigned int ErrorDialog::Show()
 {
	 HDESK old_dsk = g_cur_desktop?::GetThreadDesktop(GetCurrentThreadId()):NULL;

	 if (g_cur_desktop)
		 ::SetThreadDesktop(g_cur_desktop);

	 int out = DialogBoxParam(_ResDll.Module,MAKEINTRESOURCE (IDD_IOERROR),NULL,ErrorDialogProc,(LPARAM)this); 
	 if(old_dsk)
		 ::SetThreadDesktop(old_dsk);

	 return out;
 }
 
 
 //////////////////////////////////////////////////////////////////////////////////////
 
 SitesEditDialog::SitesEditDialog(
	 const std::wstring &fname,
	 const std::wstring &nv_hostname, 
	 const std::wstring &nv_hostip,
	 const std::wstring &nv_user,
	 const std::wstring &nv_pass,
	 const std::wstring &nv_flags
	 ):_ListFile(fname), _DontSaveHosts(true)
	{
	 _SitesList = FtpSettings::SitesListFromFile(fname,
		 nv_hostname,nv_hostip,nv_user,nv_pass,nv_flags);
	}
	
 
 SitesEditDialog::SitesEditDialog(
	 const std::wstring &fname
	 ):_ListFile(fname), _DontSaveHosts(false)
	{
	 _SitesList = FtpSettings::SitesListFromFile(fname);
	}
	
 SitesEditDialog::SitesEditDialog(
	 bool remote_netview
	 ):_DontSaveHosts(false)
 { 
	 _SitesList = FtpSettings::SitesListFromNV(remote_netview);
 }
 
 SitesEditDialog::~SitesEditDialog()
 {
	 if (_SitesList)
		 delete _SitesList;
 }
 
 void SitesEditDialog::UpdateSelectedList(HWND hwndDlg)
 {
	 LONG cur_sel=::SendDlgItemMessage(hwndDlg,IDC_SITESLIST,LB_GETCURSEL,0,0);
	 if ((cur_sel!=CB_ERR )
		 && (::SendDlgItemMessage(hwndDlg,IDC_SITESLIST,LB_GETTEXTLEN,cur_sel,0)<512))
	 {
		 WCHAR	select_site[512]={0};
		 ::SendDlgItemMessage(hwndDlg,IDC_SITESLIST,LB_GETTEXT,cur_sel,LPARAM(select_site));
		 _SiteSelected.assign(select_site);
	 }
	 else
	 {
		 _SiteSelected.resize(0);
	 }
 }
 
 std::wstring GetDlgItemString(HWND hwndDlg, UINT id)
 {
	 WCHAR buf[512];
	 UINT r = ::GetDlgItemText(hwndDlg,id,buf,512);
	 if (r>0&&r<512)
	 {
		 return std::wstring(buf);
	 }
	 return std::wstring();
	 
	}
	
	void SitesEditDialog::DeleteSiteLastSelected(HWND hwndDlg)
	{
		UpdateSelectedList(hwndDlg);
		if (!_SiteSelected.empty())
		{
			_SitesList->DelHost(_SiteSelected);
			_SiteSelected.resize(0);
			LoadSitesList(hwndDlg);
		}
	}


	void SitesEditDialog::ApplySiteEnteredName(HWND hwndDlg)
	{
		_SiteSelected = ::GetDlgItemString(hwndDlg,IDC_NAME);
		ApplySiteLastSelected(hwndDlg);
	}
	

	void SitesEditDialog::ApplySiteLastSelected(HWND hwndDlg)
	{
		if(_SiteSelected.empty())
			return;
		
		std::wstring host,user,pass,home;
		DWORD port,flags,conlim;
		ENCCP enc;
		FTPSMODE smode;

		_SitesList->GetHostSetup(_SiteSelected,host,port,user,pass,home,flags,conlim,enc,smode);
		
		host=::GetDlgItemString(hwndDlg,IDC_HOST);
		port=::GetDlgItemInt(hwndDlg,IDC_PORT,NULL,FALSE);
		if(!port)port=21;

		user=::GetDlgItemString(hwndDlg,IDC_USER);		
		pass=::GetDlgItemString(hwndDlg,IDC_PASSWORD);
		home=::GetDlgItemString(hwndDlg,IDC_HOME);
		conlim=::GetDlgItemInt(hwndDlg,IDC_CONNLIMIT,NULL,FALSE);

		if (user.empty())
		{
			user.assign(TEXT("anonymous"));
			if(pass.empty())
				pass.assign(TEXT("ftpdrive@killprog.com"))	;
		}
		
		
		switch(::SendDlgItemMessage(hwndDlg,IDC_SSLMODE,CB_GETCURSEL,0,0))
		{
		case 1:
			smode = SModeImplicit;
			break;

		case 2:
			smode = SModeExplicitSsl;
			break;

		case 3:
			smode = SModeExplicitTls;
			break;

		case 0:
		default:
			smode = SModeUnsecure;
		}

		switch(::SendDlgItemMessage(hwndDlg,IDC_ENCODING,CB_GETCURSEL,0,0))
		{
		case 1:enc = EncDOS;break;
			
		case 2:enc = EncUTF8;break;
			
		case 3:enc = EncKOI8R;break;

		case 4:enc = EncKOI8U;break;

		case 0:
		default:
			enc = EncWIN;
		}
		
		flags&=~0x1400;
		
		switch(::SendDlgItemMessage(hwndDlg,IDC_MODE,CB_GETCURSEL,0,0))
		{
		case 1:flags|=0x400;break;
			
		case 2:flags|=0x1000;break;
			
		case 0:
		default:
			flags|=0x1400;
		}
		
		if (host.empty())
			host = _SiteSelected.c_str();
		
		_SitesList->SetHostSetup(_SiteSelected,host,port,user,pass,home,flags,conlim,enc,smode);
		LoadSitesList(hwndDlg);
	}
	
	void SitesEditDialog::LoadSiteSelected(HWND hwndDlg)
	{
		UpdateSelectedList(hwndDlg);
		if (_SiteSelected.empty())
		{
			SetReadOnlyHost(hwndDlg,true);
			return;
		}
		SetReadOnlyHost(hwndDlg,false);
		
		std::wstring host,user,pass,home;
		DWORD port,flags,conlim;
		ENCCP enc;
		FTPSMODE smode;
		
		_SitesList->GetHostSetup(_SiteSelected,host,port,user,pass,home,flags,conlim,enc,smode);
		
		::SetDlgItemText(hwndDlg,IDC_NAME,_SiteSelected.c_str());
		::SetDlgItemText(hwndDlg,IDC_HOST,host.c_str());
		::SetDlgItemInt(hwndDlg,IDC_PORT,port,FALSE);
		::SetDlgItemText(hwndDlg,IDC_USER,user.c_str());
		::SetDlgItemText(hwndDlg,IDC_PASSWORD,pass.c_str());
		::SetDlgItemText(hwndDlg,IDC_HOME,home.c_str());
		::SetDlgItemInt(hwndDlg,IDC_CONNLIMIT,conlim,FALSE);
		
		switch(smode)
		{
		case SModeImplicit:
			::SendDlgItemMessage(hwndDlg,IDC_SSLMODE,CB_SETCURSEL,1,0);
			break;

		case SModeExplicitSsl:
			::SendDlgItemMessage(hwndDlg,IDC_SSLMODE,CB_SETCURSEL,2,0);
			break;

		case SModeExplicitTls:
			::SendDlgItemMessage(hwndDlg,IDC_SSLMODE,CB_SETCURSEL,3,0);
			break;

		case SModeUnsecure:
		default:
			::SendDlgItemMessage(hwndDlg,IDC_SSLMODE,CB_SETCURSEL,0,0);
		}

		switch(enc)
		{
		case EncDOS:
			::SendDlgItemMessage(hwndDlg,IDC_ENCODING,CB_SETCURSEL,1,0);
			break;
			
		case EncUTF8:
			::SendDlgItemMessage(hwndDlg,IDC_ENCODING,CB_SETCURSEL,2,0);
			break;
			
		case EncKOI8R:
			::SendDlgItemMessage(hwndDlg,IDC_ENCODING,CB_SETCURSEL,3,0);
			break;
			
		case EncKOI8U:
			::SendDlgItemMessage(hwndDlg,IDC_ENCODING,CB_SETCURSEL,4,0);
			break;
			
		case EncWIN:
		default:
			::SendDlgItemMessage(hwndDlg,IDC_ENCODING,CB_SETCURSEL,0,0);
		}

		
		switch(flags&0x1400)
		{
		case 0x400:
			::SendDlgItemMessage(hwndDlg,IDC_MODE,CB_SETCURSEL,1,0);
			break;
			
		case 0x1000:
			::SendDlgItemMessage(hwndDlg,IDC_MODE,CB_SETCURSEL,2,0);
			break;
			
		case 0x1400:
		default:
			::SendDlgItemMessage(hwndDlg,IDC_MODE,CB_SETCURSEL,0,0);
		}
		
		
		
	}
	
	void SitesEditDialog::SetReadOnlyHost(HWND hwndDlg, bool ro)
	{
		//::EnableWindow(::GetDlgItem(hwndDlg,IDC_APPLY),ro?FALSE:TRUE);
		::EnableWindow(::GetDlgItem(hwndDlg,IDC_DELETE),ro?FALSE:TRUE);
	}
	
	void SitesEditDialog::LoadSitesList(HWND hwndDlg)
	{
		UpdateSelectedList(hwndDlg);
		while (::SendDlgItemMessage(hwndDlg,IDC_SITESLIST,LB_GETCOUNT,0,0))
		{
			::SendDlgItemMessage(hwndDlg,IDC_SITESLIST,LB_DELETESTRING,0,0);
		}
		
		STRINGVECTOR sites;
		_SitesList->GetHostList(sites);
		for(STRINGVECTOR::iterator i=sites.begin();i!=sites.end();++i)
		{
			::SendDlgItemMessage(hwndDlg,IDC_SITESLIST,LB_ADDSTRING,0,LPARAM(i->c_str()));
		}
		if (!_SiteSelected.empty())
		{
			::SendDlgItemMessage(hwndDlg,IDC_SITESLIST,LB_SELECTSTRING,-1,LPARAM(_SiteSelected.c_str()));
			LoadSiteSelected(hwndDlg);
		}
	}

	void SitesEditDialog::SaveSitesList(HWND hwndDlg)
	{
		_SitesList->SaveTo(_ListFile,_DontSaveHosts);
	}
	
	int CALLBACK SitesEditDialog::DialogProc(HWND hwndDlg,UINT uMsg,WPARAM wParam,LPARAM lParam)
	{
		SitesEditDialog *dlg=(SitesEditDialog *)::GetWindowLong(hwndDlg,GWL_USERDATA);
		switch(uMsg)
		{
		case WM_INITDIALOG:
			{
				::SetWindowLong(hwndDlg,GWL_USERDATA,lParam);
				dlg=(SitesEditDialog *)lParam;
				
				::SendDlgItemMessage(hwndDlg,IDC_MODE,CB_ADDSTRING,0,LPARAM(TEXT("Default")));
				::SendDlgItemMessage(hwndDlg,IDC_MODE,CB_ADDSTRING,0,LPARAM(TEXT("Active")));
				::SendDlgItemMessage(hwndDlg,IDC_MODE,CB_ADDSTRING,0,LPARAM(TEXT("Passive")));
				
				::SendDlgItemMessage(hwndDlg,IDC_ENCODING,CB_ADDSTRING,0,LPARAM(TEXT("Windows")));
				::SendDlgItemMessage(hwndDlg,IDC_ENCODING,CB_ADDSTRING,0,LPARAM(TEXT("DOS")));
				::SendDlgItemMessage(hwndDlg,IDC_ENCODING,CB_ADDSTRING,0,LPARAM(TEXT("UTF8")));
				::SendDlgItemMessage(hwndDlg,IDC_ENCODING,CB_ADDSTRING,0,LPARAM(TEXT("KOI8R")));
				::SendDlgItemMessage(hwndDlg,IDC_ENCODING,CB_ADDSTRING,0,LPARAM(TEXT("KOI8U")));

				::SendDlgItemMessage(hwndDlg,IDC_SSLMODE,CB_ADDSTRING,0,LPARAM(TEXT("Server doesn't support FTPS")));
				::SendDlgItemMessage(hwndDlg,IDC_SSLMODE,CB_ADDSTRING,0,LPARAM(TEXT("Implicit SSL/TLS")));
				::SendDlgItemMessage(hwndDlg,IDC_SSLMODE,CB_ADDSTRING,0,LPARAM(TEXT("Explicit SSL")));
				::SendDlgItemMessage(hwndDlg,IDC_SSLMODE,CB_ADDSTRING,0,LPARAM(TEXT("Explicit TLS")));
				
				if (dlg->_ListFile.empty())
				{
					::ShowWindow(::GetDlgItem(hwndDlg,IDC_APPLY),SW_HIDE);
					::ShowWindow(::GetDlgItem(hwndDlg,IDC_DELETE),SW_HIDE);
					::ShowWindow(::GetDlgItem(hwndDlg,IDOK),SW_HIDE);
				}

				if (dlg->_DontSaveHosts)
				{
					::ShowWindow(::GetDlgItem(hwndDlg,IDC_HOSTLABEL),SW_HIDE);
					::ShowWindow(::GetDlgItem(hwndDlg,IDC_HOST),SW_HIDE);
				}
				
				dlg->LoadSitesList(hwndDlg);
				dlg->LoadSiteSelected(hwndDlg);
			}break;
			
			
		case WM_COMMAND:
			{
				switch(LOWORD(wParam))
				{
				case IDC_SITESLIST:
					{
						if(HIWORD(wParam)==LBN_SELCHANGE)
						{
							dlg->ApplySiteLastSelected(hwndDlg);
							dlg->LoadSiteSelected(hwndDlg);
						}
					}break;
				case IDC_APPLY:
					{
						dlg->ApplySiteEnteredName(hwndDlg);
					}break;
				case IDC_DELETE:
					{
						dlg->DeleteSiteLastSelected(hwndDlg);
					}break;
					
				case IDOK:
					{
						dlg->ApplySiteLastSelected(hwndDlg);
						dlg->SaveSitesList(hwndDlg);
						::EndDialog(hwndDlg,IDOK);
					}break;
					
				case IDCANCEL:
					::EndDialog(hwndDlg,IDCANCEL);
					break;
				}
			}break;
		default: return 0;
		}
		return 1;
	}
	
	void SitesEditDialog::Show(HWND parent_wnd)
	{
		DialogBoxParam(_ResDll.Module,MAKEINTRESOURCE (IDD_SITESEDIT),parent_wnd,DialogProc,(LPARAM)this); 
	}
	
	
	///////////////////////////////////////////////////////////
	
