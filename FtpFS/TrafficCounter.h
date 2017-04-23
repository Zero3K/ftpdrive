#pragma once
#include "../FtpFS/ntdefs.h"
#include "../APIHooks/ntapi.h"

#define TCM_NOTIFY	(WM_USER+1213)
#define TCF_TRANSFER	0x01
#define TCF_PROCESS		0x02

class TrafficCounter
 {
 private:
  __int64 _RecvSize;
  __int64 _SentSize;
  int _CurrentSpeed;
  FastCriticalSection _cs;
  HANDLE _exit_evnt;  
  HANDLE _htrd;
  HWND _transer_monitor;
  int _in_req_cnt;
  DWORD _flags;
  
  void SetNotifyFlag(DWORD flag);

  void speed_counter_thread();  
  static unsigned __stdcall s_speed_counter_thread(void *p);  
 public:
  TrafficCounter();
  ~TrafficCounter();
  void OnRecv(int size);
  void OnSend(int size);
  __int64 GetRecvSize();
  __int64 GetSentSize();
  int GetSpeed();  
  void StartRequestProcess();
  void EndRequestProcess();
  inline void SetTransferMonitor(HWND hwnd){_transer_monitor = hwnd;}
 };


class RequestProcessIndicator
{
private:
	TrafficCounter &_traffic_counter;

public:
	inline RequestProcessIndicator(TrafficCounter &traffic_counter):
		_traffic_counter(traffic_counter)
	{
		_traffic_counter.StartRequestProcess();
	}
	inline ~RequestProcessIndicator()
	{
		_traffic_counter.EndRequestProcess();
	}
};
