#ifndef _DDKDEFINES_H_
#define _DDKDEFINES_H_
//#include "../AnyFS/ntstatus.h"
typedef ULONG   NTSTATUS;
typedef ULONG   ULONG_PTR;

typedef struct _UNICODE_STRING {
 USHORT  Length;
 USHORT  MaximumLength;
 PWSTR  Buffer;
 } UNICODE_STRING, *PUNICODE_STRING;

typedef struct _IO_STATUS_BLOCK {
 union {
  NTSTATUS Status;
  PVOID Pointer;
  };
 ULONG_PTR Information;
 } IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;

typedef struct _OBJECT_ATTRIBUTES {
 ULONG Length;
 HANDLE RootDirectory;
 PUNICODE_STRING ObjectName;
 ULONG Attributes;
 PSECURITY_DESCRIPTOR SecurityDescriptor;
 PSECURITY_QUALITY_OF_SERVICE SecurityQualityOfService;
 } OBJECT_ATTRIBUTES, *POBJECT_ATTRIBUTES;

#define InitializeObjectAttributes( p, n, a, r, s ) { \
    (p)->Length = sizeof( OBJECT_ATTRIBUTES );          \
    (p)->RootDirectory = r;                             \
    (p)->Attributes = a;                                \
    (p)->ObjectName = n;                                \
    (p)->SecurityDescriptor = s;                        \
    (p)->SecurityQualityOfService = NULL;               \
    }
#define NT_ERROR(Status) ((ULONG)(Status) >> 30 == 3)
#define NT_WARNING(Status) ((ULONG)(Status) >> 30 == 2)
#define NT_SUCCESS(Status) ((NTSTATUS)(Status) >= 0)
#define NT_INFORMATION(Status) ((ULONG)(Status) >> 30 == 1)

typedef VOID (NTAPI *PIO_APC_ROUTINE)(PVOID ApcContext,PIO_STATUS_BLOCK IoStatusBlock,ULONG Reserved);

#define FILE_SUPERSEDE                  0x00000000
#define FILE_OPEN                       0x00000001
#define FILE_CREATE                     0x00000002
#define FILE_OPEN_IF                    0x00000003
#define FILE_OVERWRITE                  0x00000004
#define FILE_OVERWRITE_IF               0x00000005

#define FILE_DIRECTORY_FILE             0x00000001
#define FILE_WRITE_THROUGH              0x00000002
#define FILE_SEQUENTIAL_ONLY            0x00000004
#define FILE_NO_INTERMEDIATE_BUFFERING  0x00000008
#define FILE_NON_DIRECTORY_FILE         0x00000040
#define FILE_SYNCHRONOUS_IO_ALERT       0x00000010
#define FILE_SYNCHRONOUS_IO_NONALERT    0x00000020
#define FILE_CREATE_TREE_CONNECTION     0x00000080


#define STATUS_FILE_IS_A_DIRECTORY       ((NTSTATUS)0xC00000BAL)
#define STATUS_NOT_A_DIRECTORY           ((NTSTATUS)0xC0000103L)
#define STATUS_INVALID_FILE_FOR_SECTION  ((NTSTATUS)0xC0000020L)
#define STATUS_MAPPED_FILE_SIZE_ZERO     ((NTSTATUS)0xC000011EL)
#define STATUS_NOT_SAME_DEVICE           ((NTSTATUS)0xC00000D4L)
#define STATUS_SUCCESS                   ((NTSTATUS)0x00000000L) // ntsubauth
#define STATUS_BUFFER_OVERFLOW           ((NTSTATUS)0x80000005L)
#define STATUS_INFO_LENGTH_MISMATCH      ((NTSTATUS)0xC0000004L)
#define STATUS_NO_SUCH_FILE              ((NTSTATUS)0xC000000FL)
#define STATUS_NO_MORE_FILES             ((NTSTATUS)0x80000006L)
#define STATUS_END_OF_FILE               ((NTSTATUS)0xC0000011L)
#define STATUS_ACCESS_DENIED             ((NTSTATUS)0xC0000022L)
#define STATUS_OBJECT_NAME_NOT_FOUND     ((NTSTATUS)0xC0000034L)
#define STATUS_DATA_ERROR                ((NTSTATUS)0xC000003EL)
#define STATUS_OBJECT_NAME_COLLISION     ((NTSTATUS)0xC0000035L)
#define STATUS_NOT_SUPPORTED             ((NTSTATUS)0xC00000BBL)


#define FILE_ATTRIBUTE_READONLY             0x00000001  // winnt
#define FILE_ATTRIBUTE_HIDDEN               0x00000002  // winnt
#define FILE_ATTRIBUTE_SYSTEM               0x00000004  // winnt
//OLD DOS VOLID                             0x00000008

#define FILE_ATTRIBUTE_DIRECTORY            0x00000010  // winnt
#define FILE_ATTRIBUTE_ARCHIVE              0x00000020  // winnt
#define FILE_ATTRIBUTE_DEVICE               0x00000040  // winnt
#define FILE_ATTRIBUTE_NORMAL               0x00000080  // winnt

