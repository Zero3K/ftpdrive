#include "stdafx.h"
#include "TrafficCounter.h"
#include "FtpFS.h"
#include "../AnyFS/AnyFS.h"
#include "../FtpFS/FtpWorks/FtpWorks.h"
#include "../FtpSettings/settings.h"
#include "../DebugLog/DebugLog.h"
#include "PidsWatcher.h"
#include "ErrorHandler.h"
#include "./UI/Dialogs.h"

bool ProceedOpen(HANDLE hPipe, LPANYFS_OPEN_REQUEST Request, DWORD cbBytesRead, DWORD client_pid)
{
	MyWriteDebugLog("ProceedOpen point1 cbBytesRead %u need %u",cbBytesRead,sizeof(ANYFS_OPEN_REQUEST));
	if(cbBytesRead<sizeof(ANYFS_OPEN_REQUEST))
		return false;
	MyWriteDebugLog("ProceedOpen point2");
	Request->filename[ANYFS_MAX_PATH-1]=0;
	std::wstring pathname(Request->filename);
	HANDLE handle=NULL;
	ANYFS_OPEN_REPLY reply={0,0};
	reply.nt_status=FtpWorks::xxxOpenFile(pathname,handle,Request->dwDesiredAccess,Request->dwCreateDisposition,Request->dwCreateOptions);
	
	switch(reply.nt_status)
	{
	case STATUS_SUCCESS:
		reply.nt_info=FILE_OPENED;
		break;
	case STATUS_OBJECT_NAME_COLLISION:case STATUS_ACCESS_DENIED:
	case STATUS_FILE_IS_A_DIRECTORY:case STATUS_NOT_A_DIRECTORY:
		reply.nt_info=FILE_EXISTS;
		break;
	default:
		reply.nt_info=FILE_DOES_NOT_EXIST;
	}
	reply.handle=(unsigned int)handle;
	if(reply.nt_status==STATUS_SUCCESS)PidsWatcher::HandleOpened(client_pid,reply.handle);  
	DWORD dw=0;
	MyWriteDebugLog("ProceedOpen point3 status %x",reply.nt_status);
	bool fSuccess = WriteFile(hPipe,&reply,sizeof(reply),&dw,NULL)!=FALSE;
	MyWriteDebugLog("ProceedOpen point4");
	return (fSuccess && (dw==sizeof(reply)));
}

bool ProceedClose(HANDLE hPipe, LPANYFS_CLOSE_REQUEST Request, DWORD cbBytesRead, DWORD client_pid)
{
	if(cbBytesRead<sizeof(ANYFS_CLOSE_REQUEST))
		return false;
	
	ANYFS_CLOSE_REPLY reply;
	reply.nt_status=FtpWorks::xxxCloseHandle((HANDLE)Request->handle); 
	if(reply.nt_status==STATUS_SUCCESS)PidsWatcher::HandleClosed(client_pid,Request->handle);  
	
	DWORD dw=0;
	bool fSuccess = WriteFile(hPipe,&reply,sizeof(reply),&dw,NULL)!=FALSE;
	return (fSuccess && (dw==sizeof(reply)));
}



