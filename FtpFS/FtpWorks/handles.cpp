#include "stdafx.h"
#include "winsock.h"
#include "../TrafficCounter.h"
#include "../../AnyFS/AnyFS.h"
#include "ftpworks.h"
#include "../../FtpSettings/defs.h"
#include "../../AnyFS/SomeUsefull.h"
#include "../../DebugLog/DebugLog.h"
#include "dirlist.h"
#include "../SrvReplyLog.h"
#include "../../FtpFS/ntdefs.h"
#include "../../APIHooks/ntapi.h"

namespace FtpWorks
{
	//typedef BOOL (WINAPI *extFtpCommandx)(HINTERNET hConnect,BOOL fExpectResponse,DWORD dwFlags,LPCTSTR lpszCommand,DWORD dwContext,HINTERNET* phFtpCommand);
	//extFtpCommandx extFtpCommand=(extFtpCommandx)GetProcAddress(LoadLibrary("wininet.dll"),"FtpCommandA");
	
	typedef struct CONNENTRY_{
		DWORD       ip;
		DWORD		port;
		FTPSMODE	smode;
		ENCCP		enc;
		std::wstring user;
		std::wstring pass;
		std::wstring curdir;
		DWORD expires;                
		bool busy;
	}CONNENTRY;
	
	
	typedef std::map<IFtpSocketPtr,CONNENTRY,LessIFtpSocketPtr>            CONNSMAP;
	typedef std::vector<IFtpSocketPtr>  CONNSVECTOR;
	typedef std::set<std::wstring>  WSTRINGSET;
	
	FastCriticalSection    conns_cs;
	FastCriticalSection    enum_cs;
	CONNSMAP            conns_map;
	WSTRINGSET			symlinks;
	
	HANDLE AllocateHandle()
	{
		//EnterCriticalSection(&hnds_cs);
		//if(++last_handle>0xffff)last_handle=1;
		HANDLE out=GlobalAlloc(GHND,0);
		//LeaveCriticalSection(&hnds_cs);
		return out;
	}
	
	void DeAllocateHandle(HANDLE h)
	{
		//EnterCriticalSection(&hnds_cs);
		GlobalFree(h);
		//LeaveCriticalSection(&hnds_cs);
	}


	std::string FormatIPPort(DWORD ip, unsigned int port)
	{
		in_addr ina;ina.S_un.S_addr = ip;
		char buf[300];
		sprintf(buf,"%s:%u",inet_ntoa(ina),port);
		return buf;
	}

	bool FtpPerformLogin(IFtpSocketPtr &sock, FtpFilePtr &file, CmdBuf &cmd)
	{
		if (!cmd.sendf(file, sock,"USER %ws\r\n",file->user.c_str()))
			return false;
		
		DWORD resp = RecvResponse(file,sock);
		MyWriteDebugLog("USER reply %i for %x",resp,file->ip);

		if(resp==230)
			return true;

		if (resp==0xffffffff)
			return false;

		if (!cmd.sendf(file, sock,"PASS %ws\r\n",file->pass.c_str()))
			return false;

		resp = RecvResponse(file,sock);    
		MyWriteDebugLog("PASS reply %i for %x",resp,file->ip);

		if(resp==530 || resp==0xffffffff)
			return false;

		return true;
	}

	bool FtpPerformExplicitSslAuth(IFtpSocketPtr &sock, FtpFilePtr &file, CmdBuf &cmd)
	{
		if (!cmd.sendf(file, sock,"AUTH %s\r\n",
			(file->smode==SModeExplicitSsl)?
			"SSL":"TLS"))return false;
		
		for (;;)
		{
			DWORD resp = RecvResponse(file,sock);
			if(resp==234)
			{
				sock.reset(CreateFtpSslSocket(sock));
				return true;
			}

			if (resp!=220)
			{
				MyWriteDebugLog("FtpConnectLogin request 'AUTH SSL' failed, responce=%i", resp);
				return false;
			}
		}
	}

