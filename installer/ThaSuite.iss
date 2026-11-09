; Installeur Windows : copie les plugins VST3 dans C:\Program Files\Common Files\VST3 (dossier lu par FL Studio et les autres DAW)
#define AppName "Fck ThaSuite"
#define AppVersion "1.0.0"

[Setup]
AppId={{7D1E5C52-3C0B-4F6E-9B5B-5A7E0F1D0001}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=Yaza
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
UninstallFilesDir={autopf}\Fck ThaSuite
DefaultGroupName={#AppName}
OutputDir=..\installer_out
OutputBaseFilename=FckThaSuite-Installer
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
UninstallDisplayName={#AppName} (VST3)
WizardStyle=modern
LicenseFile=..\THIRD_PARTY_LICENSES.txt

#define HasMaster DirExists(AddBackslash(SourcePath) + "..\dist\Fck ThaMASTER.vst3")

[Types]
Name: "full"; Description: "Installation complete"
Name: "custom"; Description: "Personnalisee"; Flags: iscustom

[Components]
Name: "voice"; Description: "Fck ThaVOICE (suite vocale)"; Types: full custom
#if HasMaster
Name: "master"; Description: "Fck ThaMASTER (mastering)"; Types: full custom
#endif

[Files]
Source: "..\dist\Fck ThaVOICE.vst3\*"; DestDir: "{commoncf64}\VST3\Fck ThaVOICE.vst3"; Components: voice; Flags: ignoreversion recursesubdirs createallsubdirs
#if HasMaster
Source: "..\dist\Fck ThaMASTER.vst3\*"; DestDir: "{commoncf64}\VST3\Fck ThaMASTER.vst3"; Components: master; Flags: ignoreversion recursesubdirs createallsubdirs
#endif

[Messages]
FinishedLabel=Les plugins sont installes dans le dossier VST3. Dans FL Studio : Options > Manage plugins > Find installed plugins (ou "Scan"), puis cherche "Fck Tha".
