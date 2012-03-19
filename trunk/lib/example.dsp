# Microsoft Developer Studio Project File - Name="example" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=example - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "example.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "example.mak" CFG="example - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "example - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "example - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "example - Win32 Release"

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
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD BASE RSC /l 0x40c /d "NDEBUG"
# ADD RSC /l 0x40c /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 wsock32.lib /nologo /subsystem:console /machine:I386

!ELSEIF  "$(CFG)" == "example - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /FR /FD /GZ /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x40c /d "_DEBUG"
# ADD RSC /l 0x40c /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 wsock32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "example - Win32 Release"
# Name "example - Win32 Debug"
# Begin Source File

SOURCE=.\example.c
# End Source File
# Begin Source File

SOURCE=.\example.h
# End Source File
# Begin Source File

SOURCE=.\src\htsalias.c
# End Source File
# Begin Source File

SOURCE=.\src\htsalias.h
# End Source File
# Begin Source File

SOURCE=.\src\htsback.c
# End Source File
# Begin Source File

SOURCE=.\src\htsback.h
# End Source File
# Begin Source File

SOURCE=.\src\htsbase.h
# End Source File
# Begin Source File

SOURCE=.\src\htsbasenet.h
# End Source File
# Begin Source File

SOURCE=.\src\htsbauth.c
# End Source File
# Begin Source File

SOURCE=.\src\htsbauth.h
# End Source File
# Begin Source File

SOURCE=.\src\htscache.c
# End Source File
# Begin Source File

SOURCE=.\src\htscache.h
# End Source File
# Begin Source File

SOURCE=.\src\htscatchurl.c
# End Source File
# Begin Source File

SOURCE=.\src\htscatchurl.h
# End Source File
# Begin Source File

SOURCE=.\src\htsconfig.h
# End Source File
# Begin Source File

SOURCE=.\src\htscore.c
# End Source File
# Begin Source File

SOURCE=.\src\htscore.h
# End Source File
# Begin Source File

SOURCE=.\src\htscoremain.c
# End Source File
# Begin Source File

SOURCE=.\src\htscoremain.h
# End Source File
# Begin Source File

SOURCE=.\src\htsdefines.h
# End Source File
# Begin Source File

SOURCE=.\src\htsfilters.c
# End Source File
# Begin Source File

SOURCE=.\src\htsfilters.h
# End Source File
# Begin Source File

SOURCE=.\src\htsftp.c
# End Source File
# Begin Source File

SOURCE=.\src\htsftp.h
# End Source File
# Begin Source File

SOURCE=.\src\htsglobal.h
# End Source File
# Begin Source File

SOURCE=.\src\htshash.c
# End Source File
# Begin Source File

SOURCE=.\src\htshash.h
# End Source File
# Begin Source File

SOURCE=.\src\htshelp.c
# End Source File
# Begin Source File

SOURCE=.\src\htshelp.h
# End Source File
# Begin Source File

SOURCE=.\src\htsindex.c
# End Source File
# Begin Source File

SOURCE=.\src\htsindex.h
# End Source File
# Begin Source File

SOURCE=.\src\htsjava.c
# End Source File
# Begin Source File

SOURCE=.\src\htsjava.h
# End Source File
# Begin Source File

SOURCE=.\src\htslib.c
# End Source File
# Begin Source File

SOURCE=.\src\htslib.h
# End Source File
# Begin Source File

SOURCE=.\src\htsmd5.c
# End Source File
# Begin Source File

SOURCE=.\src\htsmd5.h
# End Source File
# Begin Source File

SOURCE=.\src\htsname.c
# End Source File
# Begin Source File

SOURCE=.\src\htsname.h
# End Source File
# Begin Source File

SOURCE=.\src\htsnet.h
# End Source File
# Begin Source File

SOURCE=.\src\htsopt.h
# End Source File
# Begin Source File

SOURCE=.\src\htsrobots.c
# End Source File
# Begin Source File

SOURCE=.\src\htsrobots.h
# End Source File
# Begin Source File

SOURCE=.\src\htsthread.c
# End Source File
# Begin Source File

SOURCE=.\src\htsthread.h
# End Source File
# Begin Source File

SOURCE=.\src\htstools.c
# End Source File
# Begin Source File

SOURCE=.\src\htstools.h
# End Source File
# Begin Source File

SOURCE=.\src\htswizard.c
# End Source File
# Begin Source File

SOURCE=.\src\htswizard.h
# End Source File
# Begin Source File

SOURCE=.\src\htswrap.c
# End Source File
# Begin Source File

SOURCE=.\src\htswrap.h
# End Source File
# Begin Source File

SOURCE=".\src\httrack-library.h"
# End Source File
# Begin Source File

SOURCE=.\src\httrack.c
# End Source File
# Begin Source File

SOURCE=.\src\httrack.h
# End Source File
# Begin Source File

SOURCE=.\src\md5.c
# End Source File
# Begin Source File

SOURCE=.\src\md5.h
# End Source File
# End Target
# End Project
