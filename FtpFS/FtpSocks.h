#pragma once
#include <boost/shared_ptr.hpp>
#include "TrafficCounter.h"

extern TrafficCounter ftp_traffic_counter;

class IFtpSocket
{
public:
	inline virtual ~IFtpSocket(){};
	virtual bool local_name(unsigned int &ip, unsigned short &port) = 0;
	virtual bool peer_name(unsigned int &ip, unsigned short &port) = 0;

	virtual void set_nolinger(bool nolinger) = 0;

	virtual bool flush() = 0;
	virtual int recv(char *buf, int len) = 0;
	virtual int send(const char *buf, int len=-1) = 0;
	virtual void shutdown() = 0;
	
	virtual int socket() = 0;
	virtual void set_timeout(unsigned int tmout) = 0;
	virtual int timeout() = 0;
	virtual int error() = 0;
};

typedef boost::shared_ptr<IFtpSocket> IFtpSocketPtr;


struct LessIFtpSocketPtr : std::binary_function<IFtpSocketPtr, IFtpSocketPtr, bool>
{
	bool operator()(const IFtpSocketPtr& _X, const IFtpSocketPtr& _Y) const
	{
		return ((ULONG_PTR)_X.get()<(ULONG_PTR)_Y.get());
	}
};

class IFtpTcpSocket : public IFtpSocket
{
public:
	virtual bool connect_to(unsigned int ip, unsigned short port)  = 0;
	virtual bool accept_from(unsigned int &client_ip, unsigned short &client_port)  = 0;
	virtual bool listen_on(unsigned int ip, unsigned short port)  = 0;	
};

typedef boost::shared_ptr<IFtpTcpSocket> IFtpTcpSocketPtr;
//////////////

IFtpTcpSocket *CreateFtpTcpSocket(bool nodelay, bool nolinger, bool largercvbuf, unsigned int tmout);

IFtpSocket *CreateFtpSslSocket(IFtpSocketPtr sock);

void InitOpenSSL();

///////////////

