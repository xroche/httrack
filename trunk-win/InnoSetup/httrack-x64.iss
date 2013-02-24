; HTTrack Website Copier - x64 edition

[Setup]
AppVerName=WinHTTrack Website Copier 3.46-1 (x64)
AppVersion=3.46.1
AppName=WinHTTrack Website Copier
ArchitecturesInstallIn64BitMode=x64
ArchitecturesAllowed=x64
; Setup
VersionInfoProductName=WinHTTrack Website Copier (x64)
AppPublisher=HTTrack
AppPublisherURL=http://www.httrack.com
AppSupportURL=http://forum.httrack.com
AppUpdatesURL=http://www.httrack.com/page/2/
DefaultDirName={pf}\WinHTTrack
DefaultGroupName=WinHTTrack
AllowNoIcons=yes
;AlwaysCreateUninstallIcon=yes
LicenseFile=O:\HTTrackFiles\setup_license.txt
; uncomment the following line if you want your installation to run on NT 3.51 too.
MinVersion=4,4
;;AdminPrivilegesRequired=no
AppMutex=WinHTTrack_RUN
OutputBaseFilename=httrack_x64
OutputDir=O:\
SetupIconFile=C:\Dev\WinHTTrack\res\Shell.ico
UninstallIconFile=C:\Dev\WinHTTrack\res\Shell.ico
;DiskSpanning=yes
;DiskSize=1048576

[Tasks]
Name: "regfiles"; Description: "Register WinHTTrack file types and program setup"; GroupDescription: "Setup:"; MinVersion: 4,4
Name: "desktopicon"; Description: "Create a &desktop icon"; GroupDescription: "Additional icons:"; MinVersion: 4,4
Name: "iebar"; Description: "Add a quick launch shortcut in Internet Explorer bar"; GroupDescription: "Additional icons:"; MinVersion: 4,4
Name: "quicklaunchicon"; Description: "Create a &quick launch icon"; GroupDescription: "Additional icons:"; MinVersion: 4,4; Flags: unchecked

[Files]
Source: "O:\HTTrack\httrack\html\*.*"; DestDir: "{app}\html"; CopyMode: alwaysoverwrite; Flags: recursesubdirs
Source: "O:\HTTrack\httrack\templates\*.*"; DestDir: "{app}\templates"; CopyMode: alwaysoverwrite; Flags: recursesubdirs
Source: "O:\HTTrack\httrack\lang\*.*"; DestDir: "{app}\lang"; CopyMode: alwaysoverwrite; Flags: recursesubdirs
Source: "O:\HTTrack\httrack\libtest\*.*"; DestDir: "{app}\libtest"; CopyMode: alwaysoverwrite; Flags: recursesubdirs
Source: "O:\HTTrack\httrack\src\*.*"; DestDir: "{app}\src"; CopyMode: alwaysoverwrite; Flags: recursesubdirs
Source: "O:\HTTrack\httrack\src_win\*.*"; DestDir: "{app}\src_win"; CopyMode: alwaysoverwrite; Flags: recursesubdirs
Source: "O:\HTTrack\httrack\*.*"; Excludes: "*.dll,*.manifest,*.exe"; DestDir: "{app}"; CopyMode: alwaysoverwrite
Source: "O:\HTTrack\httrack\x64\*"; DestDir: "{app}"; CopyMode: alwaysoverwrite
Source: "O:\HTTrack\file_id.diz"; DestDir: "{app}"; CopyMode: alwaysoverwrite
Source: "O:\HTTrackFiles\winhttrack.log"; DestDir: "{app}"; CopyMode: alwaysoverwrite
;Source: "O:\HTTrack\httrack\dll\*.dll"; DestDir: "{sys}"; CopyMode: normal; Flags: uninsneveruninstall sharedfile restartreplace
Source: "O:\HTTrack\httrack\WinHTTrackIEBar.dll"; DestDir: "{app}"; CopyMode: alwaysoverwrite; Flags: regserver; Tasks: iebar

