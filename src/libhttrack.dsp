# Microsoft Developer Studio Project File - Name="libhttrack" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=libhttrack - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "libhttrack.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "libhttrack.mak" CFG="libhttrack - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "libhttrack - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "libhttrack - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "libhttrack - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "C:\temp\Releaselib"
# PROP Intermediate_Dir "C:\temp\Releaselib"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "LIBHTTRACK_EXPORTS" /Yu"stdafx.h" /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /Ob2 /I "C:\Dev\IPv6Kit\inc" /I "C:\Dev\zlib\include" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "LIBHTTRACK_EXPORTS" /D "ZLIB_DLL" /FD /c
# SUBTRACT CPP /YX /Yc /Yu
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x40c /d "NDEBUG"
# ADD RSC /l 0x40c /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 wsock32.lib zdll.lib /nologo /dll /map /machine:I386 /out:"L:\HTTrack\httrack\libhttrack.dll" /libpath:"C:\Dev\zlib\lib"
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "libhttrack - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "C:\temp\Debuglib"
# PROP Intermediate_Dir "C:\temp\Debuglib"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "LIBHTTRACK_EXPORTS" /Yu"stdafx.h" /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "C:\Dev\IPv6Kit\inc" /I "C:\Dev\zlib\include" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "LIBHTTRACK_EXPORTS" /D "ZLIB_DLL" /FD /GZ /c
# SUBTRACT CPP /YX /Yc /Yu
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x40c /d "_DEBUG"
# ADD RSC /l 0x40c /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 wsock32.lib zdll.lib /nologo /dll /map /debug /machine:I386 /out:"C:\temp\libhttrack.dll" /pdbtype:sept /libpath:"C:\Dev\zlib\lib"
# SUBTRACT LINK32 /pdb:none

!ENDIF 

# Begin Target

# Name "libhttrack - Win32 Release"
# Name "libhttrack - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=minizip\crypt.h
# End Source File
# Begin Source File

SOURCE="hts-indextmpl.h"
# End Source File
# Begin Source File

SOURCE=htsalias.c
# End Source File
# Begin Source File

SOURCE=htsalias.h
# End Source File
# Begin Source File

SOURCE=htsback.c
# End Source File
# Begin Source File

SOURCE=htsback.h
# End Source File
# Begin Source File

SOURCE=htsbase.h
# End Source File
# Begin Source File

SOURCE=htsbasenet.h
# End Source File
# Begin Source File

SOURCE=htsbauth.c
# End Source File
# Begin Source File

SOURCE=htsbauth.h
# End Source File
# Begin Source File

SOURCE=htscache.c
# End Source File
# Begin Source File

SOURCE=htscache.h
# End Source File
# Begin Source File

SOURCE=htscatchurl.c
# End Source File
# Begin Source File

SOURCE=htscatchurl.h
# End Source File
# Begin Source File

SOURCE=htsconfig.h
# End Source File
# Begin Source File

SOURCE=htscore.c
# End Source File
# Begin Source File

SOURCE=htscore.h
# End Source File
# Begin Source File

SOURCE=htscoremain.c
# End Source File
# Begin Source File

SOURCE=htscoremain.h
# End Source File
# Begin Source File

SOURCE=htsdefines.h
# End Source File
# Begin Source File

SOURCE=htsfilters.c
# End Source File
# Begin Source File

SOURCE=htsfilters.h
# End Source File
# Begin Source File

SOURCE=htsftp.c
# End Source File
# Begin Source File

SOURCE=htsftp.h
# End Source File
# Begin Source File

SOURCE=htsglobal.h
# End Source File
# Begin Source File

SOURCE=htshash.c
# End Source File
# Begin Source File

SOURCE=htshash.h
# End Source File
# Begin Source File

SOURCE=htshelp.c
# End Source File
# Begin Source File

SOURCE=htshelp.h
# End Source File
# Begin Source File

SOURCE=htsindex.c
# End Source File
# Begin Source File

SOURCE=htsindex.h
# End Source File
# Begin Source File

SOURCE=htsinthash.c
# End Source File
# Begin Source File

SOURCE=htsjava.c
# End Source File
# Begin Source File

SOURCE=htsjava.h
# End Source File
# Begin Source File

SOURCE=htslib.c
# End Source File
# Begin Source File

SOURCE=htslib.h
# End Source File
# Begin Source File

SOURCE=htsmd5.c
# End Source File
# Begin Source File

SOURCE=htsmd5.h
# End Source File
# Begin Source File

SOURCE=htsmodules.c
# End Source File
# Begin Source File

SOURCE=htsmodules.h
# End Source File
# Begin Source File

SOURCE=htsname.c
# End Source File
# Begin Source File

SOURCE=htsname.h
# End Source File
# Begin Source File

SOURCE=htsnet.h
# End Source File
# Begin Source File

SOURCE=htsnostatic.c
# End Source File
# Begin Source File

SOURCE=htsnostatic.h
# End Source File
# Begin Source File

SOURCE=htsopt.h
# End Source File
# Begin Source File

SOURCE=htsparse.c
# End Source File
# Begin Source File

SOURCE=htsparse.h
# End Source File
# Begin Source File

SOURCE=htsrobots.c
# End Source File
# Begin Source File

SOURCE=htsrobots.h
# End Source File
# Begin Source File

SOURCE=htsstrings.h
# End Source File
# Begin Source File

SOURCE=htssystem.h
# End Source File
# Begin Source File

SOURCE=htsthread.c
# End Source File
# Begin Source File

SOURCE=htsthread.h
# End Source File
# Begin Source File

SOURCE=htstools.c
# End Source File
# Begin Source File

SOURCE=htstools.h
# End Source File
# Begin Source File

SOURCE=htswizard.c
# End Source File
# Begin Source File

SOURCE=htswizard.h
# End Source File
# Begin Source File

SOURCE=htswrap.c
# End Source File
# Begin Source File

SOURCE=htswrap.h
# End Source File
# Begin Source File

SOURCE=htszlib.c
# End Source File
# Begin Source File

SOURCE=htszlib.h
# End Source File
# Begin Source File

SOURCE=minizip\ioapi.c
# End Source File
# Begin Source File

SOURCE=minizip\ioapi.h
# End Source File
# Begin Source File

SOURCE=minizip\iowin32.c
# End Source File
# Begin Source File

SOURCE=minizip\iowin32.h
# End Source File
# Begin Source File

SOURCE=md5.c
# End Source File
# Begin Source File

SOURCE=md5.h
# End Source File
# Begin Source File

SOURCE=minizip\mztools.c
# End Source File
# Begin Source File

SOURCE=minizip\mztools.h
# End Source File
# Begin Source File

SOURCE=minizip\unzip.c
# End Source File
# Begin Source File

SOURCE=minizip\unzip.h
# End Source File
# Begin Source File

SOURCE=minizip\zip.c
# End Source File
# Begin Source File

SOURCE=minizip\zip.h
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# Begin Source File

SOURCE=.\ReadMe.txt
# End Source File
# End Target
# End Project
