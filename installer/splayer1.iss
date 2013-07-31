[Setup]
AppName=SPlayer
AppVersion=2.0.0
AppId=CaeSplayer
AppPublisherURL=https://github.com/Cae-Engineering/splayer
DefaultDirName={pf}\SPlayer
DefaultGroupName=SPlayer
Compression=lzma2
SolidCompression=yes
OutputDir=userdocs:Output
MinVersion=5.1
PrivilegesRequired=poweruser
CloseApplications=yes

[Files]
Source: "npSPlayer.dll"; DestDir: "{app}"; Flags: 32bit regserver uninsremovereadonly overwritereadonly replacesameversion

[Icons]
Name: "{group}\Uninstall SPlayer"; Filename: "{uninstallexe}"
