#include "StdAfx.h"
#include "process.h"
#include "TrafficCounter.h"

TrafficCounter::TrafficCounter():
_RecvSize(0),
_SentSize(0),
_in_req_cnt(0),
_flags(0)
{
	_exit_evnt = ::CreateEvent(NULL,TRUE,FALSE,NULL);
	_transer_monitor = NULL;
	unsigned tid;
	_htrd = (HANDLE)_beginthreadex(NULL,0,s_speed_counter_thread,this,0,&tid); 
}

TrafficCounter::~TrafficCounter()
{
	SetEvent(_exit_evnt);
	WaitForSingleObject(_htrd,INFINITE);
	CloseHandle(_htrd);
	CloseHandle(_exit_evnt);
}

void TrafficCounter::speed_counter_thread()
{
	__int64 LastSize = _RecvSize+_SentSize;
	unsigned char wait_boost = 0;
	bool need_notify = false;

	while (::WaitForSingleObject(_exit_evnt,(1000)>>(wait_boost>>3))==WAIT_TIMEOUT)
	{
		AutoCriticalSection cslock(_cs); 
		__int64 CurrentSize = (_RecvSize + _SentSize);
		_CurrentSpeed = (int)((__int64)(CurrentSize - LastSize)<<(wait_boost>>3));

		if (_transer_monitor)
		{
			if (_CurrentSpeed==0 
				&& _flags&TCF_TRANSFER)
			{
				need_notify = true;
				_flags&=~TCF_TRANSFER;
			}
			
			if (_in_req_cnt==0 
				&& _flags&TCF_PROCESS
				&& ((_flags&TCF_TRANSFER)==0))
			{
				need_notify = true;
				_flags&=~TCF_PROCESS;
			}
			
			if (need_notify)
			{
				if (::PostMessage(_transer_monitor,TCM_NOTIFY,0,_flags))
				{
					need_notify = false;
				}
				wait_boost = 16;
			}
		}
		
		if (wait_boost)
			wait_boost--;

		LastSize = CurrentSize;
	}
}

unsigned __stdcall TrafficCounter::s_speed_counter_thread(void *p)
{
	((TrafficCounter *)p)->speed_counter_thread();
	return 0;
}

void TrafficCounter::OnRecv(int size)
{
	AutoCriticalSection cslock(_cs); 
	_RecvSize += ((__int64)size);
	SetNotifyFlag(TCF_TRANSFER);
}

void TrafficCounter::OnSend(int size)
{
	AutoCriticalSection cslock(_cs); 
	_SentSize += ((__int64)size);
	SetNotifyFlag(TCF_TRANSFER);
}

__int64 TrafficCounter::GetRecvSize()
{
	AutoCriticalSection cslock(_cs); 
	__int64 out = _RecvSize;
	return out;
}

__int64 TrafficCounter::GetSentSize()
{
	AutoCriticalSection cslock(_cs); 
	__int64 out = _SentSize;
	return out;
}

int TrafficCounter::GetSpeed()
{
	AutoCriticalSection cslock(_cs); 
	int out = _CurrentSpeed;
	return out;
}

void TrafficCounter::SetNotifyFlag(DWORD flag)
{
	if ((_flags&flag)==0)
	{
		_flags|=flag;
		::PostMessage(_transer_monitor,TCM_NOTIFY,0,_flags);
	}
}

void TrafficCounter::StartRequestProcess()
{
	AutoCriticalSection cslock(_cs); 
	if (!_in_req_cnt++)
		SetNotifyFlag(TCF_PROCESS);
}


void TrafficCounter::EndRequestProcess()
{
	AutoCriticalSection cslock(_cs); 
	_in_req_cnt--;
}