bool ProceedRead(HANDLE hPipe, LPANYFS_READ_REQUEST Request, DWORD cbBytesRead, READBUFFER &read_buffer)
{
	if(cbBytesRead<sizeof(ANYFS_READ_REQUEST))
		return false;
	
	LARGE_INTEGER pos={Request->offset_low,Request->offset_hi}; 
	
	DWORD read_ok=0;
	
	if(Request->max_size>read_buffer.size())
	{
		read_buffer.resize(Request->max_size+0xffff);
	}else
	{
		if((read_buffer.size()>0x800000)&&(Request->max_size<0x100000))
		{
			read_buffer.resize(Request->max_size);
		}
	}
	NTSTATUS rv;
	for(;;)
	{
		rv=FtpWorks::xxxReadFile((HANDLE)Request->handle,&read_buffer[0],&pos,Request->max_size,&read_ok);
		if( (rv==STATUS_SUCCESS)||(rv==STATUS_END_OF_FILE))break;
		
		//FtpWorks::SuspendFile((HANDLE)Request->handle);
		
		ANYFS_INFO_REPLY info;
		FtpWorks::xxxGetFileInfo((HANDLE)Request->handle, info);
		if( !ErrorHandler::CheckRetry(OPCODE_READ, rv,Request->rawhdr.pid,
			std::wstring(info.file_name),Request->offset_low,Request->max_size))break;
		
		Sleep(10);   
	}
	
	MyWriteDebugLog("ProceedRead result %x read_ok %x",rv,read_ok);
	
	ANYFS_READ_REPLY_HEADER reply={rv,read_ok}; 
	DWORD dw=0;
	bool fSuccess = WriteFile(hPipe,&reply,sizeof(reply),&dw,NULL)!=FALSE; 
	if((!fSuccess) || (dw!=sizeof(reply)))return false;
	
	bool out=true;
	if(reply.len)
	{
		dw=0;
		fSuccess = WriteFile(hPipe,&read_buffer[0],reply.len,&dw,NULL)!=FALSE; 
		if((!fSuccess) || (dw!=reply.len))out=false;
	}
	
	return out;
}

bool ProceedWrite(HANDLE hPipe, LPANYFS_READ_REQUEST Request, DWORD cbBytesRead, READBUFFER &read_buffer)
{
	if(cbBytesRead<sizeof(ANYFS_READ_REQUEST))
		return false;
	
	LARGE_INTEGER pos={Request->offset_low,Request->offset_hi}; 
	
	DWORD read_ok=0;
	
	if(Request->max_size>read_buffer.size())
	{
		read_buffer.resize(Request->max_size+0xffff);
	}else
	{
		if((read_buffer.size()>0x800000)&&(Request->max_size<0x100000))
		{
			read_buffer.resize(Request->max_size);
		}
	}
	
	bool fSuccess=true;
	DWORD dw=0;
	if(Request->max_size)
	{
		dw=0;
		fSuccess = ReadFile(hPipe,&read_buffer[0],Request->max_size,&dw,NULL)!=FALSE; 
		if((!fSuccess) || (dw!=Request->max_size))return false;
	}
	
	NTSTATUS rv;
	for(;;)
	{
		rv=FtpWorks::xxxWriteFile((HANDLE)Request->handle,&read_buffer[0],&pos,Request->max_size,&read_ok);
		if( (rv==STATUS_SUCCESS)||(rv==STATUS_END_OF_FILE))break;
		
		//FtpWorks::SuspendFile((HANDLE)Request->handle);
		
		ANYFS_INFO_REPLY info;
		FtpWorks::xxxGetFileInfo((HANDLE)Request->handle, info);
		if( !ErrorHandler::CheckRetry(OPCODE_WRITE, rv,Request->rawhdr.pid,
			std::wstring(info.file_name),Request->offset_low,Request->max_size))break;
		
		Sleep(10);   
	}
	
	MyWriteDebugLog("ProceedWrite result %x read_ok %x",rv,read_ok);
	
	ANYFS_READ_REPLY_HEADER reply={rv,read_ok}; 
	fSuccess = WriteFile(hPipe,&reply,sizeof(reply),&dw,NULL)!=FALSE; 
	if((!fSuccess) || (dw!=sizeof(reply)))return false;
	
	reply.len=Request->max_size;
	
	return true;
}


