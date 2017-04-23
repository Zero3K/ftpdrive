
#include "stdafx.h"
#include "winsock.h"
#include "process.h"
#include "../TrafficCounter.h"
#include "../../FtpSettings/settings.h"
#include "../../FtpSettings/defs.h"
#include "../../FtpFS/ntdefs.h"
#include "../../APIHooks/ntapi.h"
#include "../../AnyFS/AnyFS.h"
#include "../../AnyFS/SomeUsefull.h"
#include "ftpworks.h"
#include "../../DebugLog/DebugLog.h"
#include "../UI/Dialogs.h"
#include "../ErrorHandler.h"
#include "../SrvReplyLog.h"

//todo: directory attribute control in xxxopenfile

#define LIMIT_MAXFORWARDSIZE    0x500000
#define MINIMUM_MAXFORWARDSIZE  0x4000
#define BYPASS_BUFFER_SIZE      0x1fffe//128k

namespace FtpWorks
{
	bool CmdBuf::send(FtpFilePtr &file, IFtpSocketPtr &sock, const char *str, int str_len)
	{
		if (str_len==-1)
			str_len = strlen(str);

		int r = sock->send(str,str_len);
		if (r==str_len)
			return true;

		std::string tmp_str(str,str_len);
		SrvReplyLog::AddError(file->serv,sock->error(),
			"command", "send", tmp_str.c_str());
		MyWriteDebugLog("CmdBuf send('%s', %i) result %i error %u", 
			tmp_str.c_str(), str_len, r, sock->error());

		return false;
	}

	bool __cdecl CmdBuf::sendf(FtpFilePtr &file, IFtpSocketPtr &sock, const char *format, ...)
	{
		va_list va;
		va_start(va, format);
		int r = _vsnprintf(_buf,511,format,va);
		va_end(va);
		if (r<0)
		{
			SrvReplyLog::AddError(file->serv,0,
				"command", "sendf", format);
			MyWriteDebugLog("CmdBuf format too long: '%s'", format);
			return false;
		}
		
		return send(file, sock, _buf, r);
	}

	bool __cdecl CmdBuf::sendf(FtpFilePtr &file, const char *format, ...)
	{
		va_list va;
		va_start(va, format);
		int r = _vsnprintf(_buf,511,format,va);
		va_end(va);
		if (r<0)
		{
			SrvReplyLog::AddError(file->serv,0,
				"command", "sendf", format);
			MyWriteDebugLog("CmdBuf format too long: '%s'", format);
			return false;
		}

		
		return send(file, _buf, r);
	}
	
	typedef struct FTPTEMP_
	{
		std::wstring temp_file;
		DWORD       expire;  
	}FTPTEMP;
	
	typedef std::map<std::wstring,DWORD,wstring_less_no_case>        STRINGDWORDMAP; 
	typedef std::map<std::wstring,FTPTEMP,wstring_less_no_case>      FILE2TEMPMAP; 
	
	STRINGDWORDMAP          inaccessible_files;
	static FILE2TEMPMAP     map_temp_files;
	
	
	FastCriticalSection temp_cs;
	FastCriticalSection iaf_cs;
	ICSTRINGSET      temp_busy;
	
	
	std::wstring tempdir;
	
	void ApplyDataSocketSSL(FtpFilePtr &file)
	{
		if(file->smode==SModeImplicit
			|| file->smode==SModeExplicitSsl
			|| file->smode==SModeExplicitTls)
		{
			file->sc_data.reset(CreateFtpSslSocket(file->sc_data));
			//file->sc_data->send("foo");
		}
	}
	
	DWORD RecvResponse(FtpFilePtr file,IFtpSocketPtr sc, std::string &txt) 
	{   
		MyWriteDebugLog("RecvResponse(%u)",sc);
		std::string str;
		char buf[1024];
		for(;;)
		{
			buf[1023]=0;
			DWORD tmout = FtpSettings::ftp_replies_timeout;
			if(!str.empty())
			{
				tmout/=5;
				if(tmout==0)tmout=1;
			}
			int len=sc->recv(buf,1024);
			if(len<=0)
			{
				SrvReplyLog::AddError(
					file->serv,sc->error(),
					"command","recv","responce");

				MyWriteDebugLog("RecvResponse(%u) recv error %u",
					sc.get(),GetLastError());  

				return 0xffffffff;
			}
			str.append(buf,len);
			while (!str.empty())
			{
				size_t x=str.rfind('\n');
				if(x==(str.size()-1))
				{
					int y=str.rfind('\n',x-1);
					if(y!=std::wstring::npos)str.erase(0,y+1);
					size_t a=str.find(' ');
					size_t b=str.find('-');
					if((a!=std::wstring::npos)&&((b==std::wstring::npos)||(b>a)))
					{    
						txt = str.substr(a,str.size()-a);
						DWORD resp=(DWORD)atoi(str.c_str());
						MyWriteDebugLog(
							"RecvResponse(%u) responce='%s'",
							sc.get(), str.c_str());
						SrvReplyLog::AddReply(file->serv,str);
						return resp;
					}else str.resize(0); 
				}else break;
			}
			
			if (str.size()>0x10000)
				str.erase(0x8000,0x7000);
		}
	} 
	
	DWORD RecvResponse(FtpFilePtr file, IFtpSocketPtr sc) 
	{   
		std::string txt;
		return RecvResponse(file, sc, txt);
	} 

	DWORD RecvResponseSkipAborReply(FtpFilePtr file,IFtpSocketPtr sc, std::string &txt) 
	{
		for(;;)
		{
			DWORD resp = RecvResponse(file,sc, txt);
			if (resp!=226 || txt.find(" ABOR")==std::string::npos)
				return resp;
		}
	}

	DWORD RecvResponseSkipAborReply(FtpFilePtr file,IFtpSocketPtr sc) 
	{
		std::string txt;
		return RecvResponseSkipAborReply(file,sc,txt);
	}
	
	DWORD RecvPasv(FtpFilePtr file) 
	{ 
		MyWriteDebugLog("RecvPasv entered");
		char buf[1025];memset(buf,0,1025);
		for(int i=0;((i<1024)&&(!memchr(buf,'\n',1024)));)
		{
			int len=file->sc_cmd->recv(buf+i,1024-i);//,FtpSettings::ftp_replies_timeout);
			if(len<=0)
			{
				SrvReplyLog::AddError(file->serv, file->sc_cmd->error(), "command", "recv", "pasv reply");

				return 0xffffffff;
			}
			i+=len;
		}
		SrvReplyLog::AddReply(file->serv,buf);

		MyWriteDebugLog("RecvPasv responce '%s'",buf);
		DWORD resp=0xfffffffe;
		char *x=(char *)memchr(buf,' ', 1024);
		if(x) 
		{
			bool has_text=(*x!=0);
			*x=0;
			resp=(DWORD)atoi(buf);
			
			if(has_text)
			{
				x++;
				int l=strlen(x);
				if(l)
				{
					char *y=x+l-1;
					while((x!=y) && !isdigit(*y))y--;
					char *t=y;
					y++;
					while((t!=x)&&((*t==',')||isdigit(*t)))t--;
					if(t!=x)x=t+1;else x=t;
					if(y>x)
					{
						////////////
						int arr[6]={0,0,0,0,0,0};
						int i=0;
						for(char *t=x,*l=x;t<=y;t++)
						{
							if( t==y || !isdigit(*t))
							{
								if((i<6)&&(t!=l))
								{
									*t=0;
									arr[i]=(atoi(l)&0xff);
									i++;
								}
								l=t+1;
							}
						}
						
						if(i==6)
						{
							char ipstr[256];sprintf(ipstr,"%i.%i.%i.%i",arr[0],arr[1],arr[2],arr[3]);
							int port=(arr[4]*256)+arr[5];
							IFtpTcpSocketPtr sc_ptr(
								CreateFtpTcpSocket(
								false, true, true, 
								FtpSettings::ftp_data_timeout));
							
							AutoNetControl anc(net_control,sc_ptr);
							DWORD ip=inet_addr(ipstr);
							if (sc_ptr->connect_to(ip,port))
							{
								file->sc_data = sc_ptr;
								return 0;
							}
							else
							{
								SrvReplyLog::AddError(file->serv,sc_ptr->error(), 
									"data", "connect", FormatIPPort(ip, port).c_str());
							}
						}
						////////////
					}
				}
			}   
		}
		
		MyWriteDebugLog("Failed to connect to PASV peer, error=%u",WSAGetLastError());
		return resp;
	} 
	
	////////////////////////////////////////////////////////
	std::string EncodePath(CPConv *conv, const std::wstring &path)
	{
		STRINGVECTOR v;
		
		bool end_by_slash=((path.size()>1)&&(path[path.size()-1]=='/'));
		if (end_by_slash)
		{
			SplitPath(path, v);
		}
		else
		{
			SplitPath(path+TEXT("/"), v);
		}
		
		std::string out;
		if(path[0]==TCHAR('/'))out.assign("/");
		for(size_t i=0;i<v.size();i++)
		{
			if(!out.empty())out.append(1,'/');
			out.append(conv->Encode(v[i]));
		}
		if(end_by_slash)out.append(1,'/');
		return out;
	}
	
