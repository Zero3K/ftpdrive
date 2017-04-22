#include "stdafx.h"
#include <tlhelp32.h>
#include "./UI/Dialogs.h"
#include "../FtpSettings/settings.h"
#include "../FtpSettings/defs.h"
#include "../Res/English/resource.h"
#include "NetControl.h"
#include "ErrorHandler.h"
#include "FtpFS.h"
#include "../FtpFS/ntdefs.h"
#include "../APIHooks/NTDllExports.h"

namespace ErrorHandler
{
	std::wstring GetProcessName(DWORD pid)
	{
		std::wstring out;
		
		typedef HANDLE (WINAPI *CreateToolhelp32Snapshotx)(DWORD,DWORD);
		typedef DWORD (WINAPI *Process32Firstx)(HANDLE,LPPROCESSENTRY32);
		typedef DWORD (WINAPI *Process32Nextx)(HANDLE,LPPROCESSENTRY32); 
		
		HINSTANCE m_ke=GetModuleHandle(TEXT("kernel32.dll"));
		
		CreateToolhelp32Snapshotx extCreateToolhelp32Snapshot=(CreateToolhelp32Snapshotx)GetProcAddress(m_ke,"CreateToolhelp32Snapshot"); 
#ifdef UNICODE
		Process32Firstx extProcess32First=(Process32Firstx)GetProcAddress(m_ke,"Process32FirstW");
		Process32Nextx extProcess32Next=(Process32Nextx)GetProcAddress(m_ke,"Process32NextW");
#else
		Process32Firstx extProcess32First=(Process32Firstx)GetProcAddress(m_ke,"Process32First");
		Process32Nextx extProcess32Next=(Process32Nextx)GetProcAddress(m_ke,"Process32Next");
#endif
		
		if(m_ke&&extCreateToolhelp32Snapshot&&extProcess32First&&extProcess32Next)
		{  
			HANDLE hs=extCreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
			if(hs!=INVALID_HANDLE_VALUE)
			{
				PROCESSENTRY32 pe={sizeof(PROCESSENTRY32),0};
				if(extProcess32First(hs,&pe))
					do
					{
						if(pe.th32ProcessID==pid)
						{
							out.assign(pe.szExeFile);       
							break;
						}
					}while(extProcess32Next(hs,&pe));
					CloseHandle(hs);
			}
		}
		if( out.empty())
		{
			TCHAR strpid[64];wsprintf(strpid,TEXT("pid_%u"),pid);
			out.assign(strpid);
		}
		
		return out;
	}
	
	
	typedef struct ERRENTRY_
	{
		DWORD counter;
		DWORD mode;
	}ERRENTRY;
	typedef std::map<DWORD,ERRENTRY> PID2ERRS;
	PID2ERRS pid_2_errs[OPCODE_MAXOPCODE+1];
	
	FastCriticalSection p2e_cs;
	
	void QueryRetryParams(unsigned int OpCode, DWORD pid, int &mode, DWORD &retries)
	{
		AutoCriticalSection cslock(p2e_cs);
		PID2ERRS::iterator i = pid_2_errs[OpCode].find(pid);
		if(i!=pid_2_errs[OpCode].end())
		{
			if(i->second.counter<FtpSettings::err_maxretries)
			{
				if(i->second.counter<0xfffffffe)i->second.counter = i->second.counter + 1;
				mode  = i->second.mode;
			}else pid_2_errs[OpCode].erase(i);   
			retries = i->second.counter;
		}
	}

	bool CheckRetry(unsigned int OpCode, unsigned int StatusCode,DWORD pid,const std::wstring &FileName,unsigned int Pos,unsigned int BlockSize)
	{
		if (net_control.WasKilled())return false;

		if((OpCode>OPCODE_MAXOPCODE)||(!(FtpSettings::adv_flags&FTPS_ADV_IOERRCTRLFTP)))return false;
		std::wstring ProcessName=GetProcessName(pid);
		int mode = 0;
		DWORD retries = 0xffffffff;
		QueryRetryParams(OpCode, pid, mode, retries);
	
		if(!mode)
		{
			ErrorDialog ed(OpCode,StatusCode,ProcessName,FileName,Pos,BlockSize);
			unsigned int answer = ed.Show();
			mode = answer&ERR_MODE_MASK_ANSW;
			if(answer&ERR_MODE_MASK_AUTO)
			{
				AutoCriticalSection cslock(p2e_cs);
				PID2ERRS::iterator i = pid_2_errs[OpCode].find(pid);
				if(i==pid_2_errs[OpCode].end())
				{
					ERRENTRY ee;
					ee.counter = 1;
					ee.mode = mode;
					pid_2_errs[OpCode].insert(PID2ERRS::value_type(pid,ee));
				}
			}
		}else
		{
			ResHolder ResDll;
			std::wstring ActStr, OpStr = ErrorDialog::OpCodeToStr(OpCode);
			if(mode&ERR_MODE_RETRY)
			{
				ActStr.assign(ResDll.GetString(IDS_ACT_RETRY));
			}else if(mode&ERR_MODE_IGNORE)
			{
				ActStr.assign(ResDll.GetString(IDS_ACT_IGNORE));
			}else 
			{
				ActStr.assign(TEXT("?"));
			}
			
			
			std::vector<TCHAR> Text(1024);
			_snwprintf(&Text[0],1023,
				ResDll.GetString(IDS_HINT_ERROR).c_str(),
				ProcessName.c_str(),OpStr.c_str(),retries,FtpSettings::err_maxretries,ActStr.c_str());
			Text[1023]=0;
			ShowPopup(NIIF5_WARNING,ResDll.GetString(IDS_HINT_TITLE_ERROR).c_str(),std::wstring(&Text[0]));
		}
		return (mode==ERR_MODE_RETRY);
	}
	
	void ClearAutoActions(unsigned int pid)
	{
		AutoCriticalSection cslock(p2e_cs);
		for(int c=0;c<OPCODE_MAXOPCODE;c++)
		{
			if(pid==0xffffffff)
			{
				pid_2_errs[c].clear();
			}else
			{    
				PID2ERRS::iterator i = pid_2_errs[c].find(pid);
				if(i!=pid_2_errs[c].end())pid_2_errs[c].erase(i);
			}
		}
	}
	
 }

