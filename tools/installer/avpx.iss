; Script generated by the Inno Setup Script Wizard.
; SEE THE DOCUMENTATION FOR DETAILS ON CREATING INNO SETUP SCRIPT FILES!

#define MyAppName "AvPx"
#define MyAppVersion "0.1"
#define MyAppPublisher "SirLemonhead"
#define MyAppURL "http://homepage.eircom.net/~duncandsl/avp/"
#define MyAppExeName "AvPx.exe"

[Setup]
; NOTE: The value of AppId uniquely identifies this application.
; Do not use the same AppId value in installers for other applications.
; (To generate a new GUID, click Tools | Generate GUID inside the IDE.)
AppID={{6214FDEA-8101-434C-86C0-6CBF1380D025}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
;AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={pf}\Fox\Aliens versus Predator
DefaultGroupName={#MyAppName}
OutputBaseFilename=setup_AvPx
Compression=lzma/Max
SolidCompression=true
UsePreviousAppDir=false
EnableDirDoesntExistWarning=true
AppendDefaultDirName=false
DirExistsWarning=no
SetupIconFile=..\..\win32\avp.ico
UninstallDisplayIcon={app}\AvPx.exe

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "..\..\redist\AvPx.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\redist\AvPx_License.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\redist\AvPx_Readme.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\..\redist\directx\*"; DestDir: "{app}\redist\directx\"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\..\redist\vcredist_x86.exe"; DestDir: "{app}\redist\vc\"; Flags: ignoreversion
Source: "..\..\redist\shaders\*"; DestDir: "{app}\shaders\"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\..\redist\dlls\*"; DestDir: "{app}";
; NOTE: Don't use "Flags: ignoreversion" on any shared system files

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{commondesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\redist\vc\vcredist_x86.exe"; Description: "Install Visual Studio 2010 SP1 Runtime";
Filename: "{app}\redist\directx\DXSETUP.exe"; Description: "Install DirectX Runtimes";
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, "&", "&&")}}"; Flags: nowait postinstall skipifsilent


[UninstallRun]

[UninstallDelete]
Name: {app}\shaders\; Type: filesandordirs; 
Name: {app}\redist\; Type: filesandordirs;
Name: {app}\AvPx.exe; Type: files;
Name: {app}\enet.dll; Type: files;
Name: {app}\libbinkdec.dll; Type: files;
Name: {app}\libogg.dll; Type: files;
Name: {app}\libsmackerdec.dll; Type: files;
Name: {app}\libtheora.dll; Type: files;
Name: {app}\libvorbis.dll; Type: files;
Name: {app}\libvorbisfile.dll; Type: files;
Name: {app}\OpenAL32.dll; Type: files;
Name: {app}\AvPx_License.txt; Type: files;
Name: {app}\AvPx_Readme.txt; Type: files;