bool ProceedList(HANDLE hPipe, LPANYFS_LIST_REQUEST Request, DWORD cbBytesRead)
{
	if(cbBytesRead<sizeof(ANYFS_LIST_REQUEST))
		return false;
	
	FtpWorks::FtpFindDataPtr wfd;
	ANYFS_INFO_REPLY reply;
	reply.nt_status=FtpWorks::xxxGetDirChild((HANDLE)Request->handle,Request->index,Request->rawhdr.pid,wfd);
	
	if(reply.nt_status==STATUS_SUCCESS)
	{  
		memcpy(reply.file_name,
			wfd->sFileName.c_str(),
			sizeof(TCHAR)*min(wfd->sFileName.size()+1,ANYFS_MAX_PATH));
		reply.file_name[ANYFS_MAX_PATH-1]=0;
		reply.ac_time.HighPart=wfd->ftLastAccessTime.dwHighDateTime;
		reply.ac_time.LowPart=wfd->ftLastAccessTime.dwLowDateTime;
		reply.cr_time.HighPart=wfd->ftCreationTime.dwHighDateTime;
		reply.cr_time.LowPart=wfd->ftCreationTime.dwLowDateTime;
		reply.wr_time.HighPart=wfd->ftLastWriteTime.dwHighDateTime;
		reply.wr_time.LowPart=wfd->ftLastWriteTime.dwLowDateTime;
		reply.attributes=wfd->dwFileAttributes;  
		reply.size.HighPart=wfd->nFileSizeHigh;
		reply.size.LowPart=wfd->nFileSizeLow;
		MyWriteDebugLog("ProceedList index %i name %ws",Request->index,reply.file_name);
	}else MyWriteDebugLog("ProceedList status %x",reply.nt_status);
	
	DWORD dw=0;
	bool fSuccess = WriteFile(hPipe,&reply,sizeof(reply),&dw,NULL)!=FALSE; 
	return (fSuccess && (dw==sizeof(reply)));
}

bool ProceedInfo(HANDLE hPipe, LPANYFS_INFO_REQUEST Request, DWORD cbBytesRead)
{
	if(cbBytesRead<sizeof(ANYFS_INFO_REQUEST))
		return false;
	ANYFS_INFO_REPLY reply;
	FtpWorks::xxxGetFileInfo((HANDLE)Request->handle, reply);
	DWORD dw=0;
	bool fSuccess = WriteFile(hPipe,&reply,sizeof(reply),&dw,NULL)!=FALSE; 
	MyWriteDebugLog("ProceedInfo name %ws",reply.file_name);
	return (fSuccess && (dw==sizeof(reply)));
}

bool ProceedTemp(HANDLE hPipe, LPANYFS_TEMP_REQUEST Request, DWORD cbBytesRead)
{
	if(cbBytesRead<sizeof(ANYFS_TEMP_REQUEST))
		return false;
	ANYFS_TEMP_REPLY reply;
	std::wstring tempfile; 
	FtpWorks::xxxPrepareTempFile((HANDLE)Request->handle, Request->max_size,reply); 
	DWORD dw=0;
	bool fSuccess = WriteFile(hPipe,&reply,sizeof(reply),&dw,NULL)!=FALSE; 
	MyWriteDebugLog("ProceedTemp status %x name %ws",reply.nt_status,reply.file_name);
	return (fSuccess && (dw==sizeof(reply)));
}

bool ProceedCheckApp(HANDLE hPipe, LPANYFS_CHECKAPP_REQUEST Request, DWORD cbBytesRead)
{
	if(cbBytesRead<sizeof(ANYFS_CHECKAPP_REQUEST))
		return false;
	ANYFS_CHECKAPP_REPLY reply;
	Request->filename[ANYFS_MAX_PATH-1]=0;
	reply.affected=FtpSettings::IsThisAppAffected(Request->filename)?TRUE:FALSE;
	DWORD dw=0;
	bool fSuccess = WriteFile(hPipe,&reply,sizeof(reply),&dw,NULL)!=FALSE;  
	return (fSuccess && (dw==sizeof(reply)));
}

bool ProceedRename(HANDLE hPipe, LPANYFS_RENAME_REQUEST Request, DWORD cbBytesRead)
{
	if(cbBytesRead<sizeof(ANYFS_RENAME_REQUEST))
		return false;
	ANYFS_RENAME_REPLY reply;
	Request->filename[ANYFS_MAX_PATH-1]=0;
	reply.nt_status=FtpWorks::xxxRenameFile((HANDLE)Request->handle,std::wstring(Request->filename));
	DWORD dw=0;
	bool fSuccess = WriteFile(hPipe,&reply,sizeof(reply),&dw,NULL)!=FALSE;  
	return (fSuccess && (dw==sizeof(reply)));
}

