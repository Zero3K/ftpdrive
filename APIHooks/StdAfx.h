// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#if !defined(AFX_STDAFX_H__316DA379_DB5F_4627_B545_12F2ACF1856A__INCLUDED_)
#define AFX_STDAFX_H__316DA379_DB5F_4627_B545_12F2ACF1856A__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000


// Insert your headers here
#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

#include <windows.h>
#include <MAP>
#include <VECTOR>
#include <STRING>

void CheckForDriveChanged();
extern DWORD g_my_pid;
extern HINSTANCE g_my_hmod;
// TODO: reference additional headers your program requires here

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STDAFX_H__316DA379_DB5F_4627_B545_12F2ACF1856A__INCLUDED_)