#define FILE_ATTRIBUTE_TEMPORARY            0x00000100  // winnt
#define FILE_ATTRIBUTE_SPARSE_FILE          0x00000200  // winnt
#define FILE_ATTRIBUTE_REPARSE_POINT        0x00000400  // winnt
#define FILE_ATTRIBUTE_COMPRESSED           0x00000800  // winnt

#define FILE_ATTRIBUTE_OFFLINE              0x00001000  // winnt
#define FILE_ATTRIBUTE_NOT_CONTENT_INDEXED  0x00002000  // winnt
//#define FILE_ATTRIBUTE_ENCRYPTED            0x00004000  // winnt

#define FILE_SUPERSEDED                 0x00000000
#define FILE_OPENED                     0x00000001
#define FILE_CREATED                    0x00000002
#define FILE_OVERWRITTEN                0x00000003
#define FILE_EXISTS                     0x00000004
#define FILE_DOES_NOT_EXIST             0x00000005


typedef enum _FILE_INFORMATION_CLASS {
 FileDirectoryInformation = 1, // 1 Y N D
  FileFullDirectoryInformation, // 2 Y N D
  FileBothDirectoryInformation, // 3 Y N D
  FileBasicInformation, // 4 Y Y F
  FileStandardInformation, // 5 Y N F
  FileInternalInformation, // 6 Y N F
  FileEaInformation, // 7 Y N F
  FileAccessInformation, // 8 Y N F
  FileNameInformation, // 9 Y N F
  FileRenameInformation, // 10 N Y F
  FileLinkInformation, // 11 N Y F
  FileNamesInformation, // 12 Y N D
  FileDispositionInformation, // 13 N Y F
  FilePositionInformation, // 14 Y Y F
  FileModeInformation = 16, // 16 Y Y F
  FileAlignmentInformation, // 17 Y N F
  FileAllInformation, // 18 Y N F
  FileAllocationInformation, // 19 N Y F
  FileEndOfFileInformation, // 20 N Y F
  FileAlternateNameInformation, // 21 Y N F
  FileStreamInformation, // 22 Y N F
  FilePipeInformation, // 23 Y Y F
  FilePipeLocalInformation, // 24 Y N F
  FilePipeRemoteInformation, // 25 Y Y F
  FileMailslotQueryInformation, // 26 Y N F
  FileMailslotSetInformation, // 27 N Y F
  FileCompressionInformation, // 28 Y N F
  FileObjectIdInformation, // 29 Y Y F
  FileCompletionInformation, // 30 N Y F
  FileMoveClusterInformation, // 31 N Y F
  FileQuotaInformation, // 32 Y Y F
  FileReparsePointInformation, // 33 Y N F
  FileNetworkOpenInformation, // 34 Y N F
  FileAttributeTagInformation, // 35 Y N F
  FileTrackingInformation, // 36 N Y F
  FileIdBothDirectoryInformation, // 37
  FileIdFullDirectoryInformation, // 38
  FileValidDataLengthInformation, // 39
  FileShortNameInformation,       // 40
  FileMaximumInformation
 } FILE_INFORMATION_CLASS, *PFILE_INFORMATION_CLASS;

typedef struct _FILE_DIRECTORY_INFORMATION { // Information Class 1
 ULONG NextEntryOffset;
 ULONG Unknown;
 LARGE_INTEGER CreationTime;
 LARGE_INTEGER LastAccessTime;
 LARGE_INTEGER LastWriteTime;
 LARGE_INTEGER ChangeTime;
 LARGE_INTEGER EndOfFile;
 LARGE_INTEGER AllocationSize;
 ULONG FileAttributes;
 ULONG FileNameLength;
 WCHAR FileName[1];
 } FILE_DIRECTORY_INFORMATION, *PFILE_DIRECTORY_INFORMATION;

typedef struct _FILE_FULL_DIRECTORY_INFORMATION { // Information Class 2
 ULONG NextEntryOffset;
 ULONG Unknown;
 LARGE_INTEGER CreationTime;
 LARGE_INTEGER LastAccessTime;
 LARGE_INTEGER LastWriteTime;
 LARGE_INTEGER ChangeTime;
 LARGE_INTEGER EndOfFile;
 LARGE_INTEGER AllocationSize;
 ULONG FileAttributes;
 ULONG FileNameLength;
 ULONG EaInformationLength;
 WCHAR FileName[1];
 } FILE_FULL_DIRECTORY_INFORMATION, *PFILE_FULL_DIRECTORY_INFORMATION;


typedef struct _FILE_BOTH_DIRECTORY_INFORMATION { // Information Class 3
 ULONG NextEntryOffset;
 ULONG Unknown;
 LARGE_INTEGER CreationTime;
 LARGE_INTEGER LastAccessTime;
 LARGE_INTEGER LastWriteTime;
 LARGE_INTEGER ChangeTime;
 LARGE_INTEGER EndOfFile;
 LARGE_INTEGER AllocationSize;
 ULONG FileAttributes;
 ULONG FileNameLength;
 ULONG EaInformationLength;
 UCHAR AlternateNameLength;
 WCHAR AlternateName[12];
 WCHAR FileName[1];
 } FILE_BOTH_DIRECTORY_INFORMATION, *PFILE_BOTH_DIRECTORY_INFORMATION;

