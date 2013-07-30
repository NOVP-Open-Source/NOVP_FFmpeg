[Setup]
AppName=SPlayer
AppVersion=2.0.0
AppId=CaeSplayer
DefaultDirName={pf}\SPlayer
DefaultGroupName=SPlayer
Compression=lzma2
SolidCompression=yes
OutputDir=userdocs:Output

[Files]
Source: "npSPlayer.dll"; DestDir: "{app}"; Flags: regserver

[Icons]
Name: "{group}\Uninstall SPlayer"; Filename: "{uninstallexe}"
