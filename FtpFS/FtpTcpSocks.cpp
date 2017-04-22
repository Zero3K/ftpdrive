#include "StdAfx.h"
#include <windows.h>
#include <winsock.h>
#include <process.h>
#include <openssl/ssl.h>
#include "FtpSocks.h"
#include "../DebugLog/DebugLog.h"
#include "../FtpSettings/settings.h"

TrafficCounter ftp_traffic_counter;

class FtpTcpSocket : public IFtpTcpSocket
{
private:
	int _sc,_error;
	bool _nodelay, _nolinger, _largercvbuf;
	unsigned int _tmout;
	
	void internal_shutdown(bool nolinger)
	{
		if (_sc!=INVALID_SOCKET)
		{
			int tmp=_sc;
			_sc=INVALID_SOCKET;
			linger lng={1,0};
			if (!nolinger)
			{
				
				if (FtpSettings::ftp_data_timeout>5)
					lng.l_linger = (USHORT)FtpSettings::ftp_data_timeout;
				else
					lng.l_linger = 5;
			}
			::setsockopt(tmp,SOL_SOCKET,SO_LINGER,(char *)&lng,sizeof(lng));
			if (!nolinger)::shutdown(tmp,0x02);//SD_BOTH
			::closesocket(tmp);
		}
	}
	
	void internal_apply_settings()
	{
		if (_largercvbuf)
		{
			DWORD rcvbuf=0x10000;
			::setsockopt(_sc,SOL_SOCKET,SO_RCVBUF,(char *)&rcvbuf,sizeof(rcvbuf));
		}
		if (_nodelay)
		{
			BOOL bEn=TRUE;
			::setsockopt(_sc,IPPROTO_TCP,TCP_NODELAY,(char *)&bEn,sizeof(bEn));
		}
		if (_tmout && _tmout!=0xffffffff)
		{
			int tmout_ms = _tmout*1000;
			::setsockopt(_sc,SOL_SOCKET,SO_RCVTIMEO,(const char*)&tmout_ms,sizeof(tmout_ms));
			::setsockopt(_sc,SOL_SOCKET,SO_SNDTIMEO,(const char*)&tmout_ms,sizeof(tmout_ms));
		}
	}

public:
	FtpTcpSocket(bool nodelay, bool nolinger, bool largercvbuf, unsigned int tmout)
		:_sc(INVALID_SOCKET),_nodelay(nodelay), _nolinger(nolinger),
		_largercvbuf(largercvbuf),_tmout(tmout),_error(0)
	{
	}

	virtual ~FtpTcpSocket()
	{
		internal_shutdown(_nolinger);
	}

	virtual bool connect_to(unsigned int ip, unsigned short port)
	{
		internal_shutdown(_nolinger);

		_sc = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (_sc==INVALID_SOCKET)
		{
			_error = ::WSAGetLastError();
			return false;
		}

		u_long nonblock = 1;
		::ioctlsocket(_sc,FIONBIO,&nonblock);

		sockaddr_in sin={AF_INET,0};
		sin.sin_addr.S_un.S_addr = ip;
		sin.sin_port = ::htons(port);
		if (::connect(_sc,(sockaddr *)&sin, sizeof(sin)))
		{
			if (::WSAGetLastError()!=WSAEWOULDBLOCK)
			{
				_error = ::WSAGetLastError();
				internal_shutdown(true);
				return false;
			}
		}
			
		fd_set fds={1,_sc},fds_ex={1,_sc};
		timeval tv={_tmout,0};
		int r=::select(_sc,NULL,&fds,&fds_ex,&tv);

		if (fds_ex.fd_count)
		{
			int errlen=sizeof(int);
			::getsockopt(_sc,SOL_SOCKET,SO_ERROR,(char *)&_error,&errlen);
			internal_shutdown(true);
			return false;
		}

		if (r<=0)
		{
			_error=(r==0)?WSAETIMEDOUT: ::WSAGetLastError();
			internal_shutdown(true);
			return false;
		}

		

		nonblock = 0;
		::ioctlsocket(_sc,FIONBIO,&nonblock);
		internal_apply_settings();

		return true;
	}

	virtual int error()
	{
		return _error;
	}