	void CleanTempFiles()
	{
		DWORD tm=::GetTickCount();
		AutoCriticalSection cs_lock(temp_cs);
		ICSTRINGSET delset;
		for(FILE2TEMPMAP::iterator i=map_temp_files.begin();i!=map_temp_files.end();++i)
		{    
			if(i->second.expire<=tm)delset.insert(i->first);
		}
		for(ICSTRINGSET::iterator j=delset.begin();j!=delset.end();++j)
		{
			FILE2TEMPMAP::iterator i=map_temp_files.find(*j);
			if(i!=map_temp_files.end())
			{
				bool canerase=DeleteFile(i->second.temp_file.c_str())!=FALSE;
				if(!canerase)
				{
					int err=GetLastError();
					canerase=((err==ERROR_FILE_NOT_FOUND)||(err==ERROR_PATH_NOT_FOUND));
				}
				if(canerase)
					map_temp_files.erase(i);
				else
					i->second.expire=tm+(500*FtpSettings::cache_file_expire);
			}
		}
	}

	void __cdecl back_clean_thread(void *)
	{
		
		for(unsigned int i=0;;i++)
		{
			Sleep(FtpSettings::clean_interval);
			SuspendNotActiveFiles();
			CleanTempFiles();   
			CloseCleanFtpConnectionsCache();
			CleanEnumCache();
			CleanDataCache();
			if (!(i%12))
			{
				SrvReplyLog::RemoveOld();
			}
		}
	}
	
	void DelDirTree(const std::wstring &path)
	{
		WIN32_FIND_DATA wfd;
		std::wstring search(path);
		search.append(TEXT("\\*"));
		HANDLE fh=FindFirstFile(search.c_str(),&wfd);
		if(fh!=INVALID_HANDLE_VALUE)
		{
			do {
				std::wstring sub(path);
				sub.append(TEXT("\\"));
				sub.append(wfd.cFileName);     
				if(wfd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)
				{
					if(wfd.cFileName[0]&&wcscmp(wfd.cFileName,TEXT("."))&&wcscmp(wfd.cFileName,TEXT("..")))DelDirTree(sub);
				}else 
				{
					SetFileAttributes(sub.c_str(),FILE_ATTRIBUTE_NORMAL);
					DeleteFile(sub.c_str());
				}
			} while(FindNextFile(fh,&wfd));
			FindClose(fh);
		}
		if(GetFileAttributes(path.c_str())!=0xffffffff)
		{
			SetFileAttributes(path.c_str(),FILE_ATTRIBUTE_NORMAL);
			if(!RemoveDirectory(path.c_str()))DeleteFile(path.c_str());
		}
	}
	
	void EnsureDirExist(const std::wstring &path)
	{
		std::wstring xpath(path);
		xpath.append(TEXT("\\"));
		for(size_t i=0;i<xpath.size();i++)
			if(xpath[i]=='\\')
			{
				std::wstring part=xpath.substr(0,i);
				CreateDirectory(part.c_str(),NULL);
			}
	}
	void InitFtpWorks()
	{
		WSADATA wd;WSAStartup(257,&wd);
		InitOpenSSL();
		
		TCHAR temp[MAX_PATH+32]={0},longtemp[MAX_PATH+32]={0};
		GetEnvironmentVariable(TEXT("TEMP"),temp,MAX_PATH);
		if(!temp[0])GetEnvironmentVariable(TEXT("TMP"),temp,MAX_PATH);
		if(!temp[0])wcscpy(temp,TEXT("C:"));
		if(!GetLongPathName(temp,longtemp,MAX_PATH))
			wcscpy(longtemp,temp);
		int len=wcslen(longtemp);
		wcscpy(longtemp+len,TEXT("\\FtpDrive.tmp"));
		tempdir.assign(longtemp);
		DelDirTree(tempdir);
		EnsureDirExist(tempdir);
		_beginthread(back_clean_thread,0,NULL);
	}
	
	
	///
	bool SendCmd(FtpFilePtr file,const char *cmd,const char *par=NULL)
	{
		std::string str(cmd);
		if(par)
		{
			str.append(1,' ');
			str.append(par); 
		}
		str.append("\r\n"); 
		
		MyWriteDebugLog("Sending '%s'",str.c_str());
		
		if (!CmdBuf::send(file,str.c_str(),str.size()))
			return 0;
		
		DWORD resp = RecvResponse(file, file->sc_cmd);    
		while((resp==225)||(resp==426))
			resp = RecvResponseSkipAborReply(file, file->sc_cmd);
		//if((resp==250)||(resp==220))return 1;
		//char zz[32];itoa(resp,zz,10);MessageBox(0,zz,dir,0);
		MyWriteDebugLog("%s responce %u for '%ws'",str.c_str(),resp, file->serv.c_str());   
		switch(resp)
		{
		case 550:
			return false;
		default:
			return true;
		}
	}
	
	void SplitFtpPath(const std::wstring &fspath, std::wstring &serv,std::wstring &pathname)
	{
		size_t i=fspath.find(':');
		if((i==std::wstring::npos)||((++i)>=fspath.size()))
		{
			serv.resize(0);
			pathname.resize(0);
			return;
		}
		if((fspath[i]=='\\')||(fspath[i]=='/'))
		{  
			if((++i)>=fspath.size())
			{
				serv.resize(0);
				pathname.resize(0);
				return;
			}
		}
		
		int j=fspath.find('\\',i);
		if(j==std::wstring::npos)j=fspath.find('/',i);
		if(j==std::wstring::npos)
		{
			serv=fspath.substr(i,fspath.size()-i);
			pathname.resize(0);
		}else
		{
			serv=fspath.substr(i,j-i);
			pathname=fspath.substr(j+1,fspath.size()-j-1);
			for(size_t k=0;k<pathname.size();k++)if(pathname[k]=='\\')pathname[k]='/';
		}   
	}
	
	unsigned int xxxPathEnforce(const std::wstring &strPath)
	{
		std::wstring serv,pathname;
		SplitFtpPath(strPath, serv,pathname);
		//MessageBox(0,serv.c_str(),pathname.c_str(),0);
		if (serv.empty()||!wcsicmp(serv.c_str(),TEXT("desktop.ini")))
		{
			return ERROR_INVALID_NAME;
		}
		std::wstring ip;
		DWORD port;
		std::wstring user;
		std::wstring pass; 
		std::wstring home;
		DWORD flags;
		DWORD conlim;
		ENCCP enc;
		FTPSMODE smode;
		
		FtpSettings::CurrentSitesList()->GetHostSetup(serv, ip, port, user, pass, 
			home, flags, conlim, enc, smode);
		
		//MessageBox(0,serv.c_str(),ip.c_str(),0);
		if(ip.empty())return ::WSAGetLastError();
		
		FtpSettings::CurrentSitesList()->SetHostSetup(serv, ip, port, user, pass, 
			home, flags, conlim, enc, smode);
		
		return 0;
	}
	
	void FileResetOnError(FtpFilePtr file)
	{
		if (file->sc_data)
			file->sc_data.reset();
		
		if (file->sc_cmd)
			CloseCleanFtpConnectionsCache(file, true);
	}
	
	bool InaccessibleLocked(DWORD tm)
	{
		//return 1;
		DWORD ctm = ::GetTickCount();
		return ((tm<ctm) && ((ctm-tm)<5000));
	}
	
	void InaccessibleAdd(const std::wstring &pathname)
	{
		int con = 0;
		AutoCriticalSection cslock(iaf_cs);
		DWORD tm = ::GetTickCount()+(1000*FtpSettings::cache_dir_expire);
		STRINGDWORDMAP::iterator x=inaccessible_files.find(pathname);
		if (x!=inaccessible_files.end())
		{
			if (!InaccessibleLocked(x->second))
			{
				x->second = tm;
				con = 1;
			}
		}
		else
		{
			inaccessible_files.insert(
				STRINGDWORDMAP::value_type(
				pathname,tm));
			con = 2;
		}
		if (con)
			MyWriteDebugLog("InaccessibleAdd: '%ws' con=%i",pathname.c_str(),con);
	}
	
	void InaccessibleRemove(const std::wstring &pathname)
	{
		AutoCriticalSection cslock(iaf_cs);
		inaccessible_files[pathname] = ::GetTickCount()-10;

		MyWriteDebugLog("InaccessibleRemove: '%ws'",pathname.c_str());
	}
	
	bool InaccessibleCheck(const std::wstring &pathname)
	{
		bool con = false;
		AutoCriticalSection cslock(iaf_cs);
		STRINGDWORDMAP::iterator x=inaccessible_files.find(pathname);
		if (x!=inaccessible_files.end())
		{
			if (::GetTickCount()<x->second)
			{
				con = true;
			}else
			{
				if (!InaccessibleLocked(x->second))
					inaccessible_files.erase(x);   
			}
		}  
		MyWriteDebugLog("InaccessibleCheck: '%ws' con=%i",pathname.c_str(),(int)con);
		
		if (!con)
			return false;
		
		return true;
	}
	
