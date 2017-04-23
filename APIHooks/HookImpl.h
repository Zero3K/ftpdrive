#ifndef _HOOKIMPL_H_
#define _HOOKIMPL_H_
#include <windows.h>
#include <MAP>
#include <STRING>
#include <VECTOR>
extern HINSTANCE hmod;
extern volatile HHOOK hhook;

namespace HookImpl
{
	bool IsThisExplorer();
	bool IsThisKillCopy();
	bool IsThisFtpDrive();
	bool IsThisAVP();
	bool IsThisWinDbg();

typedef std::vector<HANDLE>  HANDLEVECTOR;

NTSTATUS  WrapperOpenFile(PHANDLE FileHandle,ACCESS_MASK DesiredAccess,
                                    POBJECT_ATTRIBUTES ObjectAttributes,
                                    PIO_STATUS_BLOCK IoStatusBlock,
                                    ULONG CreateDisposition,ULONG CreateOptions);
NTSTATUS  RealCreateFile(PHANDLE FileHandle,ACCESS_MASK DesiredAccess,const std::wstring &PathName,ULONG CreateDisposition);

typedef NTSTATUS (__stdcall *realZwFsControlFile_t)(IN HANDLE FileHandle,
                                   IN HANDLE Event OPTIONAL,
                                   IN PIO_APC_ROUTINE ApcRoutine OPTIONAL,
                                   IN PVOID ApcContext OPTIONAL,
                                   OUT PIO_STATUS_BLOCK IoStatusBlock,
                                   IN ULONG FsControlCode,
                                   IN PVOID InputBuffer OPTIONAL,
                                   IN ULONG InputBufferLength,
                                   OUT PVOID OutputBuffer OPTIONAL,
                                   IN ULONG OutputBufferLength);

typedef NTSTATUS (__stdcall *realZwDeviceIoControlFile_t)(IN HANDLE FileHandle,
										 IN HANDLE Event OPTIONAL,
										 IN PIO_APC_ROUTINE ApcRoutine OPTIONAL,
										 IN PVOID ApcContext OPTIONAL,
										 OUT PIO_STATUS_BLOCK IoStatusBlock,
										 IN ULONG IoControlCode,
										 IN PVOID InputBuffer OPTIONAL,
										 IN ULONG InputBufferLength,
										 OUT PVOID OutputBuffer OPTIONAL,
										 IN ULONG OutputBufferLength
										 );

typedef NTSTATUS (__stdcall *realZwOpenFile_t)(OUT PHANDLE FileHandle,IN ACCESS_MASK DesiredAccess,
                               IN POBJECT_ATTRIBUTES ObjectAttributes,
                               OUT PIO_STATUS_BLOCK IoStatusBlock,
                               IN ULONG ShareAccess,IN ULONG OpenOptions);

typedef NTSTATUS (__stdcall *realZwCreateFile_t)(OUT PHANDLE FileHandle,IN ACCESS_MASK DesiredAccess,
                               IN POBJECT_ATTRIBUTES ObjectAttributes,
                               OUT PIO_STATUS_BLOCK IoStatusBlock,
                               IN PLARGE_INTEGER AllocationSize OPTIONAL,
                               IN ULONG FileAttributes,IN ULONG ShareAccess,
                               IN ULONG CreateDisposition,IN ULONG CreateOptions,
                               IN PVOID EaBuffer OPTIONAL,IN ULONG EaLength);

typedef NTSTATUS (__stdcall *realZwReadFile_t)(IN HANDLE FileHandle,IN HANDLE Event OPTIONAL,
                                 IN PIO_APC_ROUTINE ApcRoutine OPTIONAL,
                                 PVOID,PIO_STATUS_BLOCK,PVOID,
                                 ULONG,PLARGE_INTEGER,PULONG);
  
typedef NTSTATUS (__stdcall *realZwWriteFile_t)(IN HANDLE FileHandle,IN HANDLE Event OPTIONAL,
                                 IN PIO_APC_ROUTINE ApcRoutine OPTIONAL,
                                 PVOID,PIO_STATUS_BLOCK,PVOID,
                                 ULONG,PLARGE_INTEGER,PULONG);

typedef NTSTATUS (__stdcall *realZwClose_t)(IN HANDLE Handle);

extern realZwFsControlFile_t          realZwFsControlFile;
extern realZwOpenFile_t               realZwOpenFile;
extern realZwCreateFile_t             realZwCreateFile;
extern realZwReadFile_t               realZwReadFile;
extern realZwWriteFile_t              realZwWriteFile;
extern realZwClose_t                  realZwClose;

PVOID NtGetProcAddr(HMODULE hDll, const char *pszProcName);

void NtDllHookInfo();
void NtDllHookSections();
void NtDllHookData();
void NtDllHookSectionsDeinit();
void FileCopyHook();

void HiLevelHook();

void *DetourByName(HINSTANCE m,char *realFuncName,PBYTE newFn);
void *DetourByPtr(PBYTE realFn,PBYTE newFn);
void InitApiHooks();
void DeinitApiHooks();
void SectionsNotifyCloseHandle(HANDLE handle);
void SectionsEnsureContent(ULONG_PTR addr, SIZE_T size);
bool PathMatchSpec(const TCHAR *pszFileParam, const TCHAR *pszSpec);
std::wstring GetPathByHandle(int fs_handle);
}
POBJECT_ATTRIBUTES NameInitObjectAttributes(const wchar_t *objname, size_t namelen=std::string::npos);
POBJECT_ATTRIBUTES __inline NameInitObjectAttributes(const std::wstring &s)
 {return NameInitObjectAttributes(s.c_str(), (s.size()+1)*sizeof(WCHAR));}

#define NameFreeObjectAttributes(attr) free((char *)(attr));


#endif //_HOOKIMPL_H_

class LastErrorSaver
{
private:
	int _le;
public:
	inline LastErrorSaver(){_le=GetLastError();}
	inline ~LastErrorSaver(){SetLastError(_le);}
};