[Icons]
Name: "{group}\WinHTTrack Website Copier"; Filename: "{app}\WinHTTrack.exe"; Comment: "Launch WinHTTrack Website Copier"; WorkingDir: "{app}"
Name: "{group}\Documentation"; Filename: "{app}\httrack-doc.html"; Comment: "View documentation"; WorkingDir: "{app}"
Name: "{group}\readme"; Filename: "{win}\notepad.exe"; Parameters: "{app}\readme"; Comment: "README"; WorkingDir: "{app}"
Name: "{group}\copying"; Filename: "{win}\notepad.exe"; Parameters: "{app}\copying"; Comment: "COPYING"; WorkingDir: "{app}"
Name: "{group}\history.txt"; Filename: "{win}\notepad.exe"; Parameters: "{app}\history.txt"; Comment: "history.txt"; WorkingDir: "{app}"
Name: "{group}\license.txt"; Filename: "{win}\notepad.exe"; Parameters: "{app}\license.txt"; Comment: "license.txt"; WorkingDir: "{app}"
Name: "{group}\greetings.txt"; Filename: "{win}\notepad.exe"; Parameters: "{app}\greetings.txt"; Comment: "greetings.txt"; WorkingDir: "{app}"
Name: "{userdesktop}\HTTrack Website Copier"; Filename: "{app}\WinHTTrack.exe"; MinVersion: 4,4; Tasks: desktopicon
Name: "{userappdata}\Microsoft\Internet Explorer\Quick Launch\HTTrack Website Copier"; Filename: "{app}\WinHTTrack.exe"; MinVersion: 4,4; Tasks: quicklaunchicon

[Registry]
; Start "Software\My Company\My Program" keys under HKEY_CURRENT_USER
; and HKEY_LOCAL_MACHINE. The flags tell it to always delete the
; "My Program" keys upon uninstall, and delete the "My Company" keys
; if there is nothing left in them.
Root: HKCU; Subkey: "Software\WinHTTrack Website Copier"; Flags: uninsdeletekeyifempty noerror
Root: HKCU; Subkey: "Software\WinHTTrack Website Copier\WinHTTrack Website Copier"; Flags: uninsdeletekey noerror
Root: HKCU; Subkey: "Software\WinHTTrack Website Copier\WinHTTrack Website Copier\Interface"; ValueType: dword; ValueName: "SetupRun"; ValueData: 1; Flags: uninsdeletekey noerror
Root: HKCU; Subkey: "Software\WinHTTrack Website Copier\WinHTTrack Website Copier\Interface"; ValueType: dword; ValueName: "SetupHasRegistered"; ValueData: 1; Flags: uninsdeletekey noerror; Tasks: regfiles
Root: HKLM; Subkey: "Software\WinHTTrack Website Copier"; Flags: uninsdeletekeyifempty noerror
Root: HKLM; Subkey: "Software\WinHTTrack Website Copier\WinHTTrack Website Copier"; Flags: uninsdeletekey noerror
Root: HKLM; Subkey: "Software\WinHTTrack Website Copier\WinHTTrack Website Copier"; ValueType: string; ValueName: "Path"; ValueData: "{app}"; Flags: uninsdeletekey noerror
; .whtt launch
Root: HKCR; Subkey: ".whtt\ShellNew"; Flags: uninsdeletekey noerror; Tasks: regfiles
Root: HKCR; Subkey: ".whtt"; Flags: uninsdeletekey noerror; Tasks: regfiles
; New menu
Root: HKCR; Subkey: "WinHTTrackProject"; Flags: uninsdeletekey noerror; Tasks: regfiles
Root: HKLM; Subkey: "Software\Classes\WinHTTrackProject"; Flags: uninsdeletekey noerror; Tasks: regfiles
; Launch and various things to remove
Root: HKCR; Subkey: "Applications\WinHTTrack.exe"; Flags: uninsdeletekey noerror; Tasks: regfiles
Root: HKLM; Subkey: "SOFTWARE\Classes\Applications\WinHTTrack.exe"; Flags: uninsdeletekey noerror; Tasks: regfiles
; Sound events
Root: HKCU; Subkey: "AppEvents\Schemes\Apps\WinHTTrack"; ValueType: string; ValueData: "WinHTTrack Website Copier"; Flags: uninsdeletekey noerror; Tasks: regfiles
Root: HKCU; Subkey: "AppEvents\Schemes\Apps\WinHTTrack\MirrorStarted\.Current"; ValueType: string; ValueData: ""; Flags: uninsdeletekey noerror; Tasks: regfiles
Root: HKCU; Subkey: "AppEvents\Schemes\Apps\WinHTTrack\MirrorStarted\.Default"; ValueType: string; ValueData: ""; Flags: uninsdeletekey noerror; Tasks: regfiles
Root: HKCU; Subkey: "AppEvents\Schemes\Apps\WinHTTrack\MirrorFinished\.Current"; ValueType: string; ValueData: "{app}\html\server\sfx\finished.wav"; Flags: uninsdeletekey noerror; Tasks: regfiles
Root: HKCU; Subkey: "AppEvents\Schemes\Apps\WinHTTrack\MirrorFinished\.Default"; ValueType: string; ValueData: "{app}\html\server\sfx\finished.wav"; Flags: uninsdeletekey noerror; Tasks: regfiles
Root: HKCU; Subkey: "AppEvents\EventLabels\MirrorFinished"; ValueType: string; ValueData: "Mirror Finished"; Flags: uninsdeletekey noerror; Tasks: regfiles
Root: HKU; Subkey: ".DEFAULT\AppEvents\Schemes\Apps\WinHTTrack"; ValueType: string; ValueData: "WinHTTrack Website Copier"; Flags: uninsdeletekey noerror; Tasks: regfiles
Root: HKU; Subkey: ".DEFAULT\AppEvents\Schemes\Apps\WinHTTrack\MirrorStarted\.Current"; ValueType: string; ValueData: ""; Flags: uninsdeletekey noerror; Tasks: regfiles
Root: HKU; Subkey: ".DEFAULT\AppEvents\Schemes\Apps\WinHTTrack\MirrorStarted\.Default"; ValueType: string; ValueData: ""; Flags: uninsdeletekey noerror; Tasks: regfiles
Root: HKU; Subkey: ".DEFAULT\AppEvents\Schemes\Apps\WinHTTrack\MirrorFinished\.Current"; ValueType: string; ValueData: "{app}\html\server\sfx\finished.wav"; Flags: uninsdeletekey noerror; Tasks: regfiles
Root: HKU; Subkey: ".DEFAULT\AppEvents\Schemes\Apps\WinHTTrack\MirrorFinished\.Default"; ValueType: string; ValueData: "{app}\html\server\sfx\finished.wav"; Flags: uninsdeletekey noerror; Tasks: regfiles
Root: HKU; Subkey: ".DEFAULT\AppEvents\EventLabels\MirrorFinished"; ValueType: string; ValueData: "Mirror Finished"; Flags: uninsdeletekey noerror; Tasks: regfiles

