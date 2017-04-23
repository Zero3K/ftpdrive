#pragma once
#include <string>

class CPConv
{
public:
	virtual std::string Encode(const std::wstring &s) = 0;
	virtual std::wstring Decode(const std::string &s) = 0;
};

CPConv *GetWINConv();
CPConv *GetDOSConv();
CPConv *GetUTF8Conv();
CPConv *GetKOI8RConv();
CPConv *GetKOI8UConv();
