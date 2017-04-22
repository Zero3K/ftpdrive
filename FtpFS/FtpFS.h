#ifndef _FTPFS_H_
#define _FTPFS_H_
#include "../AnyFS/AnyFS.h"
#include <SET>
#include <VECTOR>

typedef std::vector<char> READBUFFER;
typedef std::set<DWORD> DWORDSET;

bool ProceedRequest(HANDLE hPipe, LPANYFS_RAW_REQUEST_HEADER Request, DWORD cbBytesRead, DWORD client_pid, READBUFFER &read_buffer);
HANDLE InitializePipeMainThread();
void UiMain(HINSTANCE hInstance);
void ShowPopup(unsigned int Flags,const std::wstring &Title,const std::wstring &Text);
std::wstring GetSessionObjectName(PWCHAR name);
HANDLE CreateMagicMutant(bool fail_if_exists);
void BlockPid(DWORD pid);
bool IsPidBlocked(DWORD pid);
void DisableHotkey();
void UpdateHotkey();

#define NIIF5_INFO       0x00000001
#define NIIF5_WARNING    0x00000002
#define NIIF5_ERROR      0x00000003

#endif //_FTPFS_H_

