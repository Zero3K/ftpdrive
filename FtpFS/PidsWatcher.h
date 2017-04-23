#pragma once
namespace PidsWatcher
 {
 void HandleOpened(DWORD pid,DWORD fs_handle);
 void HandleClosed(DWORD pid,DWORD fs_handle);
 }