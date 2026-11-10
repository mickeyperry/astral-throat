; Astral Throat installer (Inno Setup 6). Built by ..\build.bat.
; Same look as the ReWasp installer: artwork in brand\ comes from that project.
#define AppVersion "0.1.0"
#define Artefacts "..\build\AstralThroat_artefacts\Release"

[Setup]
AppId={{3F7A2C91-5D4E-4B8A-A6C3-9E1B0D2F7A64}
AppName=Astral Throat
AppVersion={#AppVersion}
AppPublisher=Pafu Igniters
AppPublisherURL=https://github.com/mickeyperry/astral-throat
AppSupportURL=https://github.com/mickeyperry/astral-throat/issues
DefaultDirName={autopf}\Astral Throat
DisableDirPage=yes
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=Output
OutputBaseFilename=AstralThroat-Setup-{#AppVersion}
Compression=lzma2
SolidCompression=yes
; dark, borderless look matching the artwork
WizardStyle=modern dark hidebevels
WizardBackColor=#0B0B0F
DisableWelcomePage=no
UninstallDisplayName=Astral Throat (VST3 + standalone)
UninstallDisplayIcon={app}\astralthroat.ico
SetupIconFile=brand\astralthroat.ico
; Pafu Igniters / Mickey Perry artwork; Setup picks the size that fits the screen scaling
WizardImageFile=brand\wizard_small.png,brand\wizard.png
WizardSmallImageFile=brand\corner_small.png,brand\corner.png
WizardImageBackColor=#0B0B0F
CloseApplications=no

[Files]
Source: "{#Artefacts}\VST3\Astral Throat.vst3\*"; DestDir: "{commoncf64}\VST3\Astral Throat.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#Artefacts}\Standalone\Astral Throat.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion
Source: "brand\astralthroat.ico"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\Astral Throat"; Filename: "{app}\Astral Throat.exe"

[UninstallDelete]
Type: dirifempty; Name: "{commoncf64}\VST3\Astral Throat.vst3"

[Messages]
WelcomeLabel1=Astral Throat
WelcomeLabel2=A psychedelic throat-singing vocal synth: a singing oracle with vowels, overtones, chaos and a spiralling delay.%n%nThis installs the VST3 plugin for your DAW and a standalone app with an on-screen keyboard.%n%nMade by Mickey Perry, aka Pafu Igniters (breakbeat, trance and drum & bass), who also does motion design and animation.%n%nClose your DAW before you continue.
FinishedHeadingLabel=Astral Throat is installed
FinishedLabel=Open your DAW and rescan plugins: Astral Throat is under the vendor "Throat Lab". The standalone app is in the Start menu.
FinishedLabelNoIcons=Open your DAW and rescan plugins: Astral Throat is under the vendor "Throat Lab". The standalone app is in the Start menu.

[Code]
procedure OpenLink(Sender: TObject);
var
  Code: Integer;
begin
  ShellExec('open', TNewStaticText(Sender).Hint, '', '', SW_SHOWNORMAL, ewNoWait, Code);
end;

// links under the finish text: the music, then the motion design / animation work
procedure AddLink(var Y: Integer; const Caption, Url: String; Heading: Boolean);
var
  L: TNewStaticText;
begin
  L := TNewStaticText.Create(WizardForm);
  L.Parent := WizardForm.FinishedPage;
  L.Left := WizardForm.FinishedLabel.Left;
  L.Top := Y;
  L.Caption := Caption;
  if Heading then begin
    L.Font.Style := [fsBold];
    Y := Y + L.Height + ScaleY(4);
  end else begin
    L.Hint := Url;
    L.Cursor := crHand;
    L.Font.Color := $872DFF;   // #ff2d87, mickeyperry.github.io pink (BGR)
    L.Font.Style := [fsUnderline];
    L.OnClick := @OpenLink;
    L.Left := L.Left + ScaleX(12);
    Y := Y + L.Height + ScaleY(2);
  end;
end;

var
  LinksAdded: Boolean;

procedure AddLinks;
var
  Y: Integer;
begin
  if LinksAdded then Exit;
  LinksAdded := True;

  Y := WizardForm.FinishedLabel.Top + WizardForm.FinishedLabel.Height + ScaleY(24);
  AddLink(Y, 'Pafu Igniters (music)', '', True);
  AddLink(Y, 'SoundCloud', 'https://soundcloud.com/pafu', False);
  AddLink(Y, 'Bandcamp', 'https://pafu.bandcamp.com', False);
  Y := Y + ScaleY(12);
  AddLink(Y, 'Mickey Perry (motion design and animation)', '', True);
  AddLink(Y, 'mickeyperry.github.io', 'https://mickeyperry.github.io/', False);
end;

procedure CurPageChanged(CurPageID: Integer);
begin
  if CurPageID = wpFinished then AddLinks;
end;