	void InaccessibleClean()
	{
		AutoCriticalSection cslock(iaf_cs);
		for(;;)
		{
			bool con = false;
			for(STRINGDWORDMAP::iterator x=inaccessible_files.begin();
			x!=inaccessible_files.end();++x)
			{
				if (!InaccessibleLocked(x->second))
				{
					inaccessible_files.erase(x);
					con = true;
					break;
				}
			}
			if (!con)break;
		}
		MyWriteDebugLog("InaccessibleClean");
	}
	
	
	NTSTATUS xxxOpenFile(const std::wstring &strFileName,HANDLE &out,DWORD DesiredAccess,DWORD dwCreationDisposition,DWORD dwCreateOptions, bool RecursedDirDontCheckPath)
	{
		bool WantCheckExist=((dwCreationDisposition==FILE_OPEN)
			||(dwCreationDisposition==FILE_OVERWRITE)
			||(dwCreationDisposition==FILE_CREATE));
		
		bool WantCreateFile=((dwCreationDisposition==FILE_OVERWRITE)
			||(dwCreationDisposition==FILE_OVERWRITE_IF)
			||(dwCreationDisposition==FILE_CREATE)
			||(dwCreationDisposition==FILE_OPEN_IF)
			||(dwCreationDisposition==FILE_SUPERSEDE));
		
		bool NoCacheWrite=((dwCreateOptions&FILE_NO_INTERMEDIATE_BUFFERING)!=0);
		
		std::wstring parsedFileName(strFileName);
		if(parsedFileName.find(TEXT("\\??\\"))==0)parsedFileName.erase(0,4);
		std::wstring serv,pathname;
		
		SplitFtpPath(parsedFileName, serv,pathname);
		if( (serv.find(':')!=std::wstring::npos) || (pathname.find(':')!=std::wstring::npos))
		{
			MyWriteDebugLog("xxxOpenFile try to open NTFS additional stream: '%ws'@'%ws'",pathname.c_str(),serv.c_str());
			out = NULL;
			return STATUS_OBJECT_NAME_NOT_FOUND;
		}
		
		
		if(wcsicmp(serv.c_str(),TEXT("desktop.ini"))==0)
		{
			MyWriteDebugLog("xxxOpenFile desktop.ini as server - skipped");
			out = NULL;
			return STATUS_OBJECT_NAME_NOT_FOUND;
		}
		
		size_t sl=pathname.rfind('/');
		if(sl!=std::wstring::npos)
		{
			if((sl+1)==pathname.size())
			{
				pathname.resize(pathname.size()-1);
			}
		}
		
		sl=pathname.rfind('/'); 
		std::wstring name;
		if(sl==std::wstring::npos)
		{
			name=pathname;
		}else
		{
			name=pathname.substr(sl+1,pathname.size()-sl-1);
		}
		
		if(wcsicmp(name.c_str(),TEXT("desktop.ini"))==0)
		{
			MyWriteDebugLog("xxxOpenFile desktop.ini as file - skipped");
			out = NULL;
			return STATUS_OBJECT_NAME_NOT_FOUND;
		}
		
		if(pathname.empty())
		{
			DWORD attr =serv.size()>1?FILE_ATTRIBUTE_DIRECTORY:FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_DEVICE;
			if((attr==FILE_ATTRIBUTE_DIRECTORY)&&(dwCreateOptions&FILE_NON_DIRECTORY_FILE))
			{
				out = NULL;
				MyWriteDebugLog("xxxOpenFile file type conflict for volume or server '%ws'@'%ws' attr=%x",parsedFileName.c_str(),serv.c_str(),attr);
				return STATUS_FILE_IS_A_DIRECTORY;
			}
			
			FILETIME ft={0};
			out = PutOurFile(
				FtpFilePtr(new FtpFile(
				serv,std::wstring(),0,attr,
				ft,ft,ft,NoCacheWrite)));
			MyWriteDebugLog("xxxOpenFile opened volume or server '%ws'@'%ws'",parsedFileName.c_str(),serv.c_str());
			return STATUS_SUCCESS;
		}
		
		
		
		MyWriteDebugLog("xxxOpenFile for '%ws' DesiredAccess=%x, dwCreationDisposition=%x, dwCreateOptions=%x",
			pathname.c_str(), DesiredAccess, dwCreationDisposition, dwCreateOptions);
		
		if (WantCreateFile)
		{
			InaccessibleRemove(pathname);
		}else
		{
			if (InaccessibleCheck(pathname))
				return STATUS_OBJECT_NAME_NOT_FOUND;  
		}
		
		int OpenTracker=1;
		
		NTSTATUS ret=0xffffffff;
		//MessageBox(0,serv.c_str(),ip.c_str(),0);
		OpenTracker=2;
		std::wstring srchpath(pathname);
		while(srchpath.size()&&(srchpath[srchpath.size()-1]=='/'))srchpath.resize(srchpath.size()-1);
		std::wstring fname(srchpath);
		int slp=srchpath.rfind('/');
		if(slp!=std::wstring::npos)
		{
			srchpath.resize(slp);
			fname.erase(0,slp+1);
		}else srchpath.resize(0);
		
		FtpFindData foundwfd;
		if( (!RecursedDirDontCheckPath) && 
			(WantCheckExist||(!WantCreateFile)||(dwCreateOptions&FILE_DIRECTORY_FILE)))
		{  
			FindVector found;
			if(FtpEnum(strFileName, found))
			{
				for(FindVector::iterator v=found.begin();v!=found.end();++v)
				{
					if (wcsicmp((*v)->sFileName.c_str(),fname.c_str())==0)
					{
						foundwfd=**v;
						break;
					}
				}
			}
		}
		
		MyWriteDebugLog("xxxOpenFile point 4 srchpath='%ws' fname='%ws'",srchpath.c_str(),fname.c_str());
		if (foundwfd.sFileName.empty())
		{
			if ( RecursedDirDontCheckPath || WantCreateFile)
			{
				SYSTEMTIME st;GetSystemTime(&st);
				SystemTimeToFileTime(&st,&foundwfd.ftCreationTime);
				foundwfd.ftLastWriteTime=foundwfd.ftLastAccessTime=foundwfd.ftCreationTime;
				foundwfd.dwFileAttributes=(dwCreateOptions&FILE_NON_DIRECTORY_FILE)?FILE_ATTRIBUTE_NORMAL:FILE_ATTRIBUTE_DIRECTORY;
				foundwfd.nFileSizeHigh=foundwfd.nFileSizeLow=0;
				foundwfd.sFileName = fname.c_str();
			}
		}else
		{
			WantCreateFile=false;
		}
		
		bool CreatedFile=false;
		
		if (!foundwfd.sFileName.empty())
		{
			OpenTracker=5;
			MyWriteDebugLog("xxxOpenFile '%ws' attributes=%u",pathname.c_str(),foundwfd.dwFileAttributes);
			bool isdir = (foundwfd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)!=0;
			
			if ( ((dwCreateOptions&FILE_DIRECTORY_FILE)&&(!isdir))||((dwCreateOptions&FILE_NON_DIRECTORY_FILE)&&isdir))
			{
				out = NULL;
				ret = isdir?STATUS_FILE_IS_A_DIRECTORY:STATUS_NOT_A_DIRECTORY;
				MyWriteDebugLog("xxxOpenFile '%ws' file type conflict");
			}else      
			{    
				__int64 size;
				if(isdir)
				{
					size=0;
				}else
				{
					size=foundwfd.nFileSizeHigh;
					size<<=32;
					size|=foundwfd.nFileSizeLow;
				}
				
				FtpFilePtr file(new FtpFile(
					serv,pathname,size,
					foundwfd.dwFileAttributes,
					foundwfd.ftCreationTime,
					foundwfd.ftLastWriteTime,
					foundwfd.ftLastAccessTime,
					NoCacheWrite));
				
				out = PutOurFile(file);
				
				ret=STATUS_SUCCESS;
				MyWriteDebugLog("xxxOpenFile open success for '%ws':'%ws'",serv.c_str(),pathname.c_str());
				if(WantCreateFile)
				{
					if(dwCreateOptions&FILE_DIRECTORY_FILE)
					{
						bool crdir_ok = EnsureCommandConnection(out, file);
						
						if (crdir_ok)
							crdir_ok = ChangeFtpDirectory(out, file, file->path);
						
						if (crdir_ok)
							crdir_ok = SendCmd(file,"MKD",EncodePath(file->conv,ExtractFileName(pathname)).c_str());
						
						if (crdir_ok)
						{
							CreatedFile=true;
							file->changed=true;
							//ApplyFileSettings(out,file);
							AddFileToDirCache(file,foundwfd);
						}else
						{
							FileResetOnError(file);
							ret=STATUS_ACCESS_DENIED;
							xxxCloseHandle(out);
							out=NULL;
						}
					}else
					{
						CreatedFile=true;
						file->changed=true;
						//ApplyFileSettings(out,file);
						AddFileToDirCache(file,foundwfd);
						//ret=STATUS_OBJECT_NAME_NOT_FOUND;
						//xxxCloseHandle(out);
						//out=NULL;
					}
				}
			}
		}else
		{
			MyWriteDebugLog("xxxOpenFile FtpEnum can't find this file in enumeration");
			out = NULL;
			ret=STATUS_OBJECT_NAME_NOT_FOUND;
		}
		if(ret!=STATUS_SUCCESS)
		{
			MyWriteDebugLog("xxxOpenFile failed, OpenTracker=%i",OpenTracker);
			if(OpenTracker<4)
				InaccessibleAdd(pathname);
		}else
		{
			if(!CreatedFile)
			{
				if((dwCreationDisposition==FILE_OPEN_IF)&&(dwCreateOptions&FILE_NON_DIRECTORY_FILE))
				{
					if((DesiredAccess&GENERIC_WRITE)
						||(DesiredAccess&FILE_WRITE_DATA)
						||(DesiredAccess&GENERIC_ALL))
					{
						xxxCloseHandle(out);
						out = NULL;
						ret=STATUS_ACCESS_DENIED;
					}
				}else if(dwCreationDisposition==FILE_CREATE)
				{
					xxxCloseHandle(out);
					out = NULL;
					ret=STATUS_OBJECT_NAME_COLLISION;
				}
			}
			InaccessibleRemove(pathname);
		}
		return ret;
 } 
 