	virtual bool listen_on(unsigned int ip, unsigned short port)
	{
		internal_shutdown(_nolinger);
		_sc = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (_sc==INVALID_SOCKET)
		{
			_error = ::WSAGetLastError();
			return false;
		}

		sockaddr_in sin={AF_INET,0};
		sin.sin_addr.S_un.S_addr=ip;
		sin.sin_port=htons(port);

		if(::bind(_sc,(sockaddr *)&sin,sizeof(sin)))
		{
			_error = ::WSAGetLastError();
			internal_shutdown(_nolinger);
			return false;
		}

		if(::listen(_sc,SOMAXCONN))
		{
			_error = ::WSAGetLastError();
			internal_shutdown(_nolinger);
			return false;
		}

		return true;
	}

	virtual bool accept_from(unsigned int &client_ip, unsigned short &client_port)
	{
		for(;;)
		{
			fd_set fds={1,_sc};//,fds_ex={1,_sc};
			timeval tv={_tmout,0};
			int r=::select(_sc,&fds,NULL,NULL,&tv);
			if (r<=0)
			{
				_error=(r==0)?WSAETIMEDOUT: ::WSAGetLastError();
				internal_shutdown(_nolinger);
				return false;
			}
			
			sockaddr_in sin={AF_INET,0};
			int sinlen=sizeof(sin); 
			int client=accept(_sc,(sockaddr *)&sin,&sinlen);
			if (client!=INVALID_SOCKET)
			{
				if((!client_ip)||(client_ip==sin.sin_addr.S_un.S_addr))
				{
					internal_shutdown(_nolinger);
					client_ip = sin.sin_addr.S_un.S_addr;
					client_port = ntohs(sin.sin_port);
					_sc=client;
					internal_apply_settings();
					return true;
				}
				closesocket(client);
			}
		} 
	}

	virtual int recv(char *buf, int len)
	{
		int out = ::recv(_sc,buf,len,0);
		if(out>0)
			ftp_traffic_counter.OnRecv(out);
		else
			_error=(out==0)?WSAEDISCON: ::WSAGetLastError();
		return out;
	}

	virtual int send(const char *buf, int len)
	{
		if(len==-1)len=strlen(buf);
		int out = ::send(_sc,buf,len,0);
		if(out>0)
			ftp_traffic_counter.OnSend(out);
		else
			_error=(out==0)?WSAEDISCON: ::WSAGetLastError();

		return out;
	}

	virtual void shutdown()
	{
		internal_shutdown(_nolinger);
	}

	virtual bool local_name(unsigned int &ip, unsigned short &port)
	{
		if (_sc==INVALID_SOCKET)
			return false;

		sockaddr_in sin={AF_INET,0};
		int sinlen=sizeof(sin);
		if (::getsockname(_sc,(sockaddr *)&sin,&sinlen)!=0)
			return false;

		ip = sin.sin_addr.S_un.S_addr;
		port = ::ntohs(sin.sin_port);
		return true;
	}

	virtual bool peer_name(unsigned int &ip, unsigned short &port)
	{
		if (_sc==INVALID_SOCKET)
			return false;

		sockaddr_in sin={AF_INET,0};
		int sinlen=sizeof(sin);
		if (::getpeername(_sc,(sockaddr *)&sin,&sinlen)!=0)
			return false;

		ip = sin.sin_addr.S_un.S_addr;
		port = ::ntohs(sin.sin_port);
		return true;
	}

	virtual void set_nolinger(bool nolinger)
	{
		_nolinger = nolinger;
	}

	virtual bool flush()
	{
		if (_sc==INVALID_SOCKET)
			return false;

		fd_set fds={1,_sc};
		timeval tv={0,0};
		if (!::select(0,NULL,&fds,NULL,&tv))
		{
			_error = WSAETIMEDOUT;
			internal_shutdown(true);
			return false;
		}

		for(;;)
		{
			fds.fd_count=1;fds.fd_array[0]=_sc;
			fd_set fds_ex={1,_sc};
			int r = ::select(0,&fds,NULL,&fds_ex,&tv);
			if (r<0)
			{
				_error = ::WSAGetLastError();
				internal_shutdown(true);
				return false;
			}

			if (fds_ex.fd_count)
			{
				int errlen=sizeof(int);
				::getsockopt(_sc,SOL_SOCKET,SO_ERROR,(char *)&_error,&errlen);
				internal_shutdown(true);
				return false;
			}

			if (r==0)
				return true;
		
			char tmp[256];
			r = ::recv(_sc,&tmp[0],256,0);
			if (r<=0)
			{
				int err=::WSAGetLastError();
				if (err==WSAETIMEDOUT)
					return true;

				_error = r==0?WSAEDISCON: err;
				internal_shutdown(true);
				return false;
			}
		}
		
	}

