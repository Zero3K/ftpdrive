#ifndef _FTPDRIVE_DIALOGS_H
#define _FTPDRIVE_DIALOGS_H
#include "../ResHolder.h"
#include "../../FtpSettings/settings.h"
class FDUIDialog
{
private:
	static void TabAdvancedUpdateState(HWND hwndDlg);
	static void LoadComboLng(HWND hwnd);
	static int CALLBACK TabGeneralDialogProc(HWND hwndDlg,UINT uMsg,WPARAM wParam,LPARAM lParam);
	static int CALLBACK TabAdvancedDialogProc(HWND hwndDlg,UINT uMsg,WPARAM wParam,LPARAM lParam);
	static int CALLBACK MainDialogProc(HWND hwndDlg,UINT uMsg,WPARAM wParam,LPARAM lParam); 
	HWND GenTabWnd;
	HWND AdvTabWnd;
	int OldTabIndex;
	ResHolder _ResDll;
public:
	FDUIDialog();
	~FDUIDialog();
	bool Show();
};

class ErrorDialog
{
private:
	static int CALLBACK ErrorDialogProc(HWND hwndDlg,UINT uMsg,WPARAM wParam,LPARAM lParam); 
	unsigned int _StatusCode;
	std::wstring _ProcessName;
	std::wstring _FileName;
	
	unsigned int _OpCode;
	std::wstring _OpStr;
	unsigned int _Pos;
	unsigned int _BlockSize;
	ResHolder _ResDll;
	
public:
	static std::wstring OpCodeToStr(UINT OpCode);
	ErrorDialog(unsigned int OpCode, unsigned int StatusCode,const std::wstring &ProcessName,const std::wstring &FileName,unsigned int Pos,unsigned int BlockSize);
	~ErrorDialog();
	unsigned int Show();
};

class SitesEditDialog
{
private:
	static int CALLBACK DialogProc(HWND hwndDlg,UINT uMsg,WPARAM wParam,LPARAM lParam); 
	std::wstring	_ListFile,_SiteSelected;
	ResHolder		_ResDll;
	bool			_DontSaveHosts;

	IFtpSitesList *_SitesList;
	void SaveSitesList(HWND hwndDlg);
	void LoadSitesList(HWND hwndDlg);
	void LoadSiteSelected(HWND hwndDlg);
	void ApplySiteLastSelected(HWND hwndDlg);
	void ApplySiteEnteredName(HWND hwndDlg);
	void DeleteSiteLastSelected(HWND hwndDlg);
	void SetReadOnlyHost(HWND hwndDlg,bool ro);
	
	std::wstring GetDlgItemString(HWND hwndDlg, UINT id);
	void UpdateSelectedList(HWND hwndDlg);
	
public:
	SitesEditDialog(
		const std::wstring &fname
		);
	SitesEditDialog(
		const std::wstring &fname,
		const std::wstring &nv_hostname, 
		const std::wstring &nv_hostip,
		const std::wstring &nv_user,
		const std::wstring &nv_pass,
		const std::wstring &nv_flags
		);
	SitesEditDialog(
		bool remote_netview
		);//if false - local netview used
	~SitesEditDialog();
	void Show(HWND parent_wnd = NULL);
};

#endif// _FTPDRIVE_DIALOGS_H
