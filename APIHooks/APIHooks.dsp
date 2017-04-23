# Microsoft Developer Studio Project File - Name="APIHooks" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=APIHooks - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "APIHooks.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "APIHooks.mak" CFG="APIHooks - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "APIHooks - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe
# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "APIHOOKS_EXPORTS" /Yu"stdafx.h" /FD /c
# ADD CPP /nologo /Gr /MD /W3 /GX /O2 /X /I "$(STL)" /I "$(MSSDK)/include" /I "$(MSSDK)/include/crt" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "APIHOOKS_EXPORTS" /D _WIN32_WINNT=0x0500 /D "UNICODE" /Yu"stdafx.h" /FD /G7 /Qipo /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x419 /d "NDEBUG"
# ADD RSC /l 0x419 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 ntdll.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib mpr.lib /nologo /dll /pdb:"../Build/Release/APIHooks.pdb" /machine:I386 /out:"..\Build\Release\FtpDrive.dll" /libpath:"$(DDKROOT)/lib/crt/i386" /libpath:"$(DDKROOT)\lib\w2k\i386"
# SUBTRACT LINK32 /pdb:none
# Begin Target

# Name "APIHooks - Win32 Release"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\APIHooks.cpp
# End Source File
# Begin Source File

SOURCE=.\FileCopy.cpp
# End Source File
# Begin Source File

SOURCE=.\FileDataNt.cpp
# End Source File
# Begin Source File

SOURCE=.\FileInfoNt.cpp
# End Source File
# Begin Source File

SOURCE=.\FtpFsWrappers.cpp
# End Source File
# Begin Source File

SOURCE=.\GetProcAddrNt.cpp
# End Source File
# Begin Source File

SOURCE=.\HighLevel.cpp
# End Source File
# Begin Source File

SOURCE=.\HookImpl.cpp
# End Source File
# Begin Source File

SOURCE=.\HookSettings.cpp
# End Source File
# Begin Source File

SOURCE=.\SectionsNt.cpp
# End Source File
# Begin Source File

SOURCE=.\StdAfx.cpp
# ADD CPP /Yc"stdafx.h"
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\FtpFsWrappers.h
# End Source File
# Begin Source File

SOURCE=.\HookImpl.h
# End Source File
# Begin Source File

SOURCE=.\HookSettings.h
# End Source File
# Begin Source File

SOURCE=.\ntapi.h
# End Source File
# Begin Source File

SOURCE=.\StdAfx.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# Begin Group "Detours"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\detours\src\creatwth.cpp
# End Source File
# Begin Source File

SOURCE=.\detours\src\detours.cpp
# End Source File
# Begin Source File

SOURCE=.\detours\src\detours.h
# End Source File
# Begin Source File

SOURCE=.\detours\src\disasm.cpp
# End Source File
# Begin Source File

SOURCE=.\detours\src\disasm.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\ReadMe.txt
# End Source File
# End Target
# End Project