typedef struct _FILE_ID_FULL_DIRECTORY_INFORMATION {
		ULONG	          NextEntryOffset;
		ULONG	          FileIndex;
		LARGE_INTEGER   CreationTime;
		LARGE_INTEGER   LastAccessTime;
		LARGE_INTEGER   LastWriteTime;
		LARGE_INTEGER   ChangeTime;
		LARGE_INTEGER   EndOfFile;
		LARGE_INTEGER   AllocationSize;
		ULONG           FileAttributes;
		ULONG           FileNameLength;
		ULONG           EaInformationLength;
		LARGE_INTEGER   FileId;
		WCHAR           FileName[0];
} FILE_ID_FULL_DIRECTORY_INFORMATION, *PFILE_ID_FULL_DIRECTORY_INFORMATION;

typedef struct _FILE_ID_BOTH_DIRECTORY_INFORMATION {
		ULONG         NextEntryOffset;
		ULONG	        FileIndex;
		LARGE_INTEGER CreationTime;
		LARGE_INTEGER LastAccessTime;
		LARGE_INTEGER LastWriteTime;
		LARGE_INTEGER ChangeTime;
		LARGE_INTEGER EndOfFile;
		LARGE_INTEGER AllocationSize;
		ULONG         FileAttributes;
		ULONG         FileNameLength;
		ULONG         EaInformationLength;
		CHAR          AlternateNameLength;
		WCHAR         AlternateName[12];
		LARGE_INTEGER FileId;
		WCHAR         FileName[0];
} FILE_ID_BOTH_DIRECTORY_INFORMATION, *PFILE_ID_BOTH_DIRECTORY_INFORMATION;


typedef struct _FILE_BASIC_INFORMATION { // Information Class 4
 LARGE_INTEGER CreationTime;
 LARGE_INTEGER LastAccessTime;
 LARGE_INTEGER LastWriteTime;
 LARGE_INTEGER ChangeTime;
 ULONG FileAttributes;
 } FILE_BASIC_INFORMATION, *PFILE_BASIC_INFORMATION;

typedef struct _FILE_STANDARD_INFORMATION { // Information Class 5
 LARGE_INTEGER AllocationSize;
 LARGE_INTEGER EndOfFile;
 ULONG NumberOfLinks;
 BOOLEAN DeletePending;
 BOOLEAN Directory;
 } FILE_STANDARD_INFORMATION, *PFILE_STANDARD_INFORMATION;

typedef struct _FILE_INTERNAL_INFORMATION { // Information Class 6
 LARGE_INTEGER FileId;
 } FILE_INTERNAL_INFORMATION, *PFILE_INTERNAL_INFORMATION;

typedef struct _FILE_EA_INFORMATION { // Information Class 7
 ULONG EaInformationLength;
 } FILE_EA_INFORMATION, *PFILE_EA_INFORMATION;

typedef struct _FILE_ACCESS_INFORMATION { // Information Class 8
 ACCESS_MASK GrantedAccess;
 } FILE_ACCESS_INFORMATION, *PFILE_ACCESS_INFORMATION;