	bool FtpPerformSslDataProt(IFtpSocketPtr &sock, FtpFilePtr &file, CmdBuf &cmd)
	{
		if (!CmdBuf::send(file, sock,"PBSZ 0\r\n",8))
			return false;
		
		DWORD resp = RecvResponse(file,sock);
		
		if (resp!=200)
		{
			MyWriteDebugLog("FtpConnectLogin request 'PBSZ 0' failed, responce=%i", resp);
			return false;
		}
		
		if (!CmdBuf::send(file, sock,"PROT P\r\n",8))
			return false;
		
		resp = RecvResponse(file,sock);
		
		if (resp!=200)
		{
			MyWriteDebugLog("FtpConnectLogin request 'PROT P' failed, responce=%u", resp);
			return false;
		}
		
		return true;
	}
	
	bool FtpActivateUTF8(IFtpSocketPtr &sock, FtpFilePtr &file, CmdBuf &cmd)
	{
		if (!CmdBuf::send(file, sock,"CLNT FtpDrive\r\n"))
			return false;

		DWORD resp = RecvResponse(file,sock);
		if (resp==0xffffffff)
		{
			MyWriteDebugLog("FtpConnectLogin RecvResponse failed (%i), err=%u", resp, WSAGetLastError());
			return false;
		}
		
		if (!CmdBuf::send(file, sock,"OPTS UTF8 ON\r\n"))
			return false;

		resp = RecvResponse(file,sock);
		if(resp==0xffffffff)
		{
			MyWriteDebugLog("FtpConnectLogin RecvResponse failed (%i), err=%u", resp, WSAGetLastError());
			return false;
		}

		return true;
	}