	virtual int socket()
	{
		return _sc;
	}
	virtual int timeout() 
	{
		return _tmout;
	}
	virtual void set_timeout(unsigned int tmout)
	{
		_tmout = tmout;
		internal_apply_settings();
	}
};



IFtpTcpSocket *CreateFtpTcpSocket(bool nodelay, bool nolinger, bool largercvbuf, unsigned int tmout)
{
	return (IFtpTcpSocket *)new FtpTcpSocket(nodelay, nolinger, largercvbuf, tmout);
}


//////////////////////////////////////////////////////////////////////////////////////////////////
typedef void (__cdecl *tSSL_load_error_strings)();
typedef int (__cdecl *tSSL_library_init)();

typedef SSL_CTX *(__cdecl *tSSL_CTX_new)(SSL_METHOD *meth);
typedef void	(__cdecl *tSSL_CTX_free)(SSL_CTX *);
typedef long (__cdecl *tSSL_CTX_set_timeout)(SSL_CTX *ctx,long t);
typedef long (__cdecl *tSSL_CTX_get_timeout)(SSL_CTX *ctx);

typedef SSL_METHOD *(__cdecl *tTLSv1_client_method)(void);	/* TLSv1.0 */
typedef SSL_METHOD *(__cdecl *tSSLv3_client_method)(void);	/* SSLv3 */
typedef SSL *(__cdecl *tSSL_new)(SSL_CTX *ctx);
typedef void(__cdecl *tSSL_free)(SSL *ssl);
typedef int (__cdecl *tSSL_connect)(SSL *ssl);
typedef int (__cdecl *tSSL_read)(SSL *ssl,void *buf,int num);
typedef int (__cdecl *tSSL_write)(SSL *ssl,const void *buf,int num);
typedef long(__cdecl *tSSL_ctrl)(SSL *ssl,int cmd, long larg, void *parg);
typedef int	(__cdecl *tSSL_set_fd)(SSL *s, int fd);
typedef int	(__cdecl *tSSL_shutdown)(SSL *s);

tSSL_load_error_strings		pSSL_load_error_strings = NULL;
tSSL_library_init			pSSL_library_init = NULL;

tSSL_CTX_new pSSL_CTX_new= NULL;
tSSL_CTX_free pSSL_CTX_free= NULL;
tSSL_CTX_set_timeout pSSL_CTX_set_timeout= NULL;
tSSL_CTX_get_timeout pSSL_CTX_get_timeout= NULL;

tTLSv1_client_method pTLSv1_client_method= NULL;
tSSLv3_client_method pSSLv3_client_method= NULL;
tSSL_new pSSL_new= NULL;
tSSL_free pSSL_free= NULL;
tSSL_connect pSSL_connect= NULL;
tSSL_read pSSL_read= NULL;
tSSL_write pSSL_write= NULL;
tSSL_ctrl pSSL_ctrl = NULL;
tSSL_set_fd pSSL_set_fd = NULL;
tSSL_shutdown pSSL_shutdown = NULL;

