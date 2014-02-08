# Microsoft Developer Studio Project File - Name="WinHTTrack" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=WinHTTrack - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "WinHTTrack.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "WinHTTrack.mak" CFG="WinHTTrack - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "WinHTTrack - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "WinHTTrack - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE "WinHTTrack - Win32 Debug release" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "WinHTTrack - Win32 Release"

# PROP BASE Use_MFC 5
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 6
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "c:\temp\VCRelease"
# PROP Intermediate_Dir "c:\temp\VCRelease"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /Yu"stdafx.h" /FD /c
# ADD CPP /nologo /MD /W3 /Gi /GX /O2 /Op /Ob2 /I "C:\Dev\IPv6Kit\inc\\" /I "..\\" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_AFXDLL" /FD /Zm200 /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x40c /d "NDEBUG"
# ADD RSC /l 0x40c /d "NDEBUG" /d "_AFXDLL"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:windows /machine:I386
# ADD LINK32 wsock32.lib libhttrack.lib /nologo /subsystem:windows /machine:I386 /out:"O:\HTTrack\httrack\WinHTTrack.exe" /libpath:"C:\Dev\openssl\lib" /libpath:"C:\Dev\zlib\dll32" /libpath:"C:\Dev\openssl\lib\out32dll" /libpath:"C:\temp\Releaselib"

!ELSEIF  "$(CFG)" == "WinHTTrack - Win32 Debug"

# PROP BASE Use_MFC 5
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 6
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "c:\temp\VCDebug"
# PROP Intermediate_Dir "c:\temp\VCDebug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /Yu"stdafx.h" /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /Gi /GX /ZI /Od /I "C:\Dev\IPv6Kit\inc\\" /I "..\\" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_AFXDLL" /FAs /FR /FD /GZ /Zm200 /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x40c /d "_DEBUG"
# ADD RSC /l 0x40c /d "_DEBUG" /d "_AFXDLL"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 wsock32.lib libhttrack.lib /nologo /subsystem:windows /map /debug /debugtype:both /machine:I386 /out:"c:\temp\WinHTTrack.exe" /pdbtype:sept /libpath:"C:\Dev\openssl\lib" /libpath:"C:\Dev\zlib\dll32" /libpath:"C:\Dev\openssl\lib\out32dll" /libpath:"C:\temp\Debuglib"
# SUBTRACT LINK32 /nodefaultlib

!ELSEIF  "$(CFG)" == "WinHTTrack - Win32 Debug release"

# PROP BASE Use_MFC 5
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "WinHTTrack___Win32_Debug_release"
# PROP BASE Intermediate_Dir "WinHTTrack___Win32_Debug_release"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 6
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "c:\temp\VCDebug"
# PROP Intermediate_Dir "c:\temp\VCDebug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /FR /FD /c
# SUBTRACT BASE CPP /YX /Yc /Yu
# ADD CPP /nologo /MD /W3 /GR /GX /Zi /Od /I "C:\Dev\IPv6Kit\inc\\" /I "..\\" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_AFXDLL" /FAcs /FR /FD /Zm200 /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /I "C:\Dev\IPv6Kit\inc" /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x40c /d "NDEBUG"
# ADD RSC /l 0x40c /d "NDEBUG" /d "_AFXDLL"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 wsock32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 wsock32.lib libhttrack.lib /nologo /subsystem:windows /profile /map /debug /debugtype:both /machine:I386 /libpath:"C:\Dev\openssl\lib" /libpath:"C:\Dev\zlib\dll32" /libpath:"C:\Dev\openssl\lib\out32dll"

!ENDIF 

# Begin Target

# Name "WinHTTrack - Win32 Release"
# Name "WinHTTrack - Win32 Debug"
# Name "WinHTTrack - Win32 Debug release"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\about.cpp
# End Source File
# Begin Source File

SOURCE=.\AddFilter.cpp
# End Source File
# Begin Source File

SOURCE=.\BatchUpdate.cpp
# End Source File
# Begin Source File

SOURCE=.\BuildOptions.cpp
# End Source File
# Begin Source File

SOURCE=.\CatchUrl.cpp
# End Source File
# Begin Source File

SOURCE=.\DialogContainer.cpp
# End Source File
# Begin Source File

SOURCE=.\DialogHtmlHelp.cpp
# End Source File
# Begin Source File

SOURCE=.\DirTreeView.cpp
# End Source File
# Begin Source File

SOURCE=.\EasyDropTarget.cpp
# End Source File
# Begin Source File

SOURCE=.\FirstInfo.cpp
# End Source File
# Begin Source File