bool ProceedPathEnforce(HANDLE hPipe, LPANYFS_PATHENFORCE_REQUEST Request, DWORD cbBytesRead)
{
	if(cbBytesRead<sizeof(ANYFS_PATHENFORCE_REQUEST ))
		return false;
	ANYFS_PATHENFORCE_REPLY reply;
	Request->path[ANYFS_MAX_PATH-1]=0;
	reply.win32_error_code=FtpWorks::xxxPathEnforce(std::wstring(Request->path));
	DWORD dw=0;
	bool fSuccess = WriteFile(hPipe,&reply,sizeof(reply),&dw,NULL)!=FALSE;  
	return (fSuccess && (dw==sizeof(reply)));
}


bool ProceedRequest(HANDLE hPipe, LPANYFS_RAW_REQUEST_HEADER Request, DWORD cbBytesRead, DWORD client_pid, READBUFFER &read_buffer)
{
	MyWriteDebugLog("ProceedRequest cbBytesRead %u",cbBytesRead);
	if (cbBytesRead<sizeof(ANYFS_RAW_REQUEST_HEADER))
		return false;

	if (IsPidBlocked(Request->pid))
		return false;
	MyWriteDebugLog("ProceedRequest code %u",Request->code);
	switch(Request->code)
	{
	case ANYFS_CMD_OPEN:
		{
			RequestProcessIndicator request_process_indicator(ftp_traffic_counter);
			return ProceedOpen(hPipe, (LPANYFS_OPEN_REQUEST)(Request), cbBytesRead, client_pid);
		}
		
	case ANYFS_CMD_CLOSE:
		{
			RequestProcessIndicator request_process_indicator(ftp_traffic_counter);
			return ProceedClose(hPipe, (LPANYFS_CLOSE_REQUEST)(Request), cbBytesRead, client_pid);
		}
		
	case ANYFS_CMD_READ:
		{
			RequestProcessIndicator request_process_indicator(ftp_traffic_counter);
			return ProceedRead(hPipe, (LPANYFS_READ_REQUEST)(Request), cbBytesRead, read_buffer);
		}
		
	case ANYFS_CMD_WRITE:
		{
			RequestProcessIndicator request_process_indicator(ftp_traffic_counter);
			return ProceedWrite(hPipe, (LPANYFS_WRITE_REQUEST)(Request), cbBytesRead, read_buffer);
		}
		
	case ANYFS_CMD_LIST:
		{
			RequestProcessIndicator request_process_indicator(ftp_traffic_counter);
			return ProceedList(hPipe, (LPANYFS_LIST_REQUEST)(Request), cbBytesRead);
		}
		
	case ANYFS_CMD_INFO:
		{
			RequestProcessIndicator request_process_indicator(ftp_traffic_counter);
			return ProceedInfo(hPipe, (LPANYFS_INFO_REQUEST)(Request), cbBytesRead);
		}
		
	case ANYFS_CMD_TEMP:
		{
			RequestProcessIndicator request_process_indicator(ftp_traffic_counter);
			return ProceedTemp(hPipe, (LPANYFS_TEMP_REQUEST)(Request), cbBytesRead);
		}
		
	case ANYFS_CMD_CHECKAPP:
		{
			return ProceedCheckApp(hPipe, (LPANYFS_CHECKAPP_REQUEST)(Request), cbBytesRead);
		}
		
	case ANYFS_CMD_RENAME:
		{
			RequestProcessIndicator request_process_indicator(ftp_traffic_counter);
			return ProceedRename(hPipe, (LPANYFS_RENAME_REQUEST)(Request), cbBytesRead);
		}
		
	case ANYFS_CMD_PATHENFORCE:
		{
			RequestProcessIndicator request_process_indicator(ftp_traffic_counter);
			return ProceedPathEnforce(hPipe, (LPANYFS_PATHENFORCE_REQUEST)(Request), cbBytesRead);
		}
		
	default:
		return false;
	}
}