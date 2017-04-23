# Microsoft Developer Studio Project File - Name="FtpFS" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=FtpFS - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "FtpFS.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "FtpFS.mak" CFG="FtpFS - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "FtpFS - Win32 Release" (based on "Win32 (x86) Application")
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
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /Yu"stdafx.h" /FD /c
# ADD CPP /nologo /Gr /MD /W3 /GX /O2 /X /I "$(STL)" /I "$(BOOST)" /I "$(MSSDK)/include" /I "$(MSSDK)/include/crt" /I "$(OPENSSL)/include" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D _WIN32_WINNT=0x0500 /D "UNICODE" /Yu"stdafx.h" /FD /G7 /Qipo /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x419 /d "NDEBUG"
# ADD RSC /l 0x419 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 ntdll.lib comctl32.lib ../Build\Release/DebugLog.lib ../Build/Release/FtpSettings.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ws2_32.lib /nologo /subsystem:windows /pdb:"../Build/Release/FtpFS.pdb" /machine:I386 /out:"../Build/Release/FtpDrive.exe" /libpath:"$(DDKROOT)/lib/crt/i386" /libpath:"$(DDKROOT)\lib\w2k\i386"
# SUBTRACT LINK32 /pdb:none /map /debug
# Begin Target

# Name "FtpFS - Win32 Release"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\ErrorHandler.cpp
# End Source File
# Begin Source File

SOURCE=.\FtpFS.cpp
# End Source File
# Begin Source File

SOURCE=.\FtpFsImpl.cpp
# End Source File
# Begin Source File

SOURCE=.\FtpTcpSocks.cpp
# End Source File
# Begin Source File

SOURCE=.\NetControl.cpp
# End Source File
# Begin Source File

SOURCE=.\PidsWatcher.cpp
# End Source File
# Begin Source File

SOURCE=.\PipeServer.cpp
# End Source File
# Begin Source File

SOURCE=.\ResHolder.cpp
# End Source File
# Begin Source File

SOURCE=.\SrvReplyLog.cpp
# End Source File
# Begin Source File

SOURCE=.\StdAfx.cpp
# ADD CPP /Yc"stdafx.h"
# End Source File
# Begin Source File

SOURCE=.\TrafficCounter.cpp
# End Source File
# Begin Source File

SOURCE=.\UNCSupport.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\ntdefs.h
# End Source File
# Begin Source File

SOURCE=.\FtpWorks\dirlist.h
# End Source File
# Begin Source File

SOURCE=.\ErrorHandler.h
# End Source File
# Begin Source File

SOURCE=.\FtpFS.h
# End Source File
# Begin Source File

SOURCE=.\FtpSocks.h
# End Source File
# Begin Source File

SOURCE=.\FtpWorks\FtpWorks.h
# End Source File
# Begin Source File

SOURCE=.\NetControl.h
# End Source File
# Begin Source File

SOURCE=.\PidsWatcher.h
# End Source File
# Begin Source File

SOURCE=.\ResHolder.h
# End Source File
# Begin Source File

SOURCE=.\UI\resource.h
# End Source File
# Begin Source File

SOURCE=.\SrvReplyLog.h
# End Source File
# Begin Source File

SOURCE=.\StdAfx.h
# End Source File
# Begin Source File

SOURCE=.\TrafficCounter.h
# End Source File
# Begin Source File

SOURCE=.\UNCSupport.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\UI\FTP.ico
# End Source File
# Begin Source File

SOURCE=.\UI\icon2.ico
# End Source File
# Begin Source File

SOURCE=.\UI\icon3.ico
# End Source File
# Begin Source File

SOURCE=.\UI\Script1.rc
# End Source File
# End Group
# Begin Group "FtpWorks"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\FtpWorks\dataapiwrappers.cpp
# End Source File
# Begin Source File

SOURCE=.\FtpWorks\datacache.cpp
# End Source File
# Begin Source File

SOURCE=.\FtpWorks\dirlist.cpp
# End Source File
# Begin Source File

SOURCE=.\FtpWorks\files.cpp
# End Source File
# Begin Source File

SOURCE=.\FtpWorks\handles.cpp
# End Source File
# End Group
# Begin Group "UI"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\UI\Dialogs.cpp
# End Source File
# Begin Source File

SOURCE=.\UI\UI.cpp
# End Source File
# End Group
# Begin Source File

SOURCE=.\FtpFS.txt
# End Source File
# Begin Source File

SOURCE=.\ReadMe.txt
# End Source File
# Begin Source File

SOURCE=.\UI\XpXML.txt
# End Source File
# End Target
# End Project
