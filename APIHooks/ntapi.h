#ifndef _NTDLLEXPORTS_H_
#define _NTDLLEXPORTS_H_
#include "../FtpFS/ntdefs.h"
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

NTSTATUS __stdcall ZwClose(HANDLE Handle);

NTSTATUS __stdcall ZwCreateFile(OUT PHANDLE FileHandle,IN ACCESS_MASK DesiredAccess,
                               IN POBJECT_ATTRIBUTES ObjectAttributes,
                               OUT PIO_STATUS_BLOCK IoStatusBlock,
                               IN PLARGE_INTEGER AllocationSize OPTIONAL,
                               IN ULONG FileAttributes,IN ULONG ShareAccess,
                               IN ULONG CreateDisposition,IN ULONG CreateOptions,
                               IN PVOID EaBuffer OPTIONAL,IN ULONG EaLength);

NTSTATUS __stdcall ZwOpenFile(OUT PHANDLE FileHandle,IN ACCESS_MASK DesiredAccess,
                                IN POBJECT_ATTRIBUTES ObjectAttributes,
                                OUT PIO_STATUS_BLOCK IoStatusBlock,
                                IN ULONG ShareAccess,IN ULONG OpenOptions);

NTSTATUS __stdcall ZwReadFile(IN HANDLE FileHandle,IN HANDLE Event OPTIONAL,
                                 IN PIO_APC_ROUTINE ApcRoutine OPTIONAL,
                                 IN PVOID ApcContext OPTIONAL,
                                 OUT PIO_STATUS_BLOCK IoStatusBlock,IN PVOID Buffer,
                                 IN ULONG Length,IN PLARGE_INTEGER ByteOffset OPTIONAL,
                                 IN PULONG Key OPTIONAL);

NTSTATUS __stdcall ZwWriteFile(IN HANDLE FileHandle,IN HANDLE Event OPTIONAL,
                                 IN PIO_APC_ROUTINE ApcRoutine OPTIONAL,
                                 IN PVOID ApcContext OPTIONAL,
                                 OUT PIO_STATUS_BLOCK IoStatusBlock,IN PVOID Buffer,
                                 IN ULONG Length,IN PLARGE_INTEGER ByteOffset OPTIONAL,
                                 IN PULONG Key OPTIONAL);
NTSTATUS __stdcall ZwFlushBuffersFile(IN HANDLE FileHandle,OUT PIO_STATUS_BLOCK IoStatusBlock);

NTSTATUS __stdcall ZwCancelIoFile(IN HANDLE FileHandle,OUT PIO_STATUS_BLOCK IoStatusBlock);

NTSTATUS __stdcall ZwLockFile(IN HANDLE FileHandle,IN HANDLE Event OPTIONAL,
                                IN PIO_APC_ROUTINE ApcRoutine OPTIONAL,IN PVOID ApcContext OPTIONAL,
                                OUT PIO_STATUS_BLOCK IoStatusBlock,IN PULARGE_INTEGER LockOffset,
                                IN PULARGE_INTEGER LockLength,IN ULONG Key,
                                IN BOOLEAN FailImmediately,IN BOOLEAN ExclusiveLock);

NTSTATUS __stdcall ZwUnlockFile(IN HANDLE FileHandle,OUT PIO_STATUS_BLOCK IoStatusBlock,
                                  IN PULARGE_INTEGER LockOffset,IN PULARGE_INTEGER LockLength,IN ULONG Key);

NTSTATUS __stdcall ZwDuplicateObject(IN HANDLE SourceProcessHandle,IN HANDLE SourceHandle,
                                      IN HANDLE TargetProcessHandle,OUT PHANDLE TargetHandle OPTIONAL,
                                      IN ACCESS_MASK DesiredAccess,IN ULONG Attributes,IN ULONG Options);


NTSTATUS __stdcall ZwQueryInformationProcess(HANDLE ProcessHandle,PROCESSINFOCLASS ProcessInformationClass,PVOID ProcessInformation,ULONG ProcessInformationLength,PULONG ReturnLength);

NTSTATUS __stdcall ZwQueryInformationFile(HANDLE FileHandle,PIO_STATUS_BLOCK IoStatusBlock,
                                            PVOID FileInformation,ULONG FileInformationLength,FILE_INFORMATION_CLASS FileInformationClass);

NTSTATUS __stdcall ZwSetInformationFile(HANDLE FileHandle,PIO_STATUS_BLOCK IoStatusBlock,
                                          PVOID FileInformation,ULONG FileInformationLength,FILE_INFORMATION_CLASS FileInformationClass);