; See http://msdn.microsoft.com/library/default.asp?url=/workshop/browser/ext/tutorials/menu.asp
;Root: HKLM; Subkey: "Software\Microsoft\Internet Explorer\Extensions\{{36ECAF82-3300-8F84-092E-AFF36D6C7040}}"; Flags: uninsdeletekey noerror
;Root: HKLM; Subkey: "Software\Microsoft\Internet Explorer\Extensions\{{36ECAF82-3300-8F84-092E-AFF36D6C7040}}"; ValueType: string; ValueName: "CLSID"; ValueData: "{{1FBA04EE-3024-11d2-8F1F-0000F87ABD16}}"; Flags: uninsdeletekey noerror
;Root: HKLM; Subkey: "Software\Microsoft\Internet Explorer\Extensions\{{36ECAF82-3300-8F84-092E-AFF36D6C7040}}"; ValueType: string; ValueName: "MenuText"; ValueData: "Launch WinHTTrack"; Flags: uninsdeletekey noerror
;Root: HKLM; Subkey: "Software\Microsoft\Internet Explorer\Extensions\{{36ECAF82-3300-8F84-092E-AFF36D6C7040}}"; ValueType: string; ValueName: "MenuStatusBar"; ValueData: "Mirror Websites with WinHTTrack"; Flags: uninsdeletekey noerror
;Root: HKLM; Subkey: "Software\Microsoft\Internet Explorer\Extensions\{{36ECAF82-3300-8F84-092E-AFF36D6C7040}}"; ValueType: string; ValueName: "ClsidExtension"; ValueData: "{{86529161-034E-4F8A-88D2-3C625E612E04}}"; Flags: uninsdeletekey noerror
;Root: HKLM; Subkey: "Software\Microsoft\Internet Explorer\Extensions\{{36ECAF82-3300-8F84-092E-AFF36D6C7040}}"; ValueType: string; ValueName: "Default Visible"; ValueData: "yes"; Flags: uninsdeletekey noerror
;Root: HKLM; Subkey: "Software\Microsoft\Internet Explorer\Extensions\{{36ECAF82-3300-8F84-092E-AFF36D6C7040}}"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\WinHTTrack.exe"; Flags: uninsdeletekey noerror
;Root: HKLM; Subkey: "Software\Microsoft\Internet Explorer\Extensions\{{36ECAF82-3300-8F84-092E-AFF36D6C7040}}"; ValueType: string; ValueName: "HotIcon"; ValueData: "{app}\WinHTTrack.exe"; Flags: uninsdeletekey noerror
;Root: HKLM; Subkey: "Software\Microsoft\Internet Explorer\Extensions\{{36ECAF82-3300-8F84-092E-AFF36D6C7040}}"; ValueType: string; ValueName: "ButtonText"; ValueData: "Run WinHTTrack"; Flags: uninsdeletekey noerror

[Run]
Filename: "{app}\WinHTTrack.exe"; Description: "Launch WinHTTrack Website Copier"; Flags: nowait postinstall skipifsilent
Filename: "{win}\notepad.exe"; Parameters: "{app}\history.txt"; WorkingDir: "{app}"; Description: "View history.txt file"; Flags: nowait postinstall skipifsilent