#define GET_PROC_ADDR_FAILRET(dll,name) \
{\
p##name = (t##name)::GetProcAddress(dll,#name);\
if(!p##name)return;\
}

bool g_have_openssl = false;

void InitOpenSSL()
{
	HMODULE dll=::LoadLibrary(TEXT("ssleay32.dll"));
	if(!dll)
		return;
	
	GET_PROC_ADDR_FAILRET(dll,SSL_load_error_strings);
	
	GET_PROC_ADDR_FAILRET(dll,SSL_library_init);
	
	GET_PROC_ADDR_FAILRET(dll,SSL_CTX_new);
	GET_PROC_ADDR_FAILRET(dll,SSL_CTX_free);
	GET_PROC_ADDR_FAILRET(dll,SSL_CTX_set_timeout);
	GET_PROC_ADDR_FAILRET(dll,SSL_CTX_get_timeout);
	
	GET_PROC_ADDR_FAILRET(dll,TLSv1_client_method);
	GET_PROC_ADDR_FAILRET(dll,SSLv3_client_method);
	GET_PROC_ADDR_FAILRET(dll,SSL_new);
	GET_PROC_ADDR_FAILRET(dll,SSL_free);
	GET_PROC_ADDR_FAILRET(dll,SSL_connect);
	GET_PROC_ADDR_FAILRET(dll,SSL_read);
	GET_PROC_ADDR_FAILRET(dll,SSL_write);
	GET_PROC_ADDR_FAILRET(dll,SSL_ctrl);
	GET_PROC_ADDR_FAILRET(dll,SSL_set_fd);
	GET_PROC_ADDR_FAILRET(dll,SSL_shutdown);
	
	pSSL_load_error_strings();
	pSSL_library_init();
	g_have_openssl = true;
}




class FtpSslSocket : public IFtpSocket
{
private:
	int _error;
	IFtpSocketPtr	_sock;
	SSL_CTX*		_ctx;
	SSL*			_ssl;

	void internal_shutdown()
	{
		if (_ssl)
		{
			pSSL_shutdown(_ssl);
			pSSL_free(_ssl);
			_ssl = NULL;
		}

		if (_ctx)
		{
			pSSL_CTX_free(_ctx);
			_ctx = NULL;
		}

		if (_sock)
		{
			_sock.reset();
		}
	}

public:
	FtpSslSocket(IFtpSocketPtr sock)
		:_sock(sock),_error(0)
	{
		if(!g_have_openssl)
		{
			_ctx=NULL;
			_ssl=NULL;
			sock.reset();
			MyWriteDebugLog("Want SSL but OpenSSL not installed");
			_error = WSAEPROTONOSUPPORT;
			return;
		}

		_ctx = pSSL_CTX_new(pSSLv3_client_method());
		if(_ctx)
		{
			_ssl = pSSL_new(_ctx);
		}
		else
		{
			_ssl = NULL;
		}

		if (_ssl)
		{
			//pSSL_ctrl(_ssl,SSL_CTRL_MODE,SSL_MODE_AUTO_RETRY,NULL);
			if (pSSL_set_fd(_ssl, sock->socket())==1)
			{
				//timeout()
				if (pSSL_connect(_ssl)==1)
				{
					//SSL_do_handshake(_ssl);
					//MessageBoxA(0,"ssl connect ok","zz",0);
				}
				else
				{
					_error = WSAEPROTOTYPE;
					internal_shutdown();
					//MessageBoxA(0,"ssl connect failed","zz",0);
				}
			}
			else
			{
				_error = WSAENETDOWN;
				internal_shutdown();
			}
		}
		else
		{
			internal_shutdown();
		}
	}

	virtual ~FtpSslSocket()
	{
		internal_shutdown();
	}

	virtual bool local_name(unsigned int &ip, unsigned short &port)
	{
		if(!_sock)
			return false;

		return _sock->local_name(ip, port);
	}
	
	virtual bool peer_name(unsigned int &ip, unsigned short &port)
	{
		if(!_sock)
			return false;

		return _sock->peer_name(ip, port);
	}

	virtual void set_nolinger(bool nolinger)
	{
		if(_sock)
			_sock->set_nolinger(nolinger);
	}

	virtual bool flush()
	{
		return true;//todo
	}

	virtual int recv(char *buf, int len)
	{		
		if (!_ssl)
			return -1;

		//SocketTimerTrigger stt(this);
		
		   //MessageBoxA(0,&tmp[0],"zz1",0);

		int out = pSSL_read(_ssl, buf, len);
		if (out>0)
			ftp_traffic_counter.OnRecv(out);
		else
			_error = (out==0)?WSAEDISCON: ::WSAGetLastError();

		return out;
	}

	virtual int send(const char *buf, int len)
	{
		if (!_ssl)
			return -1;

		if(len==-1)len=strlen(buf);

		//SocketTimerTrigger stt(this);

		int out = pSSL_write(_ssl, buf, len);
		if (out>0)
			ftp_traffic_counter.OnSend(out);
		else
			_error = (out==0)?WSAEDISCON: ::WSAGetLastError();
		return out;
	}

	virtual void shutdown()
	{
		internal_shutdown();
	}

	virtual int socket()
	{
		if(!_sock)
			return -1;

		return _sock->socket();
	}

	virtual int timeout()
	{
		if(!_sock)
			return -1;

		return _sock->timeout();
	}

	virtual void set_timeout(unsigned int tmout)
	{
		_sock->set_timeout(tmout);
	}

	virtual int error()
	{
		return _error;
	}
};


IFtpSocket *CreateFtpSslSocket(IFtpSocketPtr sock)
{
	return (IFtpSocket *)new FtpSslSocket(sock);
}