NTSTATUS __stdcall ZwQueryVolumeInformationFile(IN HANDLE FileHandle,OUT PIO_STATUS_BLOCK IoStatusBlock,
                                                  OUT PVOID VolumeInformation,IN ULONG VolumeInformationLength,
                                                  IN FS_INFORMATION_CLASS VolumeInformationClass);

NTSTATUS __stdcall ZwSetVolumeInformationFile(IN HANDLE FileHandle,OUT PIO_STATUS_BLOCK IoStatusBlock,
                                                IN PVOID Buffer,IN ULONG BufferLength,
                                                IN FS_INFORMATION_CLASS VolumeInformationClass);

NTSTATUS __stdcall ZwQueryAttributesFile(IN POBJECT_ATTRIBUTES ObjectAttributes,OUT PFILE_BASIC_INFORMATION FileInformation);

NTSTATUS __stdcall ZwQueryFullAttributesFile(IN POBJECT_ATTRIBUTES ObjectAttributes,OUT PFILE_NETWORK_OPEN_INFORMATION FileInformation);

NTSTATUS __stdcall ZwDeleteFile(IN POBJECT_ATTRIBUTES ObjectAttributes);

NTSTATUS __stdcall ZwCreateSection(OUT PHANDLE SectionHandle,IN ACCESS_MASK DesiredAccess,
                                    IN POBJECT_ATTRIBUTES ObjectAttributes,IN PLARGE_INTEGER SectionSize OPTIONAL,
                                    IN ULONG Protect,IN ULONG Attributes,IN HANDLE FileHandle);

NTSTATUS __stdcall ZwMapViewOfSection(IN HANDLE SectionHandle,IN HANDLE ProcessHandle,
                  IN OUT PVOID *BaseAddress,IN ULONG ZeroBits,
                  IN ULONG CommitSize,IN OUT PLARGE_INTEGER SectionOffset OPTIONAL,
                  IN OUT PULONG ViewSize,IN SECTION_INHERIT InheritDisposition,
                  IN ULONG AllocationType,IN ULONG Protect);

NTSTATUS __stdcall ZwUnmapViewOfSection(IN HANDLE ProcessHandle,IN PVOID BaseAddress);

NTSTATUS __stdcall ZwQueryDirectoryFile(IN HANDLE FileHandle,IN HANDLE Event OPTIONAL,IN PIO_APC_ROUTINE ApcRoutine OPTIONAL,
                                          IN PVOID ApcContext OPTIONAL,OUT PIO_STATUS_BLOCK IoStatusBlock,OUT PVOID FileInformation,
                                          IN ULONG FileInformationLength,IN FILE_INFORMATION_CLASS FileInformationClass,
                                          IN BOOLEAN ReturnSingleEntry,IN PUNICODE_STRING FileName OPTIONAL,IN BOOLEAN RestartScan);

NTSTATUS __stdcall ZwQueryEaFile(IN HANDLE FileHandle,OUT PIO_STATUS_BLOCK IoStatusBlock,
                                OUT PFILE_FULL_EA_INFORMATION Buffer,IN ULONG BufferLength,
                                IN BOOLEAN ReturnSingleEntry,IN PFILE_GET_EA_INFORMATION EaList OPTIONAL,
                                IN ULONG EaListLength,IN PULONG EaIndex OPTIONAL,IN BOOLEAN RestartScan);

NTSTATUS __stdcall ZwSetEaFile(IN HANDLE FileHandle,OUT PIO_STATUS_BLOCK IoStatusBlock,
                                OUT PFILE_FULL_EA_INFORMATION Buffer,IN ULONG BufferLength);

NTSTATUS __stdcall ZwNotifyChangeDirectoryFile(IN HANDLE FileHandle,IN HANDLE Event OPTIONAL,IN PIO_APC_ROUTINE ApcRoutine OPTIONAL,
                                                 IN PVOID ApcContext OPTIONAL,OUT PIO_STATUS_BLOCK IoStatusBlock,OUT PFILE_NOTIFY_INFORMATION Buffer,
                                                 IN ULONG BufferLength,IN ULONG NotifyFilter,IN BOOLEAN WatchSubtree);

NTSTATUS __stdcall LdrLoadDll (IN PWSTR DllPath OPTIONAL,
                                 IN PULONG DllCharacteristics OPTIONAL,
                                 IN PUNICODE_STRING DllName,OUT PVOID *DllHandle);