 bool EnsureCommandConnection(HANDLE hFile,FtpFilePtr file)
 {
	 if (file->sc_cmd)
		 return true;
	 
	 if (!file->ip)  
	 {
		 std::wstring ip,user,pass,home;
		 DWORD flags,port,conlim;
		 ENCCP enc;
		 FTPSMODE smode;
		 FtpSettings::CurrentSitesList()->GetHostSetup(file->serv, 
			 ip, port,user, pass, home, flags, conlim, enc, smode);
		 if(ip.empty())
			 return false;
		 
		 file->ip=FtpSettings::inet_addrW(ip.c_str());
		 file->port=port;
		 file->user=user;
		 file->pass=pass;
		 file->home=home;
		 file->pasv_mode=((flags&0x1000)!=0);
		 file->conlim=conlim;
		 file->smode=smode;
		 file->enc=enc;
		 switch(enc)
		 {
		 case EncKOI8R:
			 file->conv=GetKOI8RConv();
			 break;
			 
		 case EncKOI8U:
			 file->conv=GetKOI8UConv();
			 break;
			 
		 case EncDOS:
			 file->conv=GetDOSConv();
			 break;
			 
		 case EncUTF8:
			 file->conv=GetUTF8Conv();
			 break;
			 
		 case  EncWIN:
		 default:
			 file->conv=GetWINConv();
			 break;
		 }
		 
		 //ApplyFileSettings(hFile,file);
	 }
	 MyWriteDebugLog("EnsureCommandConnection - OpenFtpConnection for %ws...",file->serv.c_str());
	 file->sc_cmd = OpenFtpConnection(file);
	 return (file->sc_cmd);
 }
 
 IFtpTcpSocketPtr ListenDataSocket(FtpFilePtr &file)
 {
	 unsigned int local_ip;
	 unsigned short local_port;
	 if (!file->sc_cmd->local_name(local_ip,local_port))
		 return IFtpTcpSocketPtr();
	 
	 IFtpTcpSocketPtr serv_ptr(CreateFtpTcpSocket(false, true, true, FtpSettings::ftp_data_timeout));
	 
	 unsigned short server_port=0;
	 if (!serv_ptr->listen_on(local_ip, 0))
		 return IFtpTcpSocketPtr();
	 
	 if (!serv_ptr->local_name(local_ip,local_port))
		 return IFtpTcpSocketPtr();
	 
	 if (!CmdBuf().sendf(file,
		 "PORT %i,%i,%i,%i,%i,%i\r\n",
		 local_ip&0xff,(local_ip>>8)&0xff,
		 (local_ip>>16)&0xff,local_ip>>24,
		 local_port>>8,local_port&0xff))
	 {
		 return IFtpTcpSocketPtr();
	 }
	 
	 return serv_ptr;
 }
 
 bool AcceptDataSocket(FtpFilePtr &file, IFtpTcpSocketPtr sc_serv)
 {
	 unsigned int peer_ip;
	 unsigned short peer_port;
	 if (!file->sc_cmd->peer_name(peer_ip,peer_port))
	 {
		 SrvReplyLog::AddError(file->serv,file->sc_cmd->error(), 
			 "data", "accept", "peer_name failed");
		 return false;
	 }
	 
	 peer_port = 0;
	 AutoNetControl anc(net_control,sc_serv);
	 if (!sc_serv->accept_from(peer_ip,peer_port))
	 {
		 SrvReplyLog::AddError(file->serv,sc_serv->error(), 
			 "data", "accept", "accept_from failed");
		 return false;
	 }
	 
	 return true;
 }
 
 
 bool SendCwd(FtpFilePtr file,const char *dir)
 {
	 return SendCmd(file,"CWD",dir);
 }
 
 bool ChangeFtpDirectoryRelative(HANDLE hFile, FtpFilePtr file,const std::wstring &dir)
 {
	 //dir1/dir2/dir3/dir4
	 //dir1/dir2/dir5/dir6/dir7
	 //../../dir5/dir6/dir7
	 
	 STRINGVECTOR dir_v,cur_dir_v;
	 SplitPath(dir, dir_v);
	 SplitPath(file->curdir, cur_dir_v);
	 size_t i,j,first_diff=min(dir_v.size(),cur_dir_v.size());
	 for(i=0;i<first_diff;i++)
	 {
		 if(dir_v[i].compare(cur_dir_v[i]))
		 {
			 first_diff=i;
			 break;
		 }
	 }
	 
	 std::wstring curdir(file->MakeFileCurDir(false));
	 
	 for(i=first_diff,j=cur_dir_v.size();i<j;i++)
	 {
		 if (IsSymbolicLinkDir(curdir))
			 return false;
		 
		 if (!SendCwd(file,".."))
			 return false;
		 
		 size_t x = curdir.rfind('/');
		 if (x!=std::wstring::npos)
			 curdir.resize(x);
	 }
	 
	 for(i=first_diff,j=dir_v.size();i<j;i++)
	 {   
		 if (!SendCwd(file,EncodePath(file->conv,dir_v[i]).c_str()))
			 return false;
	 }
	 return true;
 }
 
 void prepare_cur_dir(std::wstring &dir, const std::wstring &home)
 {
	 size_t home_size=home.size(),home_ofs=0;
	 bool home_begin_slash=(home[0]=='/' || home[0]=='\\');
	 bool home_end_slash=(home_size>1 && (home[home_size-1]=='/' || home[home_size-1]=='\\'));
	 bool dir_begin_slash=(!dir.empty() && (dir[0]=='/' || dir[0]=='\\'));
	 
	 if (home_begin_slash)
	 {
		 home_ofs++;
		 home_size--;
	 }
	 
	 if(dir_begin_slash && home_end_slash)
	 {
		 dir.replace(0,1,home,home_ofs,home_size);
	 }
	 else 
	 {
		 if (!dir_begin_slash && !home_end_slash)
		 {
			 dir.insert((size_t)0,(size_t)1,'/');
		 }
		 dir.insert(0,home,home_ofs,home_size);
	 }
 }
 
 bool ChangeFtpDirectory(HANDLE hFile, FtpFilePtr file,const std::wstring &dir)
 {
	 bool out=false;
	 std::wstring new_dir(dir);
	 prepare_cur_dir(new_dir,file->home);
	 
	 if (!new_dir.compare(file->curdir))
		 return 1;
	 
	 out = ChangeFtpDirectoryRelative(hFile, file, new_dir);
	 
	 if (!out)
	 {
		 //MessageBox(0,dir.c_str(),file->curdir.c_str(),0);
		 out = SendCwd(file,"/");
		 if (!out)
		 {//Titan FTP server doesn't know about cwd /
			 for(int i=0;i<64;i++)
			 {
				 if (!SendCwd(file,".."))
				 {
					 out = true;
					 break;
				 }
			 }
		 }
		 
		 if (out)
		 {
			 file->curdir.resize(0);
			 //ApplyFileSettings(hFile,file);
			 out = ChangeFtpDirectoryRelative(hFile, file, new_dir);
		 }
	 }
	 if(out)
	 {
		 file->curdir=new_dir;
		 //ApplyFileSettings(hFile,file);
	 }
	 return out;
 }
 
 int AskDirListing(HANDLE hFile,FtpFilePtr file, IFtpTcpSocketPtr sc_serv)
 {
	 if (!CmdBuf::send(file,"LIST\r\n",6))
		 return 0;
	 
	 int out = 2;
	 MyWriteDebugLog("wait LIST responce");
	 DWORD resp;
	 do {
	 	resp = RecvResponseSkipAborReply(file, file->sc_cmd);
	 }while(resp==426);

	 if(resp!=425)
	 {
		 MyWriteDebugLog("LIST responce %u for '%ws'",resp, file->serv.c_str());   
		 if((resp!=450)&&(resp!=550))
		 {
			 if (sc_serv)
			 {
				 if (AcceptDataSocket(file,sc_serv))
					 file->sc_data = sc_serv;
				 else
					 file->sc_data.reset();
			 }
			 if (file->sc_data)
			 {  
				 ApplyDataSocketSSL(file);
			 }else 
			 {
				 out = 0;
				 MyWriteDebugLog("Data socket was not opened for %ws",file->serv.c_str());
			 }
		 }else
		 {
			 out = 0;
			 MyWriteDebugLog("Server rejected LIST");
		 }
	 }else
	 {
		 out = 1;//isretry?false:OpenDataConnection(hFile,file, true);
	 }
	 return out;
 }
 