SOURCE=.\HtmlCtrl.cpp
# End Source File
# Begin Source File

SOURCE=.\HtmlFrm.cpp
# End Source File
# Begin Source File

SOURCE=.\HTMLHelp.cpp
# End Source File
# Begin Source File

SOURCE=..\htsinthash.c
# End Source File
# Begin Source File

SOURCE=..\htsinthash.h
# End Source File
# Begin Source File

SOURCE=..\htsmd5.c
# End Source File
# Begin Source File

SOURCE=.\HTTrackInterface.c
# End Source File
# Begin Source File

SOURCE=.\Infoend.cpp
# End Source File
# Begin Source File

SOURCE=.\InfoUrl.cpp
# End Source File
# Begin Source File

SOURCE=.\inprogress.cpp
# End Source File
# Begin Source File

SOURCE=.\InsertUrl.cpp
# End Source File
# Begin Source File

SOURCE=.\Iplog.cpp
# End Source File
# Begin Source File

SOURCE=.\LaunchHelp.cpp
# End Source File
# Begin Source File

SOURCE=.\MainFrm.cpp
# End Source File
# Begin Source File

SOURCE=.\MainTab.cpp
# End Source File
# Begin Source File

SOURCE=..\md5.c
# End Source File
# Begin Source File

SOURCE=..\md5.h
# End Source File
# Begin Source File

SOURCE=.\MemRegister.cpp
# End Source File
# Begin Source File

SOURCE=.\NewFolder.cpp
# End Source File
# Begin Source File

SOURCE=.\newlang.cpp
# End Source File
# Begin Source File

SOURCE=.\NewProj.cpp
# End Source File
# Begin Source File

SOURCE=.\OptionTab1.cpp
# End Source File
# Begin Source File

SOURCE=.\OptionTab10.cpp
# End Source File
# Begin Source File

SOURCE=.\OptionTab11.cpp
# End Source File
# Begin Source File

SOURCE=.\OptionTab2.cpp
# End Source File
# Begin Source File

SOURCE=.\OptionTab3.cpp
# End Source File
# Begin Source File

SOURCE=.\OptionTab4.cpp
# End Source File
# Begin Source File

SOURCE=.\OptionTab5.cpp
# End Source File
# Begin Source File

SOURCE=.\OptionTab6.cpp
# End Source File
# Begin Source File

SOURCE=.\OptionTab7.cpp
# End Source File
# Begin Source File

SOURCE=.\OptionTab8.cpp
# End Source File
# Begin Source File

SOURCE=.\OptionTab9.cpp
# End Source File
# Begin Source File

SOURCE=.\ProxyId.cpp
# End Source File
# Begin Source File

SOURCE=.\RasLoad.cpp
# End Source File
# Begin Source File

SOURCE=.\Shell.cpp
# End Source File
# Begin Source File

SOURCE=.\splitter.cpp
# End Source File
# Begin Source File

SOURCE=.\StdAfx.cpp
# ADD CPP /Yc"stdafx.h"
# End Source File
# Begin Source File

SOURCE=.\trans.cpp
# End Source File
# Begin Source File

SOURCE=.\TreeViewToolTip.cpp
# End Source File
# Begin Source File

SOURCE=.\Wid1.cpp
# End Source File
# Begin Source File

SOURCE=.\WinHTTrack.cpp
# End Source File
# Begin Source File

SOURCE=.\WinHTTrack.rc
# End Source File
# Begin Source File

SOURCE=.\WinHTTrackDoc.cpp
# End Source File
# Begin Source File

SOURCE=.\WinHTTrackView.cpp
# End Source File
# Begin Source File

SOURCE=.\Wizard.cpp
# End Source File
# Begin Source File

SOURCE=.\Wizard2.cpp
# End Source File
# Begin Source File

SOURCE=.\WizLinks.cpp
# End Source File
# Begin Source File

SOURCE=.\WizTab.cpp
# End Source File
# Begin Source File

SOURCE=.\XSHBrowseForFolder.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\about.h
# End Source File
# Begin Source File

SOURCE=.\AddFilter.h
# End Source File
# Begin Source File

SOURCE=.\BatchUpdate.h
# End Source File
# Begin Source File

SOURCE=.\BuildOptions.h
# End Source File
# Begin Source File

SOURCE=.\CatchUrl.h
# End Source File
# Begin Source File

SOURCE=.\DialogContainer.h
# End Source File
# Begin Source File

SOURCE=.\DialogHtmlHelp.h
# End Source File
# Begin Source File

SOURCE=.\DirTreeView.h
# End Source File
# Begin Source File

