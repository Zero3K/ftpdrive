#pragma once
#include <string>
namespace SrvReplyLog
{
	void AddReply(const std::wstring &server, const std::string &txt);
	void AddError(const std::wstring &server, int err, const char *op_type, const char *op_name, const char *txt);
	std::string LastReplies(const std::wstring &server);
	void RemoveOld();
}

