; Inno Setup Script for PTV2A AAX Plugin (Windows)
; Download Inno Setup from: https://jrsoftware.org/isdl.php
; Compile with: "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" installer_windows.iss

[Setup]
; Basic application info
AppName=PTV2A Audio Plugin
AppVersion=0.1.0
AppPublisher=ldegenhardt
AppPublisherURL=https://github.com/uzerterter/thesis-pt-v2a
AppSupportURL=https://github.com/uzerterter/thesis-pt-v2a/issues
AppUpdatesURL=https://github.com/uzerterter/thesis-pt-v2a/releases
DefaultDirName={commoncf}\Avid\Audio\Plug-Ins
DefaultGroupName=PTV2A

; Disable directory selection page (fixed install location for Pro Tools)
DisableDirPage=yes
DirExistsWarning=no

; Output configuration
OutputDir=installer_output
OutputBaseFilename=PTV2A-Windows-Setup-v0.1.0
SetupIconFile=Resources\icon.ico
Compression=lzma2
SolidCompression=yes

; Windows version requirements
MinVersion=10.0
ArchitecturesInstallIn64BitMode=x64

; Admin rights required (system-wide plugin installation)
PrivilegesRequired=admin

; License and info files (optional - add if you have them)
; LicenseFile=LICENSE.txt
; InfoBeforeFile=README.txt

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "german"; MessagesFile: "compiler:Languages\German.isl"

[Files]
; Copy the entire signed AAX plugin folder from staging directory
; Run this before compiling: cd C:\Users\Ludenbold\Desktop\Master_Thesis\Implementation\thesis-pt-v2a\aax-plugin; Remove-Item -Recurse -Force installer_staging -ErrorAction SilentlyContinue; robocopy "..\build\pt_v2a_artefacts\Release\AAX\PTV2A.aaxplugin" "installer_staging\PTV2A.aaxplugin" /E /NFL /NDL /NJH /NJS /nc /ns /np
Source: "installer_staging\PTV2A.aaxplugin\*"; \
  DestDir: "{commoncf}\Avid\Audio\Plug-Ins\PTV2A.aaxplugin"; \
  Flags: ignoreversion recursesubdirs createallsubdirs

[Dirs]
; Ensure plugin directory exists with proper permissions
Name: "{commoncf}\Avid\Audio\Plug-Ins\PTV2A.aaxplugin"; Permissions: everyone-readexec

[Icons]
; No Start Menu shortcuts needed (plugin only)

[Code]
function InitializeSetup(): Boolean;
var
  ResultCode: Integer;
begin
  Result := True;
  
  // Check if Pro Tools is running - warn user to close it
  if CheckForMutexes('ProTools') then
  begin
    if MsgBox('Pro Tools appears to be running. Please close Pro Tools before installing the plugin.' + #13#10 + #13#10 + 'Continue anyway?', mbConfirmation, MB_YESNO) = IDNO then
    begin
      Result := False;
      Exit;
    end;
  end;
end;

[Run]
; Optional: Launch Pro Tools after installation (commented out by default)
; Filename: "{pf}\Avid\Pro Tools\ProTools.exe"; Description: "Launch Pro Tools"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; Clean up config files on uninstall
Type: filesandordirs; Name: "{userappdata}\PTV2A"

[Messages]
; Custom messages
WelcomeLabel1=Welcome to PTV2A Audio Plugin Setup
WelcomeLabel2=This will install the PTV2A AAX plugin for Pro Tools.%n%nThe plugin uses AI to generate audio from video content and provides sound effect search capabilities.%n%nClick Next to continue.
FinishedHeadingLabel=Installation Complete
FinishedLabelNoIcons=PTV2A has been successfully installed.%n%nThe plugin is now available in Pro Tools under Plug-Ins > Utility > PTV2A.
