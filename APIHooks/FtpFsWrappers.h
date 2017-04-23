#ifndef _FTPFSWRAPPERS_H_
#define _FTPFSWRAPPERS_H_


namespace FtpFsWrappers
 { 
 void  Init();
 bool EnterSkipLock();
 void LeaveSkipLock();
 int  FindFsHandle(HANDLE handle);
 HANDLE  CreateWrapperHandle(int fs_handle,HANDLE existing_handle = NULL);
 void CloseFSHandle(unsigned int fs_handle);
 bool  CloseWrapperHandle(HANDLE handle);
 bool  IncRefWrapperHandle(HANDLE handle);
 void  SetHandlePos(HANDLE handle,__int64 pos);
 __int64  GetHandlePos(HANDLE handle);
 void  SetDirMask(HANDLE handle,const std::wstring &dirmask);
 std::wstring  GetDirMask(HANDLE handle);
 std::wstring GetSessionObjectName(PWCHAR name);
 // 
 bool  SendRecvFS(void *sendbuf,int sendlen, void *recvbuf,int recvlen);
 bool  RecvFS(void *buf,int buflen);
 bool  SendFS(void *buf,unsigned int buflen);
 //
 bool ProcessAttach();
 void ProcessDetach();
 void ThreadAttach();
 void  ThreadDetach();
 //

 }

#endif// _FTPFSWRAPPERS_H_

