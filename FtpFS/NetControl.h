#pragma once
#include "FtpSocks.h"
#include <set>
#include "../FtpFS/ntdefs.h"
#include "../APIHooks/ntapi.h"

class NetControl
{
private:
	DWORD _last_kill;
	FastCriticalSection _cs;

	typedef std::set<IFtpSocketPtr, LessIFtpSocketPtr> IFtpSocketSet;
	IFtpSocketSet	_sockets;

public:
	NetControl();
	virtual ~NetControl();

	void Add(IFtpSocketPtr sock);
	void Remove(IFtpSocketPtr sock);
	void KillAll();

	bool WasKilled();
	bool IsEmpty();
};

extern NetControl net_control;


class AutoNetControl
{
private:
	NetControl &	_net_control;
	IFtpSocketPtr	_sock;


public:
	inline AutoNetControl(NetControl &net_control,IFtpSocketPtr sock)
		:_net_control(net_control),_sock(sock)
	{
		_net_control.Add(_sock);
	}
	
	inline ~AutoNetControl()
	{
		_net_control.Remove(_sock);
	}
};

