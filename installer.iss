; Installer CSV Commander (Inno Setup 6)
; Compile: ISCC installer.iss  →  dist\CSVCommander-Setup-x.x.x.exe

#define MyAppName "CSV Commander"
#define MyAppVersion "1.2.0"
#define MyAppPublisher "s4rt4"
#define MyAppURL "https://github.com/s4rt4/commander"
#define MyAppExeName "CSVCommander.exe"

[Setup]
AppId={{7C3D9A44-52B1-4E0F-9D2A-CSVCMDR11000}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
DefaultDirName={autopf}\CSV Commander
DefaultGroupName=CSV Commander
DisableProgramGroupPage=yes
PrivilegesRequiredOverridesAllowed=dialog
OutputDir=dist
OutputBaseFilename=CSVCommander-Setup-{#MyAppVersion}
SetupIconFile=src\app.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ChangesAssociations=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Buat shortcut di Desktop"; GroupDescription: "Shortcut tambahan:"; Flags: unchecked
Name: "csvassoc"; Description: "Jadikan CSV Commander pembuka default file .csv dan .tsv"; GroupDescription: "Asosiasi file:"

[Files]
Source: "CSVCommander.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "README.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\CSV Commander"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\CSV Commander"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Registry]
; ProgID dengan icon dokumen CSV (icon index 1 = csvfile.ico di dalam exe)
Root: HKA; Subkey: "Software\Classes\CSVCommander.Document"; ValueType: string; ValueData: "File CSV"; Flags: uninsdeletekey; Tasks: csvassoc
Root: HKA; Subkey: "Software\Classes\CSVCommander.Document\DefaultIcon"; ValueType: string; ValueData: "{app}\{#MyAppExeName},1"; Tasks: csvassoc
Root: HKA; Subkey: "Software\Classes\CSVCommander.Document\shell\open\command"; ValueType: string; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: csvassoc
Root: HKA; Subkey: "Software\Classes\.csv"; ValueType: string; ValueData: "CSVCommander.Document"; Tasks: csvassoc
Root: HKA; Subkey: "Software\Classes\.csv\OpenWithProgids"; ValueType: string; ValueName: "CSVCommander.Document"; ValueData: ""; Flags: uninsdeletevalue; Tasks: csvassoc
Root: HKA; Subkey: "Software\Classes\.tsv"; ValueType: string; ValueData: "CSVCommander.Document"; Tasks: csvassoc
Root: HKA; Subkey: "Software\Classes\.tsv\OpenWithProgids"; ValueType: string; ValueName: "CSVCommander.Document"; ValueData: ""; Flags: uninsdeletevalue; Tasks: csvassoc

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Jalankan CSV Commander"; Flags: nowait postinstall skipifsilent