SOURCE=.\EasyDropTarget.h
# End Source File
# Begin Source File

SOURCE=.\ESSAI.H
# End Source File
# Begin Source File

SOURCE=.\FirstInfo.h
# End Source File
# Begin Source File

SOURCE=.\GET.H
# End Source File
# Begin Source File

SOURCE=.\HtmlCtrl.h
# End Source File
# Begin Source File

SOURCE=.\HtmlFrm.h
# End Source File
# Begin Source File

SOURCE=.\HTMLHelp.h
# End Source File
# Begin Source File

SOURCE=.\infoend.h
# End Source File
# Begin Source File

SOURCE=.\InfoUrl.h
# End Source File
# Begin Source File

SOURCE=.\inprogress.h
# End Source File
# Begin Source File

SOURCE=.\InsertUrl.h
# End Source File
# Begin Source File

SOURCE=.\Iplog.h
# End Source File
# Begin Source File

SOURCE=.\LaunchHelp.h
# End Source File
# Begin Source File

SOURCE=.\MainFrm.h
# End Source File
# Begin Source File

SOURCE=.\MainTab.h
# End Source File
# Begin Source File

SOURCE=.\MD5.H
# End Source File
# Begin Source File

SOURCE=.\MemRegister.h
# End Source File
# Begin Source File

SOURCE=.\NewFolder.h
# End Source File
# Begin Source File

SOURCE=.\newlang.h
# End Source File
# Begin Source File

SOURCE=.\NewProj.h
# End Source File
# Begin Source File

SOURCE=.\OptionTab1.h
# End Source File
# Begin Source File

SOURCE=.\OptionTab10.h
# End Source File
# Begin Source File

SOURCE=.\OptionTab11.h
# End Source File
# Begin Source File

SOURCE=.\OptionTab2.h
# End Source File
# Begin Source File

SOURCE=.\OptionTab3.h
# End Source File
# Begin Source File

SOURCE=.\OptionTab4.h
# End Source File
# Begin Source File

SOURCE=.\OptionTab5.h
# End Source File
# Begin Source File

SOURCE=.\OptionTab6.h
# End Source File
# Begin Source File

SOURCE=.\OptionTab7.h
# End Source File
# Begin Source File

SOURCE=.\OptionTab8.h
# End Source File
# Begin Source File

SOURCE=.\OptionTab9.h
# End Source File
# Begin Source File

SOURCE=.\ProxyId.h
# End Source File
# Begin Source File

SOURCE=.\RasLoad.h
# End Source File
# Begin Source File

SOURCE=.\Resource.h
# End Source File
# Begin Source File

SOURCE=.\Shell.h
# End Source File
# Begin Source File

SOURCE=.\splitter.h
# End Source File
# Begin Source File

SOURCE=.\StdAfx.h
# End Source File
# Begin Source File

SOURCE=.\trans.h
# End Source File
# Begin Source File

SOURCE=.\TreeViewToolTip.h
# End Source File
# Begin Source File

SOURCE=.\Wid1.h
# End Source File
# Begin Source File

SOURCE=.\WinHTTrack.h
# End Source File
# Begin Source File

SOURCE=.\WinHTTrackDoc.h
# End Source File
# Begin Source File

SOURCE=.\WinHTTrackView.h
# End Source File
# Begin Source File

SOURCE=.\WIZARD.H
# End Source File
# Begin Source File

SOURCE=.\WIZARD2.H
# End Source File
# Begin Source File

SOURCE=.\WizLinks.h
# End Source File
# Begin Source File

SOURCE=.\WizTab.h
# End Source File
# Begin Source File

SOURCE=.\XSHBrowseForFolder.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\res\ico00001.ico
# End Source File
# Begin Source File

SOURCE=.\res\icon1.ico
# End Source File
# Begin Source File

SOURCE=.\res\icon2.ico
# End Source File
# Begin Source File

SOURCE=.\res\icon3.ico
# End Source File
# Begin Source File

SOURCE=.\res\icon4.ico
# End Source File
# Begin Source File

SOURCE=.\res\icon5.ico
# End Source File
# Begin Source File

SOURCE=.\res\icon6.ico
# End Source File
# Begin Source File

SOURCE=.\res\idr_main.ico
# End Source File
# Begin Source File

SOURCE=.\res\mainfram.bmp
# End Source File
# Begin Source File

SOURCE=.\res\Shell.ico
# End Source File
# Begin Source File

SOURCE=.\res\www.cur
# End Source File
# End Group
# Begin Source File

SOURCE=.\ReadMe.txt
# End Source File
# End Target
# End Project
