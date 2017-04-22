#include "StdAfx.h"
#include "NetControl.h"

NetControl net_control;

NetControl::NetControl()
{
}

NetControl::~NetControl()
{
}

void NetControl::Add(IFtpSocketPtr sock)
{
	AutoCriticalSection cslock(_cs);
	_sockets.insert(sock);
}

void NetControl::Remove(IFtpSocketPtr sock)
{
	AutoCriticalSection cslock(_cs);
	_sockets.erase(sock);
}

void NetControl::KillAll()
{
	AutoCriticalSection cslock(_cs);
	for(IFtpSocketSet::iterator i=_sockets.begin();i!=_sockets.end();++i)
	{
		(*i)->shutdown();
	}
	_last_kill = ::GetTickCount();
}

bool NetControl::WasKilled()
{
	AutoCriticalSection cslock(_cs);
	bool out = (::GetTickCount()<(_last_kill+500));
	return out;
}

bool NetControl::IsEmpty()
{
	AutoCriticalSection cslock(_cs);
	bool out = _sockets.empty();
	return out;
}

