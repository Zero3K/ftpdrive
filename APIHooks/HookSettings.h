#pragma once
extern volatile char FtpFsDrive[2];
namespace HookSettings
 {
 bool IsOurRootDrive(const wchar_t *path,int pathlen = -1);
 bool IsOurRootDrive(POBJECT_ATTRIBUTES ObjectAttributes);
 bool IsOurRootDrive(const char *path);
 
 bool IsThisAppAffected();//always uses ipc - slow
 bool IsThisAffected(POBJECT_ATTRIBUTES ObjectAttributes);
 }

 