#define MyAppName "Clipboard Shield"
#define MyAppVersion "20250228"
#define MyAppURL "https://github.com/CoinFabrik/ClipboardShield"
#define MyAppExeName "trayicon.exe"
#define SourceBasePath ".."

[Setup]
AppId={{B0ED9BB2-232B-4CBB-BE71-1C17A4646E6F}}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={commonpf}\{#MyAppName}
DisableProgramGroupPage=yes
LicenseFile={#SourceBasePath}\LICENSE
OutputDir={#SourceBasePath}\bin64\
OutputBaseFilename=ClipboardShield_setup
SetupIconFile=icons\icon.ico
Compression=lzma
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64os

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

;[Tasks]
;Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#SourceBasePath}\Src\bin\ClipboardFirewallDll32.dll"                              ; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "{#SourceBasePath}\Src\bin\CFManualInjector32.exe"                                  ; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "{#SourceBasePath}\Src\bin\InjectDll32.exe"                                         ; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "{#SourceBasePath}\Src\bin\trayicon.exe"                                            ; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "{#SourceBasePath}\Src\bin64\ClipboardFirewallService64.exe"                        ; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "{#SourceBasePath}\Src\bin64\ClipboardFirewallDll64.dll"                            ; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "{#SourceBasePath}\Src\bin64\CFManualInjector64.exe"                                ; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "{#SourceBasePath}\Src\bin64\InjectDll64.exe"                                       ; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "{#SourceBasePath}\LICENSE"                                                         ; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceBasePath}\README.md"                                                       ; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceBasePath}\doc\sample\c\ProgramData\CoinFabrik Clipboard Shield\config.txt" ; DestDir: "{commonappdata}\CoinFabrik Clipboard Shield"; Flags: ignoreversion
Source: "{#SourceBasePath}\driver_binary\x64\DeviarePD.sys"                                 ; DestDir: "{tmp}\ClipboardShield"; Flags: ignoreversion deleteafterinstall
Source: "{#SourceBasePath}\Src\bin64\DriverInstaller64.exe"                                 ; DestDir: "{tmp}\ClipboardShield"; Flags: ignoreversion deleteafterinstall

[Icons]
Name: "{commonstartup}\{#MyAppName}"; Filename: "{app}\bin\trayicon.exe"

[Run]
Filename: "{tmp}\ClipboardShield\DriverInstaller64.exe"; WorkingDir: "{tmp}\ClipboardShield"; StatusMsg: "Installing driver..."; Parameters: "-installdriver"; Flags: runhidden
Filename: "{app}\bin\ClipboardFirewallService64.exe"; WorkingDir: "{app}\bin"; StatusMsg: "Installing service..."; Parameters: "-install"; Flags: runhidden
