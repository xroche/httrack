# Microsoft Developer Studio Project File - Name="httrack" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=httrack - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "httrack.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "httrack.mak" CFG="httrack - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "httrack - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "httrack - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE "httrack - Win32 Release avec debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "httrack - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "c:\temp\vcpp"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /Gi /O2 /Op /Ob2 /I "C:\Dev\IPv6Kit\inc\\" /I "C:\Dev\zlib\\" /I "C:\Dev\openssl\include" /I "C:\Dev\Winhttrack" /D "NDEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "HTS_ANALYSTE_CONSOLE" /YX /FD /Zm200 /c
# SUBTRACT CPP /Fr
# ADD BASE RSC /l 0x40c /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 wsock32.lib libhttrack.lib /nologo /subsystem:console /machine:I386 /out:"L:\HTTrack\httrack\httrack.exe" /libpath:"C:\Dev\openssl\lib" /libpath:"C:\Dev\zlib\dll32" /libpath:"C:\Dev\openssl\lib\out32dll" /libpath:"C:\temp\Releaselib"
# SUBTRACT LINK32 /verbose

!ELSEIF  "$(CFG)" == "httrack - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "c:\temp\vcpp"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MDd /W3 /Gm /GR /ZI /Od /I "C:\Dev\IPv6Kit\inc\\" /I "C:\Dev\zlib\\" /I "C:\Dev\openssl\include" /I "C:\Dev\Winhttrack" /D "_DEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "HTS_ANALYSTE_CONSOLE" /FAcs /Fr /FD /Zm200 /c
# ADD BASE RSC /l 0x40c /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 wsock32.lib libhttrack.lib /nologo /subsystem:console /debug /debugtype:both /machine:I386 /out:"C:\temp\httrack.exe" /pdbtype:sept /libpath:"C:\Dev\openssl\lib" /libpath:"C:\Dev\zlib\dll32" /libpath:"C:\Dev\openssl\lib\out32dll" /libpath:"C:\temp\Debuglib"
# SUBTRACT LINK32 /profile /map

!ELSEIF  "$(CFG)" == "httrack - Win32 Release avec debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "httrack___Win32_Release_avec_debug"
# PROP BASE Intermediate_Dir "httrack___Win32_Release_avec_debug"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release_avec_debug"
# PROP Intermediate_Dir "c:\temp\vcpp"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /Ot /Oi /Oy /Ob2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# SUBTRACT BASE CPP /Ox /Oa /Ow /Og /Os
# ADD CPP /nologo /MD /W3 /GX /Zi /Ot /Oi /Oy /Ob2 /I "C:\Dev\IPv6Kit\inc\\" /I "C:\Dev\zlib\\" /I "C:\Dev\openssl\include" /I "C:\Dev\Winhttrack" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "HTS_ANALYSTE_CONSOLE" /FAcs /FR /YX /FD /Zm200 /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 wsock32.lib /nologo /subsystem:console /machine:I386 /out:"c:\temp\httrack.exe"
# SUBTRACT BASE LINK32 /verbose
# ADD LINK32 wsock32.lib libhttrack.lib /nologo /subsystem:console /debug /machine:I386 /out:"L:\HTTrack\httrack\httrack.exe" /libpath:"C:\Dev\openssl\lib" /libpath:"C:\Dev\zlib\dll32" /libpath:"C:\Dev\openssl\lib\out32dll"
# SUBTRACT LINK32 /verbose

!ENDIF 

# Begin Target

# Name "httrack - Win32 Release"
# Name "httrack - Win32 Debug"
# Name "httrack - Win32 Release avec debug"
# Begin Source File

SOURCE=httrack.c
# End Source File
# Begin Source File

SOURCE=httrack.h
# End Source File
# End Target
# End Project