typedef struct _FILE_NAME_INFORMATION { // Information Classes 9 and 21
 ULONG FileNameLength;
 WCHAR FileName[1];
 } FILE_NAME_INFORMATION, *PFILE_NAME_INFORMATION,
  FILE_ALTERNATE_NAME_INFORMATION, *PFILE_ALTERNATE_NAME_INFORMATION;
 
 typedef struct _FILE_LINK_RENAME_INFORMATION { // Info Classes 10 and 11
  BOOLEAN ReplaceIfExists;
  HANDLE RootDirectory;
  ULONG FileNameLength;
  WCHAR FileName[1];
  } FILE_LINK_INFORMATION, *PFILE_LINK_INFORMATION,
   FILE_RENAME_INFORMATION, *PFILE_RENAME_INFORMATION;
  
  typedef struct _FILE_NAMES_INFORMATION { // Information Class 12
   ULONG NextEntryOffset;
   ULONG Unknown;
   ULONG FileNameLength;
   WCHAR FileName[1];
   } FILE_NAMES_INFORMATION, *PFILE_NAMES_INFORMATION;
  
  typedef struct _FILE_DISPOSITION_INFORMATION { // Information Class 13
   BOOLEAN DeleteFile;
   } FILE_DISPOSITION_INFORMATION, *PFILE_DISPOSITION_INFORMATION;
  
  typedef struct _FILE_POSITION_INFORMATION { // Information Class 14
   LARGE_INTEGER CurrentByteOffset;
   } FILE_POSITION_INFORMATION, *PFILE_POSITION_INFORMATION;
  
  typedef struct _FILE_MODE_INFORMATION { // Information Class 16
   ULONG Mode;
   } FILE_MODE_INFORMATION, *PFILE_MODE_INFORMATION;
  
  typedef struct _FILE_ALIGNMENT_INFORMATION { // Information Class 17
   ULONG AlignmentRequirement;
   } FILE_ALIGNMENT_INFORMATION, *PFILE_ALIGNMENT_INFORMATION;
  
  typedef struct _FILE_ALL_INFORMATION { // Information Class 18
   FILE_BASIC_INFORMATION BasicInformation;
   FILE_STANDARD_INFORMATION StandardInformation;
   FILE_INTERNAL_INFORMATION InternalInformation;
   FILE_EA_INFORMATION EaInformation;
   FILE_ACCESS_INFORMATION AccessInformation;
   FILE_POSITION_INFORMATION PositionInformation;
   FILE_MODE_INFORMATION ModeInformation;
   FILE_ALIGNMENT_INFORMATION AlignmentInformation;
   FILE_NAME_INFORMATION NameInformation;
   } FILE_ALL_INFORMATION, *PFILE_ALL_INFORMATION;
  
  typedef struct _FILE_ALLOCATION_INFORMATION { // Information Class 19
   LARGE_INTEGER AllocationSize;
   } FILE_ALLOCATION_INFORMATION, *PFILE_ALLOCATION_INFORMATION;
  
  typedef struct _FILE_END_OF_FILE_INFORMATION { // Information Class 20
   LARGE_INTEGER EndOfFile;
   } FILE_END_OF_FILE_INFORMATION, *PFILE_END_OF_FILE_INFORMATION;
  
  typedef struct _FILE_STREAM_INFORMATION { // Information Class 22
   ULONG NextEntryOffset;
   ULONG StreamNameLength;
   LARGE_INTEGER EndOfStream;
   LARGE_INTEGER AllocationSize;
   WCHAR StreamName[1];
   } FILE_STREAM_INFORMATION, *PFILE_STREAM_INFORMATION;
  
  typedef struct _FILE_PIPE_INFORMATION { // Information Class 23
   ULONG ReadModeMessage;
   ULONG WaitModeBlocking;
   } FILE_PIPE_INFORMATION, *PFILE_PIPE_INFORMATION;
  
  typedef struct _FILE_PIPE_LOCAL_INFORMATION { // Information Class 24
   ULONG MessageType;
   ULONG Unknown1;
   ULONG MaxInstances;
   ULONG CurInstances;
   ULONG InBufferSize;
   ULONG Unknown2;
   ULONG OutBufferSize;
   ULONG Unknown3[2];
   ULONG ServerEnd;
   } FILE_PIPE_LOCAL_INFORMATION, *PFILE_PIPE_LOCAL_INFORMATION;
  
  typedef struct _FILE_PIPE_REMOTE_INFORMATION { // Information Class 25
   LARGE_INTEGER CollectDataTimeout;
   ULONG MaxCollectionCount;
   } FILE_PIPE_REMOTE_INFORMATION, *PFILE_PIPE_REMOTE_INFORMATION;
  
  typedef struct _FILE_MAILSLOT_QUERY_INFORMATION { // Information Class 26
   ULONG MaxMessageSize;
   ULONG Unknown;
   ULONG NextSize;
   ULONG MessageCount;
   LARGE_INTEGER ReadTimeout;
   } FILE_MAILSLOT_QUERY_INFORMATION, *PFILE_MAILSLOT_QUERY_INFORMATION;
  
  typedef struct _FILE_MAILSLOT_SET_INFORMATION { // Information Class 27
   LARGE_INTEGER ReadTimeout;
   } FILE_MAILSLOT_SET_INFORMATION, *PFILE_MAILSLOT_SET_INFORMATION;
  
  typedef struct _FILE_COMPRESSION_INFORMATION { // Information Class 28
   LARGE_INTEGER CompressedSize;
   USHORT CompressionFormat;
   UCHAR CompressionUnitShift;
   UCHAR Unknown;
   UCHAR ClusterSizeShift;
   } FILE_COMPRESSION_INFORMATION, *PFILE_COMPRESSION_INFORMATION;
  
  typedef struct _FILE_COMPLETION_INFORMATION { // Information Class 30
   HANDLE IoCompletionHandle;
   ULONG CompletionKey;
   } FILE_COMPLETION_INFORMATION, *PFILE_COMPLETION_INFORMATION;
  
  typedef struct _FILE_NETWORK_OPEN_INFORMATION { // Information Class 34
   LARGE_INTEGER CreationTime;
   LARGE_INTEGER LastAccessTime;
   LARGE_INTEGER LastWriteTime;
   LARGE_INTEGER ChangeTime;
   LARGE_INTEGER AllocationSize;
   LARGE_INTEGER EndOfFile;
   ULONG FileAttributes;
   } FILE_NETWORK_OPEN_INFORMATION, *PFILE_NETWORK_OPEN_INFORMATION;
  
  typedef struct _FILE_ATTRIBUTE_TAG_INFORMATION {// Information Class 35
   ULONG FileAttributes;
   ULONG ReparseTag;
   } FILE_ATTRIBUTE_TAG_INFORMATION, *PFILE_ATTRIBUTE_TAG_INFORMATION;
  
  
  typedef enum _PROCESSINFOCLASS {
   ProcessBasicInformation,
    ProcessQuotaLimits,
    ProcessIoCounters,
    ProcessVmCounters,
    ProcessTimes,
    ProcessBasePriority,
    ProcessRaisePriority,
    ProcessDebugPort,
    ProcessExceptionPort,
    ProcessAccessToken,
    ProcessLdtInformation,
    ProcessLdtSize,
    ProcessDefaultHardErrorMode,
    ProcessIoPortHandlers,          // Note: this is kernel mode only
    ProcessPooledUsageAndLimits,
    ProcessWorkingSetWatch,
    ProcessUserModeIOPL,
    ProcessEnableAlignmentFaultFixup,
    ProcessPriorityClass,
    ProcessWx86Information,
    ProcessHandleCount,
    ProcessAffinityMask,
    ProcessPriorityBoost,
    ProcessDeviceMap,
    ProcessSessionInformation,
    ProcessForegroundInformation,
    ProcessWow64Information,
    MaxProcessInfoClass
   } PROCESSINFOCLASS;
  
  
  typedef struct _PROCESS_DEVICEMAP_INFORMATION {
   union {
    struct {
     HANDLE DirectoryHandle;
     } Set;
    struct {
     ULONG DriveMap;
     UCHAR DriveType[ 32 ];
     } Query;
    };
   } PROCESS_DEVICEMAP_INFORMATION, *PPROCESS_DEVICEMAP_INFORMATION;
   