NTSTATUS __stdcall LdrGetProcedureAddress(
                                          IN PVOID DllHandle,
                                          IN PANSI_STRING ProcedureName OPTIONAL,
                                          IN ULONG ProcedureNumber OPTIONAL,
                                          OUT PVOID *ProcedureAddress
                                          );

NTSTATUS __stdcall ZwWaitForSingleObject(
                                         IN HANDLE Handle,
                                         IN BOOLEAN Alertable,
                                         IN PLARGE_INTEGER Timeout OPTIONAL
                                         );


NTSTATUS __stdcall ZwCreateEvent(
                                 OUT PHANDLE EventHandle,
                                 IN ACCESS_MASK DesiredAccess,
                                 IN POBJECT_ATTRIBUTES ObjectAttributes,
                                 IN EVENT_TYPE EventType,
                                 IN BOOLEAN InitialState
                                 );

NTSTATUS __stdcall ZwSetEvent(
                              IN HANDLE EventHandle,
                              OUT PULONG PreviousState OPTIONAL
                              );

NTSTATUS __stdcall ZwCreateMutant(
                                  OUT PHANDLE MutantHandle,
                                  IN ACCESS_MASK DesiredAccess,
                                  IN POBJECT_ATTRIBUTES ObjectAttributes,
                                  IN BOOLEAN InitialOwner
                                  );

NTSTATUS __stdcall ZwOpenMutant(
                                OUT PHANDLE MutantHandle,
                                IN ACCESS_MASK DesiredAccess,
                                IN POBJECT_ATTRIBUTES ObjectAttributes
                                );

NTSTATUS __stdcall ZwFsControlFile(
                                   IN HANDLE FileHandle,
                                   IN HANDLE Event OPTIONAL,
                                   IN PIO_APC_ROUTINE ApcRoutine OPTIONAL,
                                   IN PVOID ApcContext OPTIONAL,
                                   OUT PIO_STATUS_BLOCK IoStatusBlock,
                                   IN ULONG FsControlCode,
                                   IN PVOID InputBuffer OPTIONAL,
                                   IN ULONG InputBufferLength,
                                   OUT PVOID OutputBuffer OPTIONAL,
                                   IN ULONG OutputBufferLength
                                   );

NTSTATUS __stdcall ZwQueryEaFile(
                                 IN HANDLE FileHandle,
                                 OUT PIO_STATUS_BLOCK IoStatusBlock,
                                 OUT PFILE_FULL_EA_INFORMATION Buffer,
                                 IN ULONG BufferLength,
                                 IN BOOLEAN ReturnSingleEntry,
                                 IN PFILE_GET_EA_INFORMATION EaList OPTIONAL,
                                 IN ULONG EaListLength,
                                 IN PULONG EaIndex OPTIONAL,
                                 IN BOOLEAN RestartScan
                                 );

NTSTATUS __stdcall ZwSetEaFile(
                               IN HANDLE FileHandle,
                               OUT PIO_STATUS_BLOCK IoStatusBlock,
                               IN PFILE_FULL_EA_INFORMATION Buffer,
                               IN ULONG BufferLength
                               );

NTSTATUS __stdcall ZwDeviceIoControlFile(
										 IN HANDLE FileHandle,
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
#ifdef __cplusplus
 }
#endif /* __cplusplus */

 
class FastCriticalSection
{
private:
	HANDLE _hEvent;
	volatile LONG  _uLockCount;
public:
	inline FastCriticalSection():
	_uLockCount(0)
    {ZwCreateEvent(&_hEvent,EVENT_ALL_ACCESS,NULL,SynchronizationEvent,FALSE);}
	inline ~FastCriticalSection(){ZwClose(_hEvent);}
	inline void Enter()
    {
		if(InterlockedIncrement((LPLONG)&_uLockCount)>1)
			ZwWaitForSingleObject(_hEvent,FALSE,NULL);
    }
	inline void Leave()
    {
		if(InterlockedDecrement((LPLONG)&_uLockCount))
			ZwSetEvent(_hEvent,NULL);
    }
};

class AutoCriticalSection
{
private:
	FastCriticalSection &_cs;

public:
	inline AutoCriticalSection(FastCriticalSection &cs):_cs(cs)
	{
		_cs.Enter();
	}
	inline ~AutoCriticalSection()
	{
		_cs.Leave();
	}
};

#endif //_NTDLLEXPORTS_H_

