#pragma once
namespace PidsHandler
 {
 void HandleOpened(DWORD pid,DWORD fs_handle);
 void HandleClosed(DWORD pid,DWORD fs_handle);
 }