#define FILE_BYTE_ALIGNMENT             0x00000000
#define FILE_WORD_ALIGNMENT             0x00000001
#define FILE_LONG_ALIGNMENT             0x00000003
#define FILE_QUAD_ALIGNMENT             0x00000007
#define FILE_OCTA_ALIGNMENT             0x0000000f
#define FILE_32_BYTE_ALIGNMENT          0x0000001f
#define FILE_64_BYTE_ALIGNMENT          0x0000003f
#define FILE_128_BYTE_ALIGNMENT         0x0000007f
#define FILE_256_BYTE_ALIGNMENT         0x000000ff
#define FILE_512_BYTE_ALIGNMENT         0x000001ff
   
#undef DEVICE_TYPE   
#define DEVICE_TYPE ULONG

#define FILE_DEVICE_BEEP                0x00000001
#define FILE_DEVICE_CD_ROM              0x00000002
#define FILE_DEVICE_CD_ROM_FILE_SYSTEM  0x00000003
#define FILE_DEVICE_CONTROLLER          0x00000004
#define FILE_DEVICE_DATALINK            0x00000005
#define FILE_DEVICE_DFS                 0x00000006
#define FILE_DEVICE_DISK                0x00000007
#define FILE_DEVICE_DISK_FILE_SYSTEM    0x00000008
#define FILE_DEVICE_FILE_SYSTEM         0x00000009
#define FILE_DEVICE_INPORT_PORT         0x0000000a
#define FILE_DEVICE_KEYBOARD            0x0000000b
#define FILE_DEVICE_MAILSLOT            0x0000000c
#define FILE_DEVICE_MIDI_IN             0x0000000d
#define FILE_DEVICE_MIDI_OUT            0x0000000e
#define FILE_DEVICE_MOUSE               0x0000000f
#define FILE_DEVICE_MULTI_UNC_PROVIDER  0x00000010
#define FILE_DEVICE_NAMED_PIPE          0x00000011
#define FILE_DEVICE_NETWORK             0x00000012
#define FILE_DEVICE_NETWORK_BROWSER     0x00000013
#define FILE_DEVICE_NETWORK_FILE_SYSTEM 0x00000014
#define FILE_DEVICE_NULL                0x00000015
#define FILE_DEVICE_PARALLEL_PORT       0x00000016
#define FILE_DEVICE_PHYSICAL_NETCARD    0x00000017
#define FILE_DEVICE_PRINTER             0x00000018
#define FILE_DEVICE_SCANNER             0x00000019
#define FILE_DEVICE_SERIAL_MOUSE_PORT   0x0000001a
#define FILE_DEVICE_SERIAL_PORT         0x0000001b
#define FILE_DEVICE_SCREEN              0x0000001c
#define FILE_DEVICE_SOUND               0x0000001d
#define FILE_DEVICE_STREAMS             0x0000001e
#define FILE_DEVICE_TAPE                0x0000001f
#define FILE_DEVICE_TAPE_FILE_SYSTEM    0x00000020
#define FILE_DEVICE_TRANSPORT           0x00000021
#define FILE_DEVICE_UNKNOWN             0x00000022
#define FILE_DEVICE_VIDEO               0x00000023
#define FILE_DEVICE_VIRTUAL_DISK        0x00000024
#define FILE_DEVICE_WAVE_IN             0x00000025
#define FILE_DEVICE_WAVE_OUT            0x00000026
#define FILE_DEVICE_8042_PORT           0x00000027
#define FILE_DEVICE_NETWORK_REDIRECTOR  0x00000028
#define FILE_DEVICE_BATTERY             0x00000029
#define FILE_DEVICE_BUS_EXTENDER        0x0000002a
#define FILE_DEVICE_MODEM               0x0000002b
#define FILE_DEVICE_VDM                 0x0000002c
#define FILE_DEVICE_MASS_STORAGE        0x0000002d
#define FILE_DEVICE_SMB                 0x0000002e
#define FILE_DEVICE_KS                  0x0000002f
#define FILE_DEVICE_CHANGER             0x00000030
#define FILE_DEVICE_SMARTCARD           0x00000031
#define FILE_DEVICE_ACPI                0x00000032
#define FILE_DEVICE_DVD                 0x00000033
#define FILE_DEVICE_FULLSCREEN_VIDEO    0x00000034
#define FILE_DEVICE_DFS_FILE_SYSTEM     0x00000035
#define FILE_DEVICE_DFS_VOLUME          0x00000036
#define FILE_DEVICE_SERENUM             0x00000037
#define FILE_DEVICE_TERMSRV             0x00000038
#define FILE_DEVICE_KSEC                0x00000039

   
   typedef enum _FSINFOCLASS {
    FileFsVolumeInformation = 1, // 1 Y N
     FileFsLabelInformation, // 2 N Y
     FileFsSizeInformation, // 3 Y N
     FileFsDeviceInformation, // 4 Y N
     FileFsAttributeInformation, // 5 Y N
     FileFsControlInformation, // 6 Y Y
     FileFsFullSizeInformation, // 7 Y N
     FileFsObjectIdInformation // 8 Y Y
    } FS_INFORMATION_CLASS, *PFS_INFORMATION_CLASS;
   
   
  
  typedef struct _FILE_FS_VOLUME_INFORMATION {
   LARGE_INTEGER VolumeCreationTime;
   ULONG VolumeSerialNumber;
   ULONG VolumeLabelLength;
   UCHAR Unknown;
   WCHAR VolumeLabel[1];
   } FILE_FS_VOLUME_INFORMATION, *PFILE_FS_VOLUME_INFORMATION;
  
  
  typedef struct _FILE_FS_LABEL_INFORMATION {
   ULONG VolumeLabelLength;
   WCHAR VolumeLabel;
   } FILE_FS_LABEL_INFORMATION, *PFILE_FS_LABEL_INFORMATION;
  
  typedef struct _FILE_FS_SIZE_INFORMATION {
   LARGE_INTEGER TotalAllocationUnits;
   LARGE_INTEGER AvailableAllocationUnits;
   ULONG SectorsPerAllocationUnit;
   ULONG BytesPerSector;
   } FILE_FS_SIZE_INFORMATION, *PFILE_FS_SIZE_INFORMATION;
  
  typedef struct _FILE_FS_DEVICE_INFORMATION {
   DEVICE_TYPE DeviceType;
   ULONG Characteristics;
   } FILE_FS_DEVICE_INFORMATION, *PFILE_FS_DEVICE_INFORMATION;
  
  typedef struct _FILE_FS_ATTRIBUTE_INFORMATION {
   ULONG FileSystemFlags;
   ULONG MaximumComponentNameLength;
   ULONG FileSystemNameLength;
   WCHAR FileSystemName[1];
   } FILE_FS_ATTRIBUTE_INFORMATION, *PFILE_FS_ATTRIBUTE_INFORMATION;
  
  typedef struct _FILE_FS_CONTROL_INFORMATION {
   LARGE_INTEGER Reserved[3];
   LARGE_INTEGER DefaultQuotaThreshold;
   LARGE_INTEGER DefaultQuotaLimit;
   ULONG QuotaFlags;
   } FILE_FS_CONTROL_INFORMATION, *PFILE_FS_CONTROL_INFORMATION;
  
  typedef struct _FILE_FS_FULL_SIZE_INFORMATION {
   LARGE_INTEGER TotalQuotaAllocationUnits;
   LARGE_INTEGER AvailableQuotaAllocationUnits;
   LARGE_INTEGER AvailableAllocationUnits;
   ULONG SectorsPerAllocationUnit;
   ULONG BytesPerSector;
   } FILE_FS_FULL_SIZE_INFORMATION, *PFILE_FS_FULL_SIZE_INFORMATION;
  
  typedef struct _FILE_FS_OBJECT_ID_INFORMATION {
   GUID VolumeObjectId;
   ULONG VolumeObjectIdExtendedInfo[12];
   } FILE_FS_OBJECT_ID_INFORMATION, *PFILE_FS_OBJECT_ID_INFORMATION;

