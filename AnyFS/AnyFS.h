#ifndef _ANYFS_H_
#define _ANYFS_H_
///
#define PIPENAME_FTPFS   TEXT("FTPFS")

#define ANYFS_CMD_OPEN               0x01
#define ANYFS_CMD_CLOSE              0x02
#define ANYFS_CMD_READ               0x03
#define ANYFS_CMD_LIST               0x04
#define ANYFS_CMD_INFO               0x05
#define ANYFS_CMD_TEMP               0x06
#define ANYFS_CMD_CHECKAPP           0x07
#define ANYFS_CMD_RENAME             0x08
#define ANYFS_CMD_WRITE              0x09
#define ANYFS_CMD_PATHENFORCE        0x0a

///
#define ANYFS_MAX_PATH               0x400
#define ANYFS_BASE_HEADER_MAX_SIZE   ((ANYFS_MAX_PATH*sizeof(TCHAR))+0x50)//minimum size of header to receive any request/reply headers
///
#define FTPDRIVE_ACTIVE_MUTEX        TEXT("FtpDriveActiveMutex")
#define FTPDRIVE_ACTIVE_MUTANT       TEXT("\\BaseNamedObjects\\")##FTPDRIVE_ACTIVE_MUTEX

typedef struct ANYFS_RAW_REQUEST_HEADER_
 {
 unsigned int code;
 unsigned int pid;
 //unsigned char data[];
 }ANYFS_RAW_REQUEST_HEADER,*LPANYFS_RAW_REQUEST_HEADER;

typedef struct ANYFS_OPEN_REQUEST_
 {
 ANYFS_RAW_REQUEST_HEADER rawhdr;
 unsigned int dwDesiredAccess;
 unsigned int dwCreateDisposition;
 unsigned int dwCreateOptions;
 TCHAR         filename[ANYFS_MAX_PATH]; 
 }ANYFS_OPEN_REQUEST,*LPANYFS_OPEN_REQUEST;

typedef struct ANYFS_CLOSE_REQUEST_
 {
 ANYFS_RAW_REQUEST_HEADER rawhdr;
 unsigned int handle; 
 }ANYFS_CLOSE_REQUEST,*LPANYFS_CLOSE_REQUEST;


typedef struct ANYFS_READ_REQUEST_
 {
 ANYFS_RAW_REQUEST_HEADER rawhdr;
 unsigned int options;//possible values: 0, ANYFS_OPT_NO_CACHE_WRITE
 unsigned int handle;
 unsigned int offset_low;
 unsigned int offset_hi;
 unsigned int max_size;  
 }ANYFS_READ_REQUEST,*LPANYFS_READ_REQUEST;

typedef ANYFS_READ_REQUEST ANYFS_WRITE_REQUEST,*LPANYFS_WRITE_REQUEST;

typedef struct ANYFS_LIST_REQUEST_
 {
 ANYFS_RAW_REQUEST_HEADER rawhdr;
 unsigned int handle;  
 unsigned int index;
 }ANYFS_LIST_REQUEST,*LPANYFS_LIST_REQUEST;

typedef struct ANYFS_INFO_REQUEST_
 {
 ANYFS_RAW_REQUEST_HEADER rawhdr;
 unsigned int handle;
 }ANYFS_INFO_REQUEST,*LPANYFS_INFO_REQUEST;

typedef struct ANYFS_TEMP_REQUEST_
 {
 ANYFS_RAW_REQUEST_HEADER rawhdr;
 unsigned int max_size;//0xffffffff - whole file
 unsigned int handle;
 }ANYFS_TEMP_REQUEST,*LPANYFS_TEMP_REQUEST;

typedef struct ANYFS_CHECKAPP_REQUEST_
 {
 ANYFS_RAW_REQUEST_HEADER rawhdr;
 TCHAR         filename[ANYFS_MAX_PATH];   
 }ANYFS_CHECKAPP_REQUEST,*LPANYFS_CHECKAPP_REQUEST;

typedef struct ANYFS_PATHENFORCE_REQUEST_
 {
 ANYFS_RAW_REQUEST_HEADER rawhdr;
 TCHAR         path[ANYFS_MAX_PATH];   
 }ANYFS_PATHENFORCE_REQUEST,*LPANYFS_PATHENFORCE_REQUEST;

typedef struct ANYFS_RENAME_REQUEST_
 {
 ANYFS_RAW_REQUEST_HEADER rawhdr;
 unsigned int  handle;
 TCHAR         filename[ANYFS_MAX_PATH];//empty for delete
 }ANYFS_RENAME_REQUEST,*LPANYFS_RENAME_REQUEST;
///////////////////////////////////////////////////
typedef struct ANYFS_OPEN_REPLY_
 { 
 unsigned int nt_status;
 unsigned int nt_info;
 unsigned int handle;
 }ANYFS_OPEN_REPLY,*LPANYFS_OPEN_REPLY;

typedef struct ANYFS_CLOSE_REPLY_
 { 
 unsigned int nt_status; 
 }ANYFS_CLOSE_REPLY,*LPANYFS_CLOSE_REPLY;

typedef struct ANYFS_READ_REPLY_HEADER_
 {  
 unsigned int nt_status;
 unsigned int len;
 //unsigned char data[];
 }ANYFS_READ_REPLY_HEADER,*LPANYFS_READ_REPLY_HEADER;

typedef ANYFS_READ_REPLY_HEADER ANYFS_WRITE_REPLY,*LPANYFS_WRITE_REPLY;

typedef struct ANYFS_INFO_REPLY_
 {
 unsigned int   nt_status;  
 unsigned int   attributes;
 LARGE_INTEGER cr_time;
 LARGE_INTEGER wr_time; 
 LARGE_INTEGER ac_time; 
 LARGE_INTEGER size;
 TCHAR           file_name[ANYFS_MAX_PATH];
 }ANYFS_INFO_REPLY,*LPANYFS_INFO_REPLY;

typedef struct ANYFS_TEMP_REPLY_
 {
 unsigned int   nt_status;  
 unsigned int   size;
 TCHAR           file_name[ANYFS_MAX_PATH];
 }ANYFS_TEMP_REPLY,*LPANYFS_TEMP_REPLY;

typedef struct ANYFS_CHECKAPP_REPLY_
 { 
 BOOL           affected;
 }ANYFS_CHECKAPP_REPLY,*LPANYFS_CHECKAPP_REPLY;

typedef struct ANYFS_PATHENFORCE_REPLY_
 { 
 unsigned int   win32_error_code;//0 on success
 }ANYFS_PATHENFORCE_REPLY,*LPANYFS_PATHENFORCE_REPLY;

typedef struct ANYFS_RENAME_REPLY_
 { 
 unsigned int nt_status; 
 }ANYFS_RENAME_REPLY,*LPANYFS_RENAME_REPLY;

#endif// _ANYFS_H_