 int AskFileContent(HANDLE hFile,FtpFilePtr file, IFtpTcpSocketPtr sc_serv, bool write)
 {
	 int out = 2;
	 
	 CmdBuf cmdbuf;
	 
	 if(!cmdbuf.sendf(file,"REST %I64i\r\n",file->pos))
		 return 0;
	 
	 MyWriteDebugLog("wait 'REST %I64i' responce",file->pos);
	 DWORD resp = RecvResponse(file, file->sc_cmd);
	 while(resp==426)resp = RecvResponseSkipAborReply(file, file->sc_cmd);
	 
	 MyWriteDebugLog("REST responce %u for '%ws'",resp, file->serv.c_str());   
	 if((resp==350)||(resp==110)||(!file->pos))
	 {
		 size_t sl_pos = file->path.rfind(TCHAR('/'));		 
		 if (!cmdbuf.sendf(file,
			 write?"STOR %s\r\n":"RETR %s\r\n",
			 EncodePath(file->conv,
				(sl_pos!=std::wstring::npos)
					?file->path.substr(sl_pos+1,file->path.size()-sl_pos-1)
					:file->path).c_str()))return 0;
		 
		 MyWriteDebugLog("wait transfer responce");
		 resp = RecvResponse(file, file->sc_cmd);    
		 while (resp==426||resp==350)
			 resp = RecvResponseSkipAborReply(file, file->sc_cmd);

		 if(resp!=425)
		 {
			 if((resp!=450)&&(resp!=550))
			 {
				 MyWriteDebugLog(
					 "AskFileContent: transfer request responce %u",
					 resp, file->serv.c_str());    
				 if(sc_serv)
				 {
					 if (AcceptDataSocket(file,sc_serv))
						 file->sc_data = sc_serv;
					 else
						 file->sc_data.reset();
				 }
				 
				 if(file->sc_data)
				 {  
					 ApplyDataSocketSSL(file);
					 //if(!ApplyFileSettings(hFile,file))out = 0;
				 }else 
				 {
					 out = 0;
					 MyWriteDebugLog("Data socket was not opened for %ws",file->serv.c_str());
				 }
			 }else
			 {
				 out = 0;
				 MyWriteDebugLog("Server rejected transfer");
			 }
		 }else
		 {
			 out = 1;//isretry?false:OpenDataConnection(hFile,file, true);
		 }
	 }else 
	 {
		 out = 0;
		 if (file->sc_data)
		 {
			 CloseDataSocket(file);
			 //ApplyFileSettings(hFile,file);
		 }
	 }
	 return out;
 }
 
 bool OpenDataConnection(HANDLE hFile,FtpFilePtr file, bool write, bool isretry=false)
 {    
	 Sleep(FtpSettings::pre_seek_delay); 
	 if (!ChangeFtpDirectory(hFile, file, 
		 (file->attr&FILE_ATTRIBUTE_DIRECTORY)?file->path+TEXT("/"):file->path))return 0;
	 MyWriteDebugLog("OpenDataConnection for '%ws' go",file->serv.c_str());
	 
	 
	 bool out=true;
	 if(file->attr&FILE_ATTRIBUTE_DIRECTORY)
		 CmdBuf::send(file,"TYPE A\r\n",8);
	 else
		 CmdBuf::send(file,"TYPE I\r\n",8);
	 MyWriteDebugLog("wait TYPE responce");
	 DWORD resp = RecvResponse(file, file->sc_cmd);
	 while((resp!=0xffffffff)&&
		 ((resp==425)||(resp==426)||(resp==225)||(resp>=550)||(resp==250)||
		 (resp==350)||(resp==110)||(resp==150)||(resp==450)||(resp==451)
		 ||(resp==230)||(resp==220)||(resp==331)))resp = RecvResponseSkipAborReply(file, file->sc_cmd);
	 MyWriteDebugLog("TYPE responce %u for '%ws'",resp, file->serv.c_str());
	 
	 IFtpTcpSocketPtr sc_serv;
	 if (!file->pasv_mode)
	 {	  
		 sc_serv = ListenDataSocket(file);
		 if (!sc_serv)
		 {
			 MyWriteDebugLog("ListenDataSocket failed, falling back to passive mode");
			 file->pasv_mode=1;
			 //ApplyFileSettings(hFile,file);
		 }else
		 {
			 MyWriteDebugLog("wait PORT responce");
			 resp = RecvResponseSkipAborReply(file, file->sc_cmd);
			 MyWriteDebugLog("PORT responce %u for '%ws'",resp, file->serv.c_str());  
		 }
	 }
	 
	 if (file->pasv_mode)
	 {
		 if (!CmdBuf::send(file,"PASV\r\n",6))
			 return false;

		 MyWriteDebugLog("wait PASV responce");
		 for(int i=0;;i++)
		 {
			 resp = RecvPasv(file);
			 if (resp!=200 && resp!=426 && resp!=226)
				 break;
			 
			 if(i==3)
				 break;
		 }
		 MyWriteDebugLog("PASV responce %u for '%ws'",resp, file->serv.c_str());
		 if (resp!=0)
		 {
			 return false;
		 }
	 }
	 
	 
	 ////////////////////////////
	 int askresult=(file->attr&FILE_ATTRIBUTE_DIRECTORY)?
		 AskDirListing(hFile,file, sc_serv):AskFileContent(hFile,file, sc_serv, write);
	 
	 //out = (askresult==2);
	 if(askresult==1) out = isretry?false:OpenDataConnection(hFile, file, write, true);
	 else if(askresult==2) out = true; else out = false;
	 /////////////////////////////////////////////////    
	 
	 
	 return out;
 }
 
 void SetFilePointer(HANDLE hFile,FtpFilePtr file,__int64 newpos, bool NoCacheWrite, bool write)
 {    
	 if(newpos!=file->pos)
	 {  
		 MyWriteDebugLog("SetFilePointer %ws: %u->%u",file->serv.c_str(),(DWORD)file->pos,(DWORD)newpos);  
		 
		 
		 if(file->sc_data)
		 {    //(__int64)((__int64)1024*(__int64)FtpSettings::forward_seek_bypass)    
			 __int64 MaxForwardSize;
			 if (file->DataConnectDelay&&file->DataTransferSpeed) 
			 {
				 MaxForwardSize=(file->DataConnectDelay*file->DataTransferSpeed*1000);
				 if(MaxForwardSize>LIMIT_MAXFORWARDSIZE)MaxForwardSize=LIMIT_MAXFORWARDSIZE;
				 if(MaxForwardSize<MINIMUM_MAXFORWARDSIZE)MaxForwardSize=MINIMUM_MAXFORWARDSIZE;     
			 }
			 if((!write)&&(FtpSettings::adv_flags&FTPS_ADV_FORWARDBYPASS)&&(newpos>file->pos)&&((newpos-file->pos)<MaxForwardSize))
			 {
				 DWORD totalrecv=0;
				 std::vector<char> tmp(BYPASS_BUFFER_SIZE);
				 DWORD tm=GetTickCount();     
				 for(;;)
				 {
					 int maxlen=tmp.size();
					 if((__int64)maxlen>(newpos-file->pos))maxlen=(int)(__int64)(newpos-file->pos);
					 int ret=file->sc_data->recv(&tmp[0],maxlen);//,FtpSettings::ftp_data_timeout);
					 if(ret<=0)break;
					 totalrecv+=(DWORD)ret;
					 LARGE_INTEGER iPointer;iPointer.QuadPart=file->pos;
					 if(!file->no_cache_write)WriteToCache(file,&tmp[0],&iPointer, (DWORD)ret);
					 file->pos=file->pos+(__int64)ret;
					 if((GetTickCount()-tm)>(2*file->DataConnectDelay))break;
					 if(file->pos==newpos)break;
				 }
				 
				 tm=GetTickCount()-tm;
				 if(totalrecv&&(tm>50))
				 {
					 DWORD DataTransferSpeed=(1000*totalrecv)/tm;
					 if(file->DataTransferSpeed)
						 file->DataTransferSpeed=(DataTransferSpeed+file->DataTransferSpeed)/2;
					 else
						 file->DataTransferSpeed=DataTransferSpeed;
				 }
				 
				 if(file->pos==newpos)
					 MyWriteDebugLog("SetFilePointer forward bypass OK. Time %u",GetTickCount()-tm);  
				 else
					 MyWriteDebugLog("SetFilePointer forward bypass FAILED or DISCARDED");  
			 }
			 
			 if(newpos!=file->pos)
			 {
				 CloseDataSocket(file);
				 CmdBuf::send(file,"ABOR\r\n",6);
				 DWORD resp = RecvResponse(file, file->sc_cmd);      
				 if(resp == 550)//bypass 550 File transfer failed if any
				 {
					 MyWriteDebugLog("Got responce 550, getting more for %ws",file->serv.c_str());  
					 resp = RecvResponse(file, file->sc_cmd);
				 }    
				 MyWriteDebugLog("SetFilePointer %ws: ABOR responce=%u",file->serv.c_str(),resp);  
			 }
		 }
		 file->pos=newpos;
		 //ApplyFileSettings(hFile,file);
	 }   
 }
 