#define FILE_REMOVABLE_MEDIA            0x00000001
#define FILE_READ_ONLY_DEVICE           0x00000002
#define FILE_FLOPPY_DISKETTE            0x00000004
#define FILE_WRITE_ONCE_MEDIA           0x00000008
#define FILE_REMOTE_DEVICE              0x00000010
#define FILE_DEVICE_IS_MOUNTED          0x00000020
#define FILE_VIRTUAL_VOLUME             0x00000040
#define FILE_AUTOGENERATED_DEVICE_NAME  0x00000080
#define FILE_DEVICE_SECURE_OPEN         0x00000100
  
  typedef struct _FILE_FULL_EA_INFORMATION {
   ULONG NextEntryOffset;
   UCHAR Flags;
   UCHAR EaNameLength;
   USHORT EaValueLength;
   CHAR EaName[1];
   } FILE_FULL_EA_INFORMATION, *PFILE_FULL_EA_INFORMATION;
  
  typedef struct _FILE_GET_EA_INFORMATION {
   ULONG NextEntryOffset;
   UCHAR EaNameLength;
   CHAR EaName[1];
   } FILE_GET_EA_INFORMATION, *PFILE_GET_EA_INFORMATION;
  

  /*typedef struct _FILE_NOTIFY_INFORMATION {
   ULONG NextEntryOffset;
   ULONG Action;
   ULONG NameLength;
   ULONG Name[1];
   } FILE_NOTIFY_INFORMATION, *PFILE_NOTIFY_INFORMATION;*/

  typedef enum _SECTION_INHERIT {
   ViewShare = 1,
    ViewUnmap = 2
   } SECTION_INHERIT;
  
