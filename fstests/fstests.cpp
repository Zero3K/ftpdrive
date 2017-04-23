// fstests.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "windows.h"


int main(int argc, char* argv[])
{
SetLastError(0);
char buf[MAX_PATH]={0};
DWORD len=GetLongPathName("Z:\\killer\\Games\\BroodWar\\patch.txt",buf,MAX_PATH);
printf("GetLongPathName: %u %u '%s'\n",GetLastError(),len,buf);
buf[0]=0;
len=GetShortPathName("Z:\\killer\\Games\\BroodWar\\patch.txt",buf,MAX_PATH);
printf("GetShortPathName: %u %u '%s'\n",GetLastError(),len,buf);
buf[0]=0;
char *fn=NULL;
len=GetFullPathName("Z:\\killer\\Games\\BroodWar\\patch.txt",MAX_PATH,buf,&fn);
printf("GetFullPathName: %u %u '%s' '%s'\n",GetLastError(),len,buf,fn);
DWORD fattr=GetFileAttributes("Z:\\killer\\Games\\BroodWar\\patch.txt");
printf("GetFileAttributes: %u %u\n",GetLastError(),fattr);


HANDLE f=CreateFile("D:\\Program Files\\KillSoft\\zzz.txt",GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
printf("CreateFile: %u %u\n",GetLastError(),f);
  
f=CreateFile("Z:\\killer\\Games\\BroodWar\\patch.txt",GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
printf("CreateFile: %u %u\n",GetLastError(),f);
if(f!=INVALID_HANDLE_VALUE)
 {
 int ret=CloseHandle(f);
 printf("CloseHandle: %u %u\n",GetLastError(),ret);
 }

f=CreateFile("Z:\\killer\\Games\\BroodWar\\patch.txt",GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
printf("CreateFile: %u %u\n",GetLastError(),f);
if(f!=INVALID_HANDLE_VALUE)
 {
 DWORD fsh=0;
 DWORD fsize=GetFileSize(f,&fsh);
 printf("GetFileSize: %u %u:%u\n",GetLastError(),fsize,fsh);

 FILETIME tm1,tm2,tm3;
 int ret=GetFileTime(f,&tm1,&tm2,&tm3);
 printf("GetFileTime: %u %u\n",GetLastError(),ret);
 
 DWORD ftip=GetFileType(f);
 printf("GetFileType: %u %u\n",GetLastError(),ftip);
 
 ret=CloseHandle(f);
 printf("CloseHandle: %u %u\n",GetLastError(),ret);
 }


	return 0;
}