	IFtpSocketPtr FtpConnectLogin(FtpFilePtr &file)
	{
		IFtpTcpSocketPtr tcp_sock(
			CreateFtpTcpSocket(
			true, true, false, FtpSettings::ftp_replies_timeout));
		
		MyWriteDebugLog("FtpConnectLogin connecting to %x",file->ip);   
		AutoNetControl anc(net_control,tcp_sock);
		
		if (!tcp_sock->connect_to(file->ip,(unsigned short)file->port))
		{
			SrvReplyLog::AddError(file->serv,tcp_sock->error(),
				"command", "connect", FormatIPPort(file->ip, file->port).c_str());
			MyWriteDebugLog("FtpConnectLogin connect failed, err=%u", WSAGetLastError());
			return IFtpSocketPtr();
		}
		SrvReplyLog::AddReply(file->serv,
			(std::string("Command connection estabilished to ")
			+FormatIPPort(file->ip, file->port)).c_str());
		
		IFtpSocketPtr sock;
		
		if (file->smode==SModeImplicit)
		{
			sock.reset(CreateFtpSslSocket(tcp_sock));
			if (!sock)
				return IFtpSocketPtr();
		}
		else
		{
			sock = tcp_sock;
		}
		
		CmdBuf cmd;

		MyWriteDebugLog("EnsureCommandConnection wait logo for %x",file->ip);
		DWORD resp=RecvResponse(file,sock);
		MyWriteDebugLog("EnsureCommandConnection logo=%u sending user for %x",resp,file->ip);
		if (resp==421 || resp==0xffffffff)
			return IFtpSocketPtr();

		
		if (file->smode==SModeExplicitSsl 
		|| file->smode==SModeExplicitTls)
		{
			if (!FtpPerformExplicitSslAuth(sock, file, cmd))
				return IFtpSocketPtr();
		}

		if (!FtpPerformLogin(sock,file,cmd))
			return IFtpSocketPtr();
		
		if (file->smode==SModeExplicitSsl 
			|| file->smode==SModeExplicitTls
			|| file->smode==SModeImplicit)
		{
			if (!FtpPerformSslDataProt(sock,file,cmd))
				return IFtpSocketPtr();
		}
		
		if (file->enc==EncUTF8)
		{
			if (!FtpActivateUTF8(sock, file, cmd))
				return IFtpSocketPtr();
		}
		
		return sock;
}

bool CheckByNoop(FtpFilePtr &file, IFtpSocketPtr sock)
{
	if (!sock->flush())
	{
		SrvReplyLog::AddError(file->serv,sock->error(),
			"command", "flush", "NOOP check");
		return false;
	}
	
	if (!CmdBuf::send(file,sock,"NOOP\r\n",6))
		return false;
	
	bool out = false;
	//need perform custom responce parsing
	//since we need to react on 200 responce 
	//anywhere it returned (even if something 
	//after it received
	std::string str;
	char buf[0x1000];
	//int old_tmout = sock->timeout();
	for(int i=0;;i++)
	{
	//	if (i==1 && old_tmout>1000)
	//		sock->set_timeout(1000);
		
		int len = sock->recv(buf,sizeof(buf)-1);
		if (len<=0)
		{
			SrvReplyLog::AddError(file->serv,sock->error(),
				"command", "recv", "NOOP check");
			return false;
		}

		str.append(buf,len);
		for(;;)
		{
			size_t x=str.find('\n');
			if(x==std::string::npos)break;
			
			std::string line = str.substr(0,x);
			str.erase(0,x+1);
			SrvReplyLog::AddReply(file->serv,line);

			x = line.find(' ');
			if(x==std::string::npos)break;

			size_t y=line.find('-');
			if(y!=std::string::npos && y<x)break;

			line.resize(x);

			int r=atoi(line.c_str());
			if(r==200||r==220||r==500)
			{
				out = true;
			}
		}
		if (out && str.empty())
		{
			return true;
		}
	}
}

void LookupMatchedConnectionsInCache(FtpFilePtr file, 
									 CONNSVECTOR &foundvector,
									 CONNSVECTOR &delvector,
									 size_t &connscount)
{
	AutoCriticalSection cslock(conns_cs);
	DWORD cur_tm = ::GetTickCount();
	for(CONNSMAP::iterator i=conns_map.begin();i!=conns_map.end();++i)
	{
		if((file->ip==i->second.ip)
			&&(file->port==i->second.port)
			&&(file->user==i->second.user)
			&&(file->pass==i->second.pass)
			&&(file->smode==i->second.smode)
			&&(file->enc==i->second.enc))
		{  
			if (!i->second.busy)
			{
				i->second.busy=true;
				foundvector.push_back(i->first);
			}else 
			{
				connscount++;
			}
		}
		else if((!i->second.busy)&&(cur_tm>i->second.expires))
		{  
			//MessageBoxA(0,"expired","zz",0);
			delvector.push_back(i->first);
		}
	}
}

void DeleteConnectionsFromCache(CONNSVECTOR &delvector)
{
	AutoCriticalSection cslock(conns_cs);
	for(CONNSVECTOR::iterator j=delvector.begin();j!=delvector.end();++j)
	{
		CONNSMAP::iterator i=conns_map.find(*j);
		if(i!=conns_map.end())
		{
			MyWriteDebugLog("OpenFtpConnection closing(%u)",j->get());
			conns_map.erase(i);
		}
	}
}


IFtpSocketPtr OpenFtpConnectionCached(FtpFilePtr file, size_t &connscount)
{
	IFtpSocketPtr out;
	
	for(int i=0;i<20;i++)
	{
		connscount = 0;
		CONNSVECTOR delvector,foundvector;
		LookupMatchedConnectionsInCache(file, foundvector, delvector, connscount);
		
		for(CONNSVECTOR::iterator j=foundvector.begin();j!=foundvector.end();++j)
		{
			if (out)
			{
				AutoCriticalSection cslock(conns_cs);
				CONNSMAP::iterator i=conns_map.find(*j);
				if (i!=conns_map.end())i->second.busy=false;
			}else
			{
				if (CheckByNoop(file,*j))
				{    
					AutoCriticalSection cslock(conns_cs);
					CONNSMAP::iterator i=conns_map.find(*j);
					if(i!=conns_map.end())
					{
						i->second.busy=true;
						i->second.expires=GetTickCount()+(1000*FtpSettings::idle_timeout);
						file->curdir=i->second.curdir;
						out = *j;
						MyWriteDebugLog("OpenFtpConnection from cache for '%x'",file->ip);
					}else 
					{
						MyWriteDebugLog("OpenFtpConnection from cache for '%x' failed: map cache changed???!!!",file->ip);
					}         
				}else 
				{
					MyWriteDebugLog("OpenFtpConnection from cache for '%x' failed: WSAGetLastError()=%u",file->ip,WSAGetLastError());    
					delvector.push_back(*j);
				}
			}
		}

		DeleteConnectionsFromCache(delvector);
		if((out)||(!connscount))break;
		Sleep(100);
	}
	return out;
}

									  
IFtpSocketPtr OpenFtpConnectionNew(FtpFilePtr &file)
{
	IFtpSocketPtr out(FtpConnectLogin(file));
	//todo: FtpConnect(ip.c_str(),user.c_str(),pass.c_str());
	//InternetConnect(hinet,ip.c_str(),INTERNET_DEFAULT_FTP_PORT,user.c_str(),pass.c_str(),INTERNET_SERVICE_FTP,passive_mode?INTERNET_FLAG_PASSIVE:0,0);
	if(out)
	{
		MyWriteDebugLog("OpenFtpConnection for '%x'",file->ip);  
		file->curdir.resize(0);
		CONNENTRY ce;
		ce.ip=file->ip;
		ce.port=file->port;
		ce.smode=file->smode;
		ce.enc=file->enc;
		ce.user=file->user;
		ce.pass=file->pass;
		//ce.hicon=out;
		ce.expires=GetTickCount()+(1000*FtpSettings::idle_timeout);
		ce.busy=true;
		AutoCriticalSection cslock(conns_cs);
		conns_map.insert(CONNSMAP::value_type(out,ce));
	}else 
	{
		MyWriteDebugLog("FtpConnectLogin failed: WSAGetLastError()=%u, ip=%x",
			WSAGetLastError(),file->ip);
	}
	
	return out;
}

IFtpSocketPtr OpenFtpConnection(FtpFilePtr file)
{
	for(;;)
	{
		size_t connscount=0;
		IFtpSocketPtr out = OpenFtpConnectionCached(file, connscount);
		
		if (out)
			return out;
		
		if(file->conlim==0 || connscount<file->conlim)
		{
			return OpenFtpConnectionNew(file);
		}
		
		ForceSuspendNotActiveFilesForIP(file->ip, file->port);
		Sleep(10);
		//MarkDataConnectionsForExpire()
	}
}

void CloseCleanFtpConnectionsCache(FtpFilePtr file, bool forced)
{
	AutoCriticalSection cslock(conns_cs);
	CONNSVECTOR delvector;
	bool closed=0;
	if (file)
	{
		CONNSMAP::iterator i=conns_map.find(file->sc_cmd);
		if(i!=conns_map.end())
		{
			MyWriteDebugLog("CloseCleanFtpConnectionsCache marked as free sc_cmd=%u",
				file->sc_cmd.get());
			i->second.expires=GetTickCount()+(1000*FtpSettings::idle_timeout);
			i->second.busy=false;
			i->second.curdir=file->curdir;
		}
		else
			MyWriteDebugLog("CloseFtpConnection failed");
		
		file->sc_cmd.reset();
	}
	for(CONNSMAP::iterator i=conns_map.begin();i!=conns_map.end();++i)
	{
		if((!i->second.busy)&&(forced||(GetTickCount()>i->second.expires)))delvector.push_back(i->first);
	}
	
	if(!delvector.empty())
	{ 
		for(CONNSVECTOR::iterator j=delvector.begin();j!=delvector.end();++j)
		{  
			CONNSMAP::iterator i=conns_map.find(*j);
			if(i!=conns_map.end())
			{
				conns_map.erase(i);
				MyWriteDebugLog("CloseCleanFtpConnectionsCache closesocket(%u)",j->get());
			}
		}
	} 
}

typedef struct FTPCACHEENTRY_
{
	DWORD expire;
	FindVector list;
}FTPCACHEENTRY;

typedef std::map<std::wstring,FTPCACHEENTRY,wstring_less_no_case> FTPDIRCACHEMAP;

FTPDIRCACHEMAP ftp_dir_cache;

FILETIME root_time={0};

void AddFileToDirCache(FtpFilePtr file, const FtpFindData &wfd)
{
	std::wstring servpathname=file->MakeFilePathName(false);
	std::wstring fname=ExtractFileName(servpathname);
	servpathname=ExtractFilePath(servpathname);
	if(servpathname.empty()||fname.empty())return;
	
	AutoCriticalSection cslock(enum_cs);
	FTPDIRCACHEMAP::iterator i=ftp_dir_cache.find(servpathname);
	if(i!=ftp_dir_cache.end())
	{
		bool con=true;
		FTPCACHEENTRY &fce=i->second;
		for(FindVector::iterator f=fce.list.begin();f!=fce.list.end();++f)
		{
			if (!wcsicmp((*f)->sFileName.c_str(),fname.c_str()))
			{
				con=false;
				break;
			}
		}
		if(con)
		{
			fce.list.push_back(
				FtpFindDataPtr(
				new FtpFindData(wfd)));
		}
	}
}

void CleanFileFromDirCache(FtpFilePtr file)
{
	std::wstring servpathname=file->MakeFilePathName(false);
	std::wstring fname=ExtractFileName(servpathname);
	servpathname=ExtractFilePath(servpathname);
	if(servpathname.empty()||fname.empty())return;
	
	AutoCriticalSection cslock(enum_cs);
	FTPDIRCACHEMAP::iterator i=ftp_dir_cache.find(servpathname);
	if(i!=ftp_dir_cache.end())
	{
		FTPCACHEENTRY &fce=i->second;
		for(FindVector::iterator f=fce.list.begin();f!=fce.list.end();++f)
		{
			if (!wcsicmp((*f)->sFileName.c_str(),fname.c_str()))
			{
				DWORD tm=GetTickCount();
				if(fce.expire>tm)fce.expire=tm+3000;
				fce.list.erase(f);
				break;
			}
		}
	}
}

void CleanDirCache(const std::wstring &dir)
{
	AutoCriticalSection cslock(enum_cs);
	FTPDIRCACHEMAP::iterator i=ftp_dir_cache.find(dir);
	if(i!=ftp_dir_cache.end())ftp_dir_cache.erase(i);
}

void CleanEnumCache(bool forced)
{
	typedef std::set<std::wstring,wstring_less_no_case>              ICSTRINGSET;
	ICSTRINGSET delset;
	DWORD tm=GetTickCount(); 
	AutoCriticalSection cslock(enum_cs);
	for(FTPDIRCACHEMAP::iterator i=ftp_dir_cache.begin();i!=ftp_dir_cache.end();i++) 
	{
		if(forced||(tm>i->second.expire))delset.insert(i->first);
	}
	for(ICSTRINGSET::iterator j=delset.begin();j!=delset.end();++j)
	{
		FTPDIRCACHEMAP::iterator i=ftp_dir_cache.find(*j);
		if(i!=ftp_dir_cache.end())ftp_dir_cache.erase(i);
	}
}

void ApplyBackSlash(std::wstring &s, bool with_backslash)
{
	if (with_backslash)
	{
		if(s.empty()||(s[s.size()-1]!='/'))
			s.append(1,'/');
	}
	else
	{
		if ((!s.empty())&&(s[s.size()-1]=='/'))
			s.resize(s.size()-1);
	}
}

std::wstring FtpFile::MakeFilePathName(bool with_backslash)
{
	std::wstring out(serv);
	if(!path.empty() && path[0]!='/')
		out.append(1,'/');
	out.append(path);
	ApplyBackSlash(out, with_backslash);
	return out;
}

std::wstring FtpFile::MakeFileCurDir(bool with_backslash)
{
	std::wstring out(serv);
	if(!curdir.empty() && curdir[0]!='/')
		out.append(1,'/');
	out.append(curdir);
	ApplyBackSlash(out, with_backslash);
	return out;
}

void FtpEnumRefreshFile(HANDLE hFile, FtpFilePtr file)
{
	std::wstring servpathname=file->MakeFilePathName(true);
	
	while ((!servpathname.empty()) 
		&& (servpathname[servpathname.size()-1]=='/'
		|| servpathname[servpathname.size()-1]=='\\'))
	{
		servpathname.resize(servpathname.size()-1);
	}
	if (servpathname.empty())return;


	std::wstring fname(ExtractFileName(servpathname));
	std::wstring fpath(ExtractFilePath(servpathname));
	
	AutoCriticalSection cslock(enum_cs);
	FTPDIRCACHEMAP::iterator i=ftp_dir_cache.find(fpath);
	if(i!=ftp_dir_cache.end())
	{
		bool flush_dir = true;
		if (file->changed)
		{
			for (FindVector::iterator j=i->second.list.begin();
				j!=i->second.list.end();++j)
			{
				if ((*j)->sFileName==fname)
				{
					::GetSystemTimeAsFileTime(&(*j)->ftLastWriteTime);
					(*j)->nFileSizeHigh = (DWORD)((file->size>>32)&0xffffffff);
					(*j)->nFileSizeLow = (DWORD)(file->size&0xffffffff);
					flush_dir = false;
				}
			}
		}

		if (flush_dir)
			ftp_dir_cache.erase(i);
	}
}

bool FtpEnum(HANDLE hFile, FtpFilePtr file, FindVector &out)
{
	if(file->serv.empty())
	{
		if(!root_time.dwLowDateTime)
		{
			SYSTEMTIME st;GetSystemTime(&st);
			SystemTimeToFileTime(&st,&root_time);
		}
		
		FtpFindData wfd;  
		wfd.ftLastWriteTime=root_time;
		wfd.ftCreationTime=root_time;
		wfd.ftLastAccessTime=root_time;
		wfd.ftLastWriteTime=root_time; 
		wfd.nFileSizeLow=wfd.nFileSizeHigh=0;
		wfd.dwFileAttributes=FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_READONLY;
		STRINGVECTOR hlst;
		//MyWriteDebugLog("FtpEnum p1"); 
		
		FtpSettings::CurrentSitesList()->GetHostList(hlst);
		//MyWriteDebugLog("FtpEnum p2"); 
		for(STRINGVECTOR::iterator i=hlst.begin();i!=hlst.end();++i)
		{
			wfd.sFileName = *i;
			out.push_back(
				FtpFindDataPtr(
				new FtpFindData(wfd)));
		}
		//MyWriteDebugLog("FtpEnum p4"); 
		return true;
	}
	std::wstring servpathname=file->MakeFilePathName(true);
	if (FtpSettings::adv_flags&FTPS_ADV_DIRCACHE)
	{
		AutoCriticalSection cslock(enum_cs);
		FTPDIRCACHEMAP::iterator i=ftp_dir_cache.find(servpathname);
		if (i!=ftp_dir_cache.end())
		{
			out=i->second.list;
			return true;
		}
		if(ftp_dir_cache.size()>0x4000)ftp_dir_cache.clear();
	}
	if (!EnsureCommandConnection(hFile,file))
	{
		MyWriteDebugLog("FtpEnum EnsureCommandConnection failed"); 
		return false;
	}
	
	std::vector<char> buf;
	if (!rawFtpEnum(hFile,file,buf))
	{  
		MyWriteDebugLog("FtpEnum rawFtpEnum failed"); 
		return false;
	}
	
	DWORD presult = ERROR_SUCCESS;
	if (buf.size())
	{
		presult = ParseDirList(&buf[0],buf.size(),out,file->conv);
		
		for (FindVector::iterator i=out.begin();i!=out.end();++i)
		{
			if ((*i)->dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)
			{
				//(*i)->dwFileAttributes&=~FILE_ATTRIBUTE_REPARSE_POINT;
				
				LARGE_INTEGER liSize;
				liSize.QuadPart = rawGetSize(hFile,file,(*i)->sFileName);
				if (liSize.QuadPart!=-1)
				{
					(*i)->nFileSizeHigh = (DWORD)liSize.HighPart;
					(*i)->nFileSizeLow = (DWORD)liSize.LowPart;
					//(*i)->dwFileAttributes&=~FILE_ATTRIBUTE_REPARSE_POINT;
					//MessageBox(0,(*i)->sFileName.c_str(),L"zz",0);
				}
				else
				{
					(*i)->dwFileAttributes|=FILE_ATTRIBUTE_DIRECTORY;
					std::wstring lo_path(file->MakeFilePathName(true)+(*i)->sFileName);
					CharLower(lo_path.begin());
					AutoCriticalSection cslock(enum_cs);
					if (symlinks.find(lo_path)==symlinks.end())
						symlinks.insert(lo_path);
				}
			}
			
		}	
	}
	
	CloseCleanFtpConnectionsCache(file);
	
	if(presult!=ERROR_SUCCESS)
	{
		MyWriteDebugLog("FtpEnum ParseDirList error = %u",presult); 
		SetLastError(presult);
		return false;
	}
	
	if(FtpSettings::adv_flags&FTPS_ADV_DIRCACHE)
	{
		FTPCACHEENTRY fce;
		fce.expire=GetTickCount()+(1000*FtpSettings::cache_dir_expire);
		fce.list=out;
		AutoCriticalSection cslock(enum_cs);
		FTPDIRCACHEMAP::iterator i=ftp_dir_cache.find(servpathname);
		if(i!=ftp_dir_cache.end())
			i->second=fce;
		else
			ftp_dir_cache.insert(FTPDIRCACHEMAP::value_type(servpathname,fce));
	}
	
	
	return true;
}

bool IsSymbolicLinkDir(const std::wstring &path)
{
	std::wstring lo_path(path);
	CharLower(lo_path.begin());
	AutoCriticalSection cslock(enum_cs);
	bool out=(symlinks.find(lo_path)!=symlinks.end());
	return out;
}

bool FtpEnum(const std::wstring &strFileName, FindVector &out)
{
	std::wstring strParentName(strFileName);
	size_t x=strParentName.rfind('\\');
	if((x+1)==strParentName.size())
	{
		strParentName.resize(strParentName.size()-1);
		x=strParentName.rfind('\\');
	}
	if(x!=std::wstring::npos)
	{
		strParentName.resize(x+1);
	}else strParentName.append(TEXT("\\"));
	
	bool ret=false;
	HANDLE hFile=NULL; 
	if(xxxOpenFile(strParentName,hFile,FILE_READ_ACCESS,FILE_OPEN,0,true)==STATUS_SUCCESS)
	{
		FtpFilePtr file;
		FileLocker locker(hFile,file);
		if(locker.LockSucceed)ret = FtpEnum(hFile,file,out);
	}
	if(hFile)xxxCloseHandle(hFile);
	return ret;
}

}