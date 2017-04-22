#include "stdafx.h"
#include "UNCSupport.h"
#include "../FtpSettings/settings.h"

class WINConv : public CPConv
{
public:
	WINConv(){};
	virtual std::string Encode(const std::wstring &s)
	{
		return FtpSettings::WString2String(s);
	}

	virtual std::wstring Decode(const std::string &s)
	{
		return FtpSettings::String2WString(s);
	}
	
	static WINConv conv;
};

WINConv WINConv::conv;
CPConv *GetWINConv(){return (CPConv *)&WINConv::conv;}
//////////////////////////////////////////

class ANYConv : public CPConv
{
private:
	UINT _cp;
public:
	ANYConv(UINT cp):_cp(cp){};
	virtual std::string Encode(const std::wstring &s)
	{
		std::string out;
		out.resize(16+(s.size()*4));
		int r=::WideCharToMultiByte(_cp,0,s.c_str(),s.size(),out.begin(),out.size(),NULL,NULL);
		out.resize(r);
		return out;
	}

	virtual std::wstring Decode(const std::string &s)
	{
		std::wstring out;
		out.resize(s.size());
		int r=::MultiByteToWideChar(_cp,0,s.c_str(),s.size(),out.begin(),out.size());
		out.resize(r);
		return out;
	}

};

ANYConv dos_conv(CP_OEMCP);
ANYConv utf8_conv(CP_UTF8);
ANYConv koi8r_conv(20866);
ANYConv koi8u_conv(21866);

CPConv *GetDOSConv(){return (CPConv *)&dos_conv;}
CPConv *GetUTF8Conv(){return (CPConv *)&utf8_conv;}
CPConv *GetKOI8RConv(){return (CPConv *)&koi8r_conv;}
CPConv *GetKOI8UConv(){return (CPConv *)&koi8u_conv;}
//////////////////////////////////////////