#define OBJ_INHERIT             0x00000002L
#define OBJ_PERMANENT           0x00000010L
#define OBJ_EXCLUSIVE           0x00000020L
#define OBJ_CASE_INSENSITIVE    0x00000040L
#define OBJ_OPENIF              0x00000080L
#define OBJ_OPENLINK            0x00000100L
#define OBJ_KERNEL_HANDLE       0x00000200L
#define OBJ_VALID_ATTRIBUTES    0x000003F2L

#define FILE_ANY_ACCESS                 0
#define FILE_SPECIAL_ACCESS    (FILE_ANY_ACCESS)
#define FILE_READ_ACCESS          ( 0x0001 )    // file & pipe
#define FILE_WRITE_ACCESS         ( 0x0002 )    // file & pipe

  #define FILE_READ_DATA            ( 0x0001 )    // file & pipe
#define FILE_LIST_DIRECTORY       ( 0x0001 )    // directory

#define FILE_WRITE_DATA           ( 0x0002 )    // file & pipe
#define FILE_ADD_FILE             ( 0x0002 )    // directory

#define FILE_APPEND_DATA          ( 0x0004 )    // file
#define FILE_ADD_SUBDIRECTORY     ( 0x0004 )    // directory
#define FILE_CREATE_PIPE_INSTANCE ( 0x0004 )    // named pipe


#define FILE_READ_EA              ( 0x0008 )    // file & directory

#define FILE_WRITE_EA             ( 0x0010 )    // file & directory

#define FILE_EXECUTE              ( 0x0020 )    // file
#define FILE_TRAVERSE             ( 0x0020 )    // directory

#define FILE_DELETE_CHILD         ( 0x0040 )    // directory

#define FILE_READ_ATTRIBUTES      ( 0x0080 )    // all

#define FILE_WRITE_ATTRIBUTES     ( 0x0100 )    // all


#define DELETE                           (0x00010000L)
#define READ_CONTROL                     (0x00020000L)
#define WRITE_DAC                        (0x00040000L)
#define WRITE_OWNER                      (0x00080000L)
#define SYNCHRONIZE                      (0x00100000L)

#define STANDARD_RIGHTS_REQUIRED         (0x000F0000L)

#define STANDARD_RIGHTS_READ             (READ_CONTROL)
#define STANDARD_RIGHTS_WRITE            (READ_CONTROL)
#define STANDARD_RIGHTS_EXECUTE          (READ_CONTROL)

#define STANDARD_RIGHTS_ALL              (0x001F0000L)

#define SPECIFIC_RIGHTS_ALL              (0x0000FFFFL)

//
// AccessSystemAcl access type
//

#define ACCESS_SYSTEM_SECURITY           (0x01000000L)

//
// MaximumAllowed access type
//

#define MAXIMUM_ALLOWED                  (0x02000000L)

//
//  These are the generic rights.

//#define FILE_ALL_ACCESS (STANDARD_RIGHTS_REQUIRED | SYNCHRONIZE | 0x1FF)

#define FILE_GENERIC_READ         (STANDARD_RIGHTS_READ     |\
                                   FILE_READ_DATA           |\
                                   FILE_READ_ATTRIBUTES     |\
                                   FILE_READ_EA             |\
                                   SYNCHRONIZE)


#define FILE_GENERIC_WRITE        (STANDARD_RIGHTS_WRITE    |\
                                   FILE_WRITE_DATA          |\
                                   FILE_WRITE_ATTRIBUTES    |\
                                   FILE_WRITE_EA            |\
                                   FILE_APPEND_DATA         |\
                                   SYNCHRONIZE)


#define FILE_GENERIC_EXECUTE      (STANDARD_RIGHTS_EXECUTE  |\
                                   FILE_READ_ATTRIBUTES     |\
                                   FILE_EXECUTE             |\
                                   SYNCHRONIZE)

typedef struct _STRING {
    USHORT Length;
    USHORT MaximumLength;
    PCHAR Buffer;
} STRING;

typedef enum _EVENT_TYPE {
 NotificationEvent, // A manual-reset event
  SynchronizationEvent // An auto-reset event
 } EVENT_TYPE;

typedef STRING *PSTRING;
typedef STRING ANSI_STRING;
typedef PSTRING PANSI_STRING;