 __int64 rawGetSize(HANDLE hFile,FtpFilePtr file, const std::wstring &fname)
 {//assume that command connection ready!
	 if (!ChangeFtpDirectory(hFile, file, file->path+TEXT("/")))
		 return -1;
	 
	 if (!CmdBuf().sendf(
		 file,"SIZE %s\r\n",
		 file->conv->Encode(fname).c_str()))
		 return -1;
	 
	 std::string txt;
	 
	 for(;;)
	 {
		 DWORD resp = RecvResponseSkipAborReply(file, file->sc_cmd, txt);
		 
		 if (resp==0xffffffff || resp==550)
			 return -1;
		 
		 if (resp!=150 && resp!=426 && resp!=225) 
			 break;
	 }
	 
	 
	 std::string digits;
	 for(size_t i=0;i<txt.size();i++)
		 if (isdigit(txt[i]))digits.append((size_t)1,txt[i]);
		 
		 //MyWriteDebugLog("size reply for '%s' : %s [%s]",str.c_str(),txt.c_str(),digits.c_str());
		 
		 if (digits.empty())
			 return -1;
		 
		 return _atoi64(digits.c_str());
 }
 
 bool rawFtpEnum(HANDLE hFile, FtpFilePtr file, std::vector<char> &buf)
 {//assume that command connection ready!
	 MyWriteDebugLog("rawFtpEnum entered for %ws",file->serv.c_str()); 
	 if(!(file->attr&FILE_ATTRIBUTE_DIRECTORY))
	 {
		 MyWriteDebugLog("rawFtpEnum try to enum from file"); 
		 return false;
	 }
	 /*
	 if (!EnsureCommandConnection(hFile,file))
	 {
	 return false;
	 }
	 */
	 if (!file->sc_data)
	 {
		 if (!OpenDataConnection(hFile,file,false))
		 {
			 return false;
		 }
	 }   
	 
	 std::vector<char> tmp(0x10000);
	 for(;;)
	 {
		 int transferred=file->sc_data->recv(&tmp[0],tmp.size());//,FtpSettings::ftp_data_timeout);
		 if(transferred<=0)
			 break;
		 int oldsz=buf.size();
		 buf.resize(buf.size()+transferred);
		 memcpy(&buf[oldsz],&tmp[0],transferred);
		 if(((size_t)transferred==tmp.size())&&(tmp.size()<512*1024))tmp.resize(2*tmp.size());
		 if(tmp.size()>0x2000000)break;
	 }  
	 //MyWriteDebugLog("rawFtpEnum CloseCleanFtpConnectionsCache %u",file->sc_cmd);  
	 CloseDataSocket(file);
	 //closesocket(file->sc_cmd);
	 //ApplyFileSettings(hFile,file);
	 return true;
 }
 
 NTSTATUS xxxReadFile(HANDLE hFile,LPVOID lpBuffer,PLARGE_INTEGER liPointer, DWORD nNumberOfBytesToProcess,LPDWORD lpNumberOfBytesReady)
 {  
	 *lpNumberOfBytesReady=0;
	 
	 FtpFilePtr file;
	 FileLocker locker(hFile,file);
	 if (!locker.LockSucceed)
		 return STATUS_INVALID_HANDLE;
	 
	 MyWriteDebugLog("xxxReadFile entered for %ws, pos=%u, size=%u, nNumberOfBytesToProcess=%u",file->serv.c_str(),(DWORD)file->pos,(DWORD)file->size,nNumberOfBytesToProcess); 
	 
	 
	 if (file->attr&FILE_ATTRIBUTE_DIRECTORY)
	 {
		 MyWriteDebugLog("xxxReadFile try to read from directory. Make app happy - give it EOF."); 
		 *lpNumberOfBytesReady=0;
		 return STATUS_SUCCESS;//STATUS_END_OF_FILE;
	 }
	 
	 if (liPointer->QuadPart>=file->size)
	 {
		 MyWriteDebugLog("xxxReadFile (liPointer->QuadPart>=file->size)"); 
		 *lpNumberOfBytesReady=0;
		 return STATUS_SUCCESS;//STATUS_END_OF_FILE;
	 }
	 
	 
	 
	 LARGE_INTEGER IOPointer=*liPointer;
	 
	 if((__int64)nNumberOfBytesToProcess>(file->size-IOPointer.QuadPart))nNumberOfBytesToProcess=(DWORD)(file->size-IOPointer.QuadPart);
	 
	 
	 if((file->pos!=IOPointer.QuadPart)||(!file->sc_data))
	 {
		 DWORD nNumberOfBytesToProcessFromCache=nNumberOfBytesToProcess;
		 if((file->pos>IOPointer.QuadPart)&&(file->pos<(IOPointer.QuadPart+(__int64)nNumberOfBytesToProcessFromCache)))
		 {
			 nNumberOfBytesToProcessFromCache=(DWORD)(__int64)(file->pos-IOPointer.QuadPart);
		 }
		 
		 *lpNumberOfBytesReady=ReadFromCache(file,lpBuffer,&IOPointer, nNumberOfBytesToProcessFromCache);
		 
		 
		 if(*lpNumberOfBytesReady==nNumberOfBytesToProcess)
		 {
			 MyWriteDebugLog("xxxReadFile read totally from cache for %ws, pos=%u, size=%u, *lpNumberOfBytesReady=%u",file->serv.c_str(),(DWORD)IOPointer.LowPart,(DWORD)file->size,*lpNumberOfBytesReady);     
			 return STATUS_SUCCESS;
		 }
		 
		 if(*lpNumberOfBytesReady)
		 {
			 MyWriteDebugLog("xxxReadFile read partially from cache for %ws, pos=%u, size=%u, *lpNumberOfBytesReady=%u",file->serv.c_str(),(DWORD)IOPointer.LowPart,(DWORD)file->size,*lpNumberOfBytesReady); 
			 IOPointer.QuadPart=IOPointer.QuadPart+(__int64)*lpNumberOfBytesReady;
			 nNumberOfBytesToProcess-=*lpNumberOfBytesReady;
			 lpBuffer=((char *)lpBuffer)+*lpNumberOfBytesReady;
		 }
	 }
	 
	 SetFilePointer(hFile,file,IOPointer.QuadPart,file->no_cache_write, false);
	 
	 //if(nNumberOfBytesToProcess>(file->size-file->pos))nNumberOfBytesToProcess=(DWORD)(file->size-file->pos);
	 
	 MyWriteDebugLog("xxxReadFile prepared, length = %u",nNumberOfBytesToProcess);
	 DWORD totalread=0;
	 
	 NTSTATUS out=STATUS_SUCCESS;
	 
	 if(nNumberOfBytesToProcess)
	 {   
		 if (!EnsureCommandConnection(hFile,file))
		 {
			 FileResetOnError(file);
			 return STATUS_DATA_ERROR; 
		 }
		 
		 if (!file->sc_data)
		 {
			 DWORD tm=GetTickCount();
			 if (!OpenDataConnection(hFile,file,false))
			 {
				 FileResetOnError(file);
				 return STATUS_DATA_ERROR; 
			 }
			 DWORD DataConnectDelay=GetTickCount()-tm;
			 if (file->DataConnectDelay)
				 file->DataConnectDelay=(DataConnectDelay+file->DataConnectDelay)/2;
			 else
				 file->DataConnectDelay=DataConnectDelay;
		 }   
		 
		 for(;;)
		 {
			 //MyWriteDebugLog("xxxReadFile go recv(%u), length = %u",file->sc_data,nNumberOfBytesToProcess-totalread);    
			 int transferred=file->sc_data->recv(((char *)lpBuffer)+totalread,nNumberOfBytesToProcess-totalread);//,FtpSettings::ftp_data_timeout);
			 if(transferred<=0)
			 {//todo - ask reconnect
				 SrvReplyLog::AddError(file->serv,file->sc_data->error(), 
					 "data", "recv", "read file");
				 MyWriteDebugLog("xxxReadFile recv error %u",WSAGetLastError());
				 out=STATUS_DATA_ERROR;
				 break;
			 }
			 totalread+=transferred; 
			 MyWriteDebugLog("xxxReadFile progress %u of %u",transferred,totalread);
			 if(totalread>=nNumberOfBytesToProcess)break;
		 }
		 
		 if(totalread)
		 {
			 if(!file->no_cache_write)WriteToCache(file,lpBuffer,&IOPointer, totalread);
			 file->pos=file->pos+totalread;
		 }else out=STATUS_DATA_ERROR;            
	 }
	 
	 if (out!=STATUS_SUCCESS)
	 {
		 FileResetOnError(file);
	 }
	 //ApplyFileSettings(hFile,file);
	 *lpNumberOfBytesReady=(*lpNumberOfBytesReady)+totalread;    
	 return out;
  }
  /////////////////////////////////////////////////////////////////////////////////// 
  NTSTATUS xxxWriteFile(HANDLE hFile,LPVOID lpBuffer,PLARGE_INTEGER liPointer, DWORD nNumberOfBytesToProcess,LPDWORD lpNumberOfBytesReady)
  {
	  if(!liPointer->QuadPart)
	  {
		  MyWriteDebugLog("xxxWriteFile cleanup"); 
		  xxxRenameFile(hFile,std::wstring());
	  }
	  *lpNumberOfBytesReady=0;
	  
	  FtpFilePtr file;
	  FileLocker locker(hFile,file);
	  if(!locker.LockSucceed)return STATUS_INVALID_HANDLE;
	  
	  MyWriteDebugLog("xxxWriteFile entered for %ws, pos=%u, size=%u, nNumberOfBytesToProcess=%u",file->serv.c_str(),(DWORD)file->pos,(DWORD)file->size,nNumberOfBytesToProcess); 
	  
	  
	  if(file->attr&FILE_ATTRIBUTE_DIRECTORY)
	  {
		  MyWriteDebugLog("xxxWriteFile try to write to directory. Make app happy - give it EOF."); 
		  *lpNumberOfBytesReady=0;
		  return STATUS_SUCCESS;//STATUS_END_OF_FILE;
	  }  
	  
	  LARGE_INTEGER IOPointer=*liPointer;
	  
	  /*
	  if((__int64)nNumberOfBytesToProcess>(file->size-IOPointer.QuadPart))nNumberOfBytesToProcess=(DWORD)(file->size-IOPointer.QuadPart);
	  
		
		  if((file->pos!=IOPointer.QuadPart)||(!file->sc_data))
		  {
		  DWORD nNumberOfBytesToProcessFromCache=nNumberOfBytesToProcess;
		  if((file->pos>IOPointer.QuadPart)&&(file->pos<(IOPointer.QuadPart+(__int64)nNumberOfBytesToProcessFromCache)))
		  {
		  nNumberOfBytesToProcessFromCache=(DWORD)(__int64)(file->pos-IOPointer.QuadPart);
		  }
		  
			*lpNumberOfBytesReady=ReadFromCache(file,lpBuffer,&IOPointer, nNumberOfBytesToProcessFromCache);
			
			  
				if(*lpNumberOfBytesReady==nNumberOfBytesToProcess)
				{
				MyWriteDebugLog("xxxReadFile read totally from cache for %ws, pos=%u, size=%u, *lpNumberOfBytesReady=%u",file->serv.c_str(),(DWORD)IOPointer.LowPart,(DWORD)file->size,*lpNumberOfBytesReady);     
				return STATUS_SUCCESS;
				}
				
				  if(*lpNumberOfBytesReady)
				  {
				  MyWriteDebugLog("xxxReadFile read partially from cache for %ws, pos=%u, size=%u, *lpNumberOfBytesReady=%u",file->serv.c_str(),(DWORD)IOPointer.LowPart,(DWORD)file->size,*lpNumberOfBytesReady); 
				  IOPointer.QuadPart=IOPointer.QuadPart+(__int64)*lpNumberOfBytesReady;
				  nNumberOfBytesToProcess-=*lpNumberOfBytesReady;
				  lpBuffer=((char *)lpBuffer)+*lpNumberOfBytesReady;
				  }
				  }
				  
	  */
	  file->changed=true;
	  if (file->pos!=IOPointer.QuadPart)
	  {
	  /* if(IOPointer.QuadPart!=0 && IOPointer.QuadPart!=file->size)		  
	  {
	  wchar_t zzz[256];wsprintf(zzz,L"%u %u %u",
	  (DWORD)file->size,(DWORD)file->pos,IOPointer.LowPart);
	  MessageBox(0,zzz,file->path.c_str(),0);
	  return STATUS_NOT_SUPPORTED;
	  }*/
		  
		  SetFilePointer(hFile,file,IOPointer.QuadPart,file->no_cache_write, true);
	  }
	  
	  
	  //if(nNumberOfBytesToProcess>(file->size-file->pos))nNumberOfBytesToProcess=(DWORD)(file->size-file->pos);
	  
	  MyWriteDebugLog("xxxWriteFile prepared, length = %u",nNumberOfBytesToProcess);
	  
	  NTSTATUS out=STATUS_SUCCESS;
	  
	  if(nNumberOfBytesToProcess)
	  {   
		  if (!EnsureCommandConnection(hFile,file))
		  {
			  FileResetOnError(file);
			  return STATUS_DATA_ERROR; 
		  }
		  
		  if (!file->sc_data)
		  {
			  DWORD tm=GetTickCount();
			  if (!OpenDataConnection(hFile,file,true))
			  {
				  FileResetOnError(file);
				  return STATUS_DATA_ERROR; 
			  }
			  DWORD DataConnectDelay=GetTickCount()-tm;
			  if(file->DataConnectDelay)
				  file->DataConnectDelay=(DataConnectDelay+file->DataConnectDelay)/2;
			  else
				  file->DataConnectDelay=DataConnectDelay;
		  }   
		  
		  int transferred=file->sc_data->send((char *)lpBuffer,nNumberOfBytesToProcess);
		  if(transferred<=0)
		  {//todo - ask reconnect
			  SrvReplyLog::AddError(file->serv,file->sc_data->error(), 
				  "data", "send", "write file");
			  MyWriteDebugLog("xxxWriteFile send error %u",WSAGetLastError());     
			  out=STATUS_DATA_ERROR;
		  }else  
		  {
			  if (!file->no_cache_write)
				  WriteToCache(file,lpBuffer,&IOPointer, transferred);
			  file->pos = file->pos+transferred;
			  if (file->size<file->pos)
				  file->size = file->pos;
			  *lpNumberOfBytesReady=(*lpNumberOfBytesReady)+transferred;
		  }
	  }
	  
	  if (out!=STATUS_SUCCESS)
	  {
		  FileResetOnError(file);
	  }
	  
	  //ApplyFileSettings(hFile,file);
	  return out;
  }
  //////////////////////////////////////////////////////////////////////////////////////  
  
  std::wstring ExtractFilePath(const std::wstring &strPathName, bool with_slash)
  {
	  size_t p=strPathName.rfind(TCHAR('/'));
	  if(p==std::wstring::npos)
		  p=strPathName.rfind(TCHAR('\\'));
	  if(p==std::wstring::npos)
		  return std::wstring();
	  return strPathName.substr(0,with_slash?p+1:p);
  }
  
  std::wstring ExtractFileName(const std::wstring &strPathName)
  {
	  size_t p=strPathName.rfind(TCHAR('/'));
	  if(p==std::wstring::npos)
		  p=strPathName.rfind(TCHAR('\\'));
	  if(p==std::wstring::npos)
		  return strPathName;
	  return strPathName.substr(p+1,strPathName.size()-p-1);
  }
  
  NTSTATUS xxxRenameFile(HANDLE hFile,const std::wstring &strNewName)
  {
	  FtpFilePtr file;
	  FileLocker locker(hFile,file);
	  if(!locker.LockSucceed)return STATUS_INVALID_HANDLE;
	  //file->path
	  MyWriteDebugLog("xxxRenameFile - %ws -> %ws...",file->serv.c_str(),strNewName.c_str());
	  
	  if (!EnsureCommandConnection(hFile,file))
	  {
		  FileResetOnError(file);
		  return STATUS_ACCESS_DENIED;
	  }
	  
	  if(!ChangeFtpDirectory(hFile, file, file->path))
		  return STATUS_ACCESS_DENIED;
	  
	  std::wstring from=ExtractFileName(file->path);
	  
	  if(strNewName.empty())
	  {
		  if(file->attr&FILE_ATTRIBUTE_DIRECTORY)
		  {
			  if(!SendCmd(file,"RMD",EncodePath(file->conv,from).c_str()))
				  return STATUS_ACCESS_DENIED;
		  }else
		  {
			  if(!SendCmd(file,"DELE",EncodePath(file->conv,from).c_str()))
				  return STATUS_ACCESS_DENIED;
		  }
	  }else
	  {
		  std::wstring to=ExtractFileName(strNewName);
		  
		  if(!SendCmd(file,"RNFR",EncodePath(file->conv,from).c_str()))
			  return STATUS_ACCESS_DENIED;
		  
		  if(!SendCmd(file,"RNTO",EncodePath(file->conv,to).c_str()))
			  return STATUS_ACCESS_DENIED;
	  }
	  
	  InaccessibleClean();
	  CleanFileFromDataCache(file);
	  if (strNewName.empty())
		  CleanFileFromDirCache(file);
	  else
		  FtpEnumRefreshFile(hFile, file);
	  
	  return STATUS_SUCCESS;
  }
  
  NTSTATUS xxxCloseHandle(HANDLE h)
  {
	  {
		  FtpFilePtr file;
		  FileLocker locker(h,file);
		  if (!locker.LockSucceed)
			  return STATUS_INVALID_HANDLE;
		  if (file->changed)
			  FtpEnumRefreshFile(h, file);
	  }  
	  
	  return CloseOurFile(h)?STATUS_SUCCESS:STATUS_INVALID_HANDLE;
  }
  ////////////////////////////////////////////////////////////////////////////////////
  
  void GetVirtualFilePath(FtpFilePtr file, TCHAR *buf)
  {
	  _snwprintf(buf,ANYFS_MAX_PATH,TEXT("\\??\\%c:\\%s\\%s"),FtpSettings::FtpFsDrive[0],file->serv.c_str(),file->path.c_str());
	  for(unsigned int i=0;i<ANYFS_MAX_PATH;i++)if(buf[i]=='/')buf[i]='\\';   
  }
  std::wstring GetVirtualFilePath(FtpFilePtr file)
  {  
	  std::vector<TCHAR> tmp(ANYFS_MAX_PATH);
	  GetVirtualFilePath(file, &tmp[0]);
	  return std::wstring(&tmp[0]);
  }
  
  NTSTATUS xxxGetDirChild(HANDLE hFile, unsigned int index, unsigned int pid,FtpFindDataPtr &wfd)
  {  
	  FtpFilePtr file;
	  FileLocker locker(hFile,file);
	  if(!locker.LockSucceed)return STATUS_INVALID_HANDLE;
	  
	  if(!(file->attr&FILE_ATTRIBUTE_DIRECTORY))
	  {
		  MyWriteDebugLog("xxxGetDirChild for file serv=%ws path=%ws index=%u",file->serv.c_str(),file->path.c_str(),index);
		  return STATUS_NO_MORE_FILES;
	  }  
	  if(!file->dir_enumerated)
	  {
		  std::wstring pathname(file->path);
		  MyWriteDebugLog("xxxGetDirChild go enum"); 
		  for(;;)
		  {
			  if( FtpEnum(hFile,file,file->dir_children))break;    
			  FileResetOnError(file);
			  DWORD lerr=GetLastError();
			  if( !ErrorHandler::CheckRetry(OPCODE_ENUM, lerr?lerr:WSAGetLastError(),
				  pid,GetVirtualFilePath(file)))break;
		  }
		  file->dir_enumerated=true;
		  
		  //ApplyFileSettings(hFile,file);
	  }
	  MyWriteDebugLog("xxxGetDirChild serv=%ws path=%ws index=%u size=%u",file->serv.c_str(),file->path.c_str(),index,file->dir_children.size());
	  
	  if(index>=file->dir_children.size())
		  return index?STATUS_NO_MORE_FILES:STATUS_NO_SUCH_FILE;
	  
	  wfd=file->dir_children[index];  
	  //if(!wfd)MessageBoxA(0,"wow","zz",0);
	  //ApplyFileSettings(hFile,file);
	  return STATUS_SUCCESS;
  }
  
  void xxxGetFileInfo(HANDLE hFile, ANYFS_INFO_REPLY &info)
  {
	  FtpFilePtr file;
	  FileLocker locker(hFile,file);
	  if(locker.LockSucceed)
	  {   
		  info.nt_status=STATUS_SUCCESS;
		  info.ac_time.HighPart=file->actime.dwHighDateTime;
		  info.ac_time.LowPart=file->actime.dwLowDateTime;
		  info.wr_time.HighPart=file->wrtime.dwHighDateTime;
		  info.wr_time.LowPart=file->wrtime.dwLowDateTime;
		  info.cr_time.HighPart=file->crtime.dwHighDateTime;
		  info.cr_time.LowPart=file->crtime.dwLowDateTime;   
		  info.size.QuadPart=file->size;
		  info.attributes=file->attr;   
		  GetVirtualFilePath(file,info.file_name);
	  }else info.nt_status=STATUS_INVALID_HANDLE;
  }
  
  
  void xxxPrepareTempFile(HANDLE hFile, unsigned int max_size, ANYFS_TEMP_REPLY &reply)
  {
	  FtpFilePtr file;
	  FileLocker locker(hFile,file);
	  if(locker.LockSucceed)
	  {   
		  reply.size=(unsigned int)(unsigned __int64)file->size;
		  if(reply.size==0)
		  {    
			  reply.nt_status=file->attr&FILE_ATTRIBUTE_DIRECTORY?STATUS_INVALID_FILE_FOR_SECTION:STATUS_MAPPED_FILE_SIZE_ZERO;
			  return;
		  }
		  
		  std::wstring fpathname(file->MakeFilePathName(false));
		  
		  std::wstring dir_path(fpathname);
		  size_t psl=dir_path.rfind('/');
		  if( psl!=std::wstring::npos)dir_path.resize(psl);
		  for(size_t x=0;x<dir_path.size();x++)if(dir_path[x]=='/')dir_path[x]='\\';
		  if(dir_path.find('\\')!=0)dir_path.insert(0,TEXT("\\"));
		  dir_path.insert(0,tempdir);
		  EnsureDirExist(dir_path);
		  
		  
		  TCHAR sdig[16];
		  if(max_size!=0xffffffff)wsprintf(sdig,TEXT("_%x"),max_size);else sdig[0]=0;
		  fpathname.append(sdig);
		  for(;;)
		  {
			  temp_cs.Enter();
			  if(temp_busy.find(fpathname)==temp_busy.end())break;
			  temp_cs.Leave();
			  Sleep(10);
		  }   
		  
		  FILE2TEMPMAP::iterator i=map_temp_files.find(fpathname);
		  bool ready=false;
		  if(i!=map_temp_files.end())
		  {
			  WIN32_FIND_DATA wfd;
			  HANDLE fh=FindFirstFile(i->second.temp_file.c_str(),&wfd);
			  if(fh==INVALID_HANDLE_VALUE)
			  {
				  map_temp_files.erase(i);
			  }else
			  {
				  FindClose(fh);
				  if(wfd.nFileSizeLow!=(file->size&0xffffffff))
				  {
					  map_temp_files.erase(i);
				  }else
				  {
					  ready=true;
					  i->second.expire=GetTickCount()+(1000*FtpSettings::cache_file_expire);
					  wcsncpy(reply.file_name,i->second.temp_file.c_str(),ANYFS_MAX_PATH);
					  reply.nt_status=STATUS_SUCCESS;
				  }
			  }
		  }
		  temp_busy.insert(fpathname);
		  temp_cs.Leave();
		  if(!ready)
		  {
			  int len=tempdir.size();
			  CreateDirectory(tempdir.c_str(),NULL);
			  std::vector<TCHAR> temp(len+ANYFS_MAX_PATH);
			  wcscpy(&temp[0],tempdir.c_str());
			  HANDLE tmpfile;
			  std::wstring realname(file->path);
			  int x=realname.rfind('/');
			  if(x)realname.erase(0,x+1);
			  
			  for(int j=0;j<9999;j++)
			  {
				  if(j==0)
					  wsprintf(&temp[0]+len,TEXT("\\%s%s"),realname.c_str(),sdig);
				  else
					  wsprintf(&temp[0]+len,TEXT("\\%s_%i%s"),realname.c_str(),j,sdig);
				  
				  if((GetFileAttributes(&temp[0])==0xffffffff)&&(GetLastError()==ERROR_FILE_NOT_FOUND))break;
			  }
			  
			  tmpfile=CreateFile(&temp[0],GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);//|GENERIC_EXECUTE//|FILE_FLAG_DELETE_ON_CLOSE
			  MyWriteDebugLog("xxxPrepareTempFile name='%ws' temp='%ws'",fpathname.c_str(),&temp[0]);    
			  
			  if(tmpfile!=INVALID_HANDLE_VALUE)
			  {
				  LARGE_INTEGER filepos={0,0};
				  std::vector<char> io_buff(0xffff);
				  bool copied=false;
				  for(;;)
				  {
					  DWORD dw=0;      
					  NTSTATUS rv=xxxReadFile(hFile,&io_buff[0],&filepos,io_buff.size(),&dw);
					  if((rv==STATUS_END_OF_FILE)||((rv==STATUS_SUCCESS)&&(dw==0)))
					  {
						  copied=true;
						  break;
					  }
					  if(rv!=STATUS_SUCCESS)
					  {
						  MyWriteDebugLog("xxxPrepareTempFile name='%ws' xxxReadFile error=%x",fpathname.c_str(),rv);
						  break;
					  }      
					  if(!WriteFile(tmpfile,&io_buff[0],dw,&dw,NULL))break;
					  filepos.QuadPart=filepos.QuadPart+(unsigned __int64)dw;
					  if(filepos.LowPart>=max_size)
					  {
						  copied=true;
						  break;
					  }      
				  }
				  CloseHandle(tmpfile);
				  if(copied)
				  {
					  AutoCriticalSection cs_lock(temp_cs);
					  FILE2TEMPMAP::iterator i=map_temp_files.find(fpathname);   
					  if(i==map_temp_files.end())
					  {
						  FTPTEMP ft;
						  ft.expire=GetTickCount()+(1000*FtpSettings::cache_file_expire);
						  ft.temp_file.assign(&temp[0]);       
						  map_temp_files.insert(FILE2TEMPMAP::value_type(fpathname,ft));
					  }
					  wcscpy(reply.file_name,&temp[0]);
					  reply.nt_status=STATUS_SUCCESS;
					  MyWriteDebugLog("xxxPrepareTempFile name='%ws' temp=%ws ok",fpathname.c_str(),&temp[0]);
				  }
				  else
				  {
					  DeleteFile(&temp[0]);
					  reply.nt_status=STATUS_DATA_ERROR;
				  }     
			  }
			  else 
			  {
				  MyWriteDebugLog("xxxPrepareTempFile name='%ws' temp create error=%u",fpathname.c_str(),GetLastError());
				  reply.nt_status=STATUS_ACCESS_DENIED;
			  }
		  }
		  
		  AutoCriticalSection cs_lock(temp_cs);
		  ICSTRINGSET::iterator b = temp_busy.find(fpathname);
		  if (b!=temp_busy.end())
			  temp_busy.erase(b);

   }
   else 
	   reply.nt_status=STATUS_INVALID_HANDLE;
  }
  
}
