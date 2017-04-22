#include "StdAfx.h"
#include "SrvReplyLog.h"
#include <map>
#include <list>
#include <winsock.h>
#include <boost/shared_ptr.hpp>
#include "../AnyFS/SomeUsefull.h"
#include "../FtpSettings/settings.h"
#include "../FtpFS/ntdefs.h"
#include "../APIHooks/NTDllExports.h"

namespace SrvReplyLog
{
	class SrvReply
	{
	private:
		std::string _txt;
		SYSTEMTIME _st;

	public:
		SrvReply(const std::string &txt_):_txt(txt_)
		{
			::GetLocalTime(&_st);
		}

		inline std::string txt()
		{
			return _txt;
		}

		inline unsigned int hours()
		{
			return _st.wHour;
		}

		inline unsigned int mins()
		{
			return _st.wMinute;
		}

		inline unsigned int secs()
		{
			return _st.wSecond;
		}

		inline bool is_too_old()
		{
			SYSTEMTIME cst;
			FILETIME ft,cft;
			::GetLocalTime(&cst);
			::SystemTimeToFileTime(&_st,&ft);
			::SystemTimeToFileTime(&cst,&cft);
			ULARGE_INTEGER uli_ft={ft.dwLowDateTime,ft.dwHighDateTime};
			ULARGE_INTEGER uli_cft={cft.dwLowDateTime,cft.dwHighDateTime};
			ULONGLONG uli_delta = uli_cft.QuadPart - uli_ft.QuadPart;
			uli_delta/=10000000;
			return (uli_delta>60);
		}
	};

	typedef boost::shared_ptr<SrvReply>		SrvReplyPtr;
	typedef std::list<SrvReplyPtr>	SrvReplyList;

	typedef std::list<std::string>	StrList;
	typedef std::map<std::string, SrvReplyList, string_less_no_case> StrReplyMap;
	class SrvReplyMap : private StrReplyMap
	{
	private:
		FastCriticalSection _cs;

		void remove_old(SrvReplyList &replies)
		{
			while (!replies.empty() 
				&& (*replies.begin())->is_too_old())
			{
				replies.pop_front();
			}
		}

	public:
		SrvReplyMap():StrReplyMap()
		{
		}
		~SrvReplyMap()
		{
		}

		void remove_old()
		{	
			AutoCriticalSection cslock(_cs);
			StrList empty_lst;
			for(iterator i=begin();i!=end();++i)
			{
				remove_old(i->second);
				if (i->second.empty())
					empty_lst.push_back(i->first);
			}
			for(StrList::const_iterator j=empty_lst.begin();j!=empty_lst.end();++j)
				erase(*j);
		}

		void add_reply(const std::string &server, const std::string &txt)
		{
			AutoCriticalSection cslock(_cs);
			iterator i = find(server);
			if (i==end())
			{
				insert(value_type(server,SrvReplyList()));
				i = find(server);
			}
			else
			{
				remove_old(i->second);
			}
			i->second.push_back(
				SrvReplyPtr(
				new SrvReply(txt)));
		}

		void last_replies(const std::string &server, std::string &out)
		{
			AutoCriticalSection cslock(_cs);
			iterator i = find(server);
			if (i!=end())
			{
				remove_old(i->second);
				for(SrvReplyList::iterator j=i->second.begin();j!=i->second.end();++j)
				{
					char tmp[128];
					sprintf(tmp,"[%02u:%02u.%02u] ",(*j)->hours(),(*j)->mins(),(*j)->secs());
					out.append(tmp);
					
					std::string txt((*j)->txt());
					if (txt.rfind('\r')==std::string::npos)
						txt.append((size_t)1,'\r');
					if (txt.rfind('\n')==std::string::npos)
						txt.append((size_t)1,'\n');
					out.append(txt);
				}
			}
		}
	};

	SrvReplyMap	srv_reply_map;

	
	void AddReply(const std::wstring &server, const std::string &txt)
	{
		srv_reply_map.add_reply(FtpSettings::WString2String(server), txt);
	}

	void AddError(const std::wstring &server, int err, const char *op_type, const char *op_name, const char *txt)
	{
		//int err = ::WSAGetLastError();
		char *lpMsgBuf = NULL;

		if (!FormatMessageA(
			FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM,
			NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(char *)&lpMsgBuf, 0, NULL))lpMsgBuf = NULL;
		
		if (lpMsgBuf)
		{
			size_t l = strlen(lpMsgBuf);
			while (l && (lpMsgBuf[l-1]=='\r' || lpMsgBuf[l-1]=='\n'))lpMsgBuf[--l]=0;
		}

		AddReply(server,  
			FtpSettings::StrFormat(
			"socket error: '%s' '%s' '%s' '%u - %s'", 
			op_type, op_name, txt, err, 
			lpMsgBuf?lpMsgBuf:"unknown error"));

		if (lpMsgBuf)::LocalFree(lpMsgBuf);
	}

	std::string LastReplies(const std::wstring &server)
	{
		std::string out;
		srv_reply_map.last_replies(FtpSettings::WString2String(server), out);
		return out;
	}

	void RemoveOld()
	{
		srv_reply_map.remove_old();
	}
}