#define METHOD_BUFFERED                 0
#define METHOD_IN_DIRECT                1
#define METHOD_OUT_DIRECT               2
#define METHOD_NEITHER                  3

#define CTL_CODE( DeviceType, Function, Method, Access ) (                 \
    ((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method) \
)


#define FSCTL_MAILSLOT_PEEK             CTL_CODE(FILE_DEVICE_MAILSLOT, 0, METHOD_NEITHER, FILE_READ_DATA)

#define FSCTL_NETWORK_SET_CONFIGURATION_INFO    CTL_CODE(FILE_DEVICE_NETWORK_FILE_SYSTEM, 102, METHOD_IN_DIRECT, FILE_ANY_ACCESS)
#define FSCTL_NETWORK_GET_CONFIGURATION_INFO    CTL_CODE(FILE_DEVICE_NETWORK_FILE_SYSTEM, 103, METHOD_OUT_DIRECT, FILE_ANY_ACCESS)
#define FSCTL_NETWORK_GET_CONNECTION_INFO       CTL_CODE(FILE_DEVICE_NETWORK_FILE_SYSTEM, 104, METHOD_NEITHER, FILE_ANY_ACCESS)
#define FSCTL_NETWORK_ENUMERATE_CONNECTIONS     CTL_CODE(FILE_DEVICE_NETWORK_FILE_SYSTEM, 105, METHOD_NEITHER, FILE_ANY_ACCESS)
#define FSCTL_NETWORK_DELETE_CONNECTION         CTL_CODE(FILE_DEVICE_NETWORK_FILE_SYSTEM, 107, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define FSCTL_NETWORK_GET_STATISTICS            CTL_CODE(FILE_DEVICE_NETWORK_FILE_SYSTEM, 116, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define FSCTL_NETWORK_SET_DOMAIN_NAME           CTL_CODE(FILE_DEVICE_NETWORK_FILE_SYSTEM, 120, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define FSCTL_NETWORK_REMOTE_BOOT_INIT_SCRT     CTL_CODE(FILE_DEVICE_NETWORK_FILE_SYSTEM, 250, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define FSCTL_PIPE_ASSIGN_EVENT         CTL_CODE(FILE_DEVICE_NAMED_PIPE, 0, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define FSCTL_PIPE_DISCONNECT           CTL_CODE(FILE_DEVICE_NAMED_PIPE, 1, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define FSCTL_PIPE_LISTEN               CTL_CODE(FILE_DEVICE_NAMED_PIPE, 2, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define FSCTL_PIPE_PEEK                 CTL_CODE(FILE_DEVICE_NAMED_PIPE, 3, METHOD_BUFFERED, FILE_READ_DATA)
#define FSCTL_PIPE_QUERY_EVENT          CTL_CODE(FILE_DEVICE_NAMED_PIPE, 4, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define FSCTL_PIPE_TRANSCEIVE           CTL_CODE(FILE_DEVICE_NAMED_PIPE, 5, METHOD_NEITHER,  FILE_READ_DATA | FILE_WRITE_DATA)
#define FSCTL_PIPE_WAIT                 CTL_CODE(FILE_DEVICE_NAMED_PIPE, 6, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define FSCTL_PIPE_IMPERSONATE          CTL_CODE(FILE_DEVICE_NAMED_PIPE, 7, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define FSCTL_PIPE_SET_CLIENT_PROCESS   CTL_CODE(FILE_DEVICE_NAMED_PIPE, 8, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define FSCTL_PIPE_QUERY_CLIENT_PROCESS CTL_CODE(FILE_DEVICE_NAMED_PIPE, 9, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define FSCTL_PIPE_INTERNAL_READ        CTL_CODE(FILE_DEVICE_NAMED_PIPE, 2045, METHOD_BUFFERED, FILE_READ_DATA)
#define FSCTL_PIPE_INTERNAL_WRITE       CTL_CODE(FILE_DEVICE_NAMED_PIPE, 2046, METHOD_BUFFERED, FILE_WRITE_DATA)
#define FSCTL_PIPE_INTERNAL_TRANSCEIVE  CTL_CODE(FILE_DEVICE_NAMED_PIPE, 2047, METHOD_NEITHER, FILE_READ_DATA | FILE_WRITE_DATA)
#define FSCTL_PIPE_INTERNAL_READ_OVFLOW CTL_CODE(FILE_DEVICE_NAMED_PIPE, 2048, METHOD_BUFFERED, FILE_READ_DATA)

#define IOCTL_REDIR_QUERY_PATH          CTL_CODE(FILE_DEVICE_NETWORK_FILE_SYSTEM, 99, METHOD_NEITHER, FILE_ANY_ACCESS)


typedef struct _FILE_PIPE_WAIT_FOR_BUFFER {
    LARGE_INTEGER   Timeout;
    ULONG           NameLength;
    BOOLEAN         TimeoutSpecified;
    WCHAR           Name[1];
} FILE_PIPE_WAIT_FOR_BUFFER, *PFILE_PIPE_WAIT_FOR_BUFFER;

#endif//_DDKDEFINES_H_