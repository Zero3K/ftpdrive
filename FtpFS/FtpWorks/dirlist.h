#pragma once
#include "../UNCSupport.h"

namespace FtpWorks
 {
 DWORD ParseDirList(
	 LPSTR lpBuffer,
	 DWORD dwBufferLength,
	 FindVector &found,
	 CPConv *conv);
 }