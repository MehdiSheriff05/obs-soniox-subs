; Inno Setup script for the obs-soniox-subs Windows installer.
;
; Defines are passed in from Package-Windows.ps1 via /D so this script has
; no hardcoded paths or version strings; the fallbacks below only exist so
; the script can still be compiled standalone (e.g. for local testing).
#ifndef MyAppVersion
  #define MyAppVersion "0.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\..\..\release\RelWithDebInfo\obs-soniox-subs"
#endif
#ifndef OutputDir
  #define OutputDir "..\..\..\release"
#endif
#ifndef OutputBaseFilename
  #define OutputBaseFilename "obs-soniox-subs-windows-x64-Installer"
#endif

#define MyAppName "Live Captions (Soniox)"
#define MyAppPublisher "Mehdi Sheriff"
#define MyAppURL "https://github.com/MehdiSheriff05/obs-soniox-subs"

[Setup]
; Fixed GUID, do not regenerate — Inno/Windows use it to recognize upgrades
; of the same product across versions.
AppId={{0A9C6C16-BAF6-4BE1-B859-4BE766F5AF70}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}/releases
; OBS's own docs recommend this location specifically because, unlike
; Program Files, it doesn't require elevated/admin install rights.
DefaultDirName={commonappdata}\obs-studio\plugins\obs-soniox-subs
DisableDirPage=yes
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
OutputDir={#OutputDir}
OutputBaseFilename={#OutputBaseFilename}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "{#SourceDir}\bin\64bit\obs-soniox-subs.dll"; DestDir: "{app}\bin\64bit"; Flags: ignoreversion
Source: "{#SourceDir}\data\locale\en-US.ini"; DestDir: "{app}\data\locale"; Flags: ignoreversion

[UninstallDelete]
Type: dirifempty; Name: "{app}\bin\64bit"
Type: dirifempty; Name: "{app}\bin"
Type: dirifempty; Name: "{app}\data\locale"
Type: dirifempty; Name: "{app}\data"
Type: dirifempty; Name: "{app}"
