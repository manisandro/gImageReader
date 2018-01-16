SetCompressor /SOLID /FINAL lzma

!define MUI_ABORTWARNING
!define MUI_ICON "gimagereader.ico"
!define MULTIUSER_MUI
!define MULTIUSER_EXECUTIONLEVEL Highest
!define MULTIUSER_INSTALLMODE_COMMANDLINE
!define MULTIUSER_INSTALLMODE_INSTDIR "${NAME}"
!define MULTIUSER_INSTALLMODE_DEFAULT_REGISTRY_KEY "Software\${NAME}"
!define MULTIUSER_INSTALLMODE_DEFAULT_REGISTRY_VALUENAME "InstallMode"
!define MULTIUSER_INSTALLMODE_INSTDIR_REGISTRY_KEY "Software\${NAME}"
!define MULTIUSER_INSTALLMODE_INSTDIR_REGISTRY_VALUENAME "InstallPath"
!if ${ARCH} == "x86_64"
!define MULTIUSER_USE_PROGRAMFILES64
!endif
!define REG_UNINSTALL "Software\Microsoft\Windows\CurrentVersion\Uninstall\${NAME}"

!include "MUI2.nsh"
!include "MultiUser.nsh"
!include "x64.nsh"

;********** General **********
Name "${NAME} ${PROGVERSION}"
OutFile "${NAME}_${PROGVERSION}_${IFACE}_${ARCH}.exe"

;********** Functions **********
Function GetParent
  Exch $R0
  Push $R1
  Push $R2
  Push $R3
 
  StrCpy $R1 0
  StrLen $R2 $R0
 
  loop:
    IntOp $R1 $R1 + 1
    IntCmp $R1 $R2 get 0 get
    StrCpy $R3 $R0 1 -$R1
    StrCmp $R3 "\" get
  Goto loop
 
  get:
    StrCpy $R0 $R0 -$R1
 
    Pop $R3
    Pop $R2
    Pop $R1
    Exch $R0
FunctionEnd

Function .onInit
  ; Check if custom install dir specified
  Var /GLOBAL CUSTOMINSTDIR
  ${If} "$INSTDIR" != ""
    StrCpy $CUSTOMINSTDIR "$INSTDIR"
  ${EndIf}

  !insertmacro MULTIUSER_INIT

  ; MULTIUSER_INIT overrides $INSTDIR, restore if necessary
  ${If} "$CUSTOMINSTDIR" != ""
    StrCpy $INSTDIR "$CUSTOMINSTDIR"
  ${EndIf}
  
  InitPluginsDir

  Var /GLOBAL archMismatch
  StrCpy $archMismatch "false"
  ${IfNot} ${RunningX64}
    ${If} ${ARCH} != "i686"
    ${AndIf} ${ARCH} != "i686_debug"
      StrCpy $archMismatch "true"
    ${EndIf}
  ${EndIf}

  ${If} $archMismatch == "true"
    MessageBox MB_OK "The installer is not compatible with your system architecture."
    Abort
  ${EndIf}

  ; Remove previous versions before installing new one
  Var /GLOBAL UNINSTCMD
  ReadRegStr $UNINSTCMD HKCU "${REG_UNINSTALL}" "UninstallString"
  StrCmp "$UNINSTCMD" "" 0 uninst_query

  ReadRegStr $UNINSTCMD HKLM "${REG_UNINSTALL}" "UninstallString"
  StrCmp "$UNINSTCMD" "" done uninst_query

  uninst_query:
    MessageBox MB_OKCANCEL|MB_ICONEXCLAMATION \
      "${NAME} is already installed.$\n$\nTo remove the installed version, press 'OK'. To cancel the installation, press 'Cancel'." \
      IDOK uninst
    Abort

  uninst:
    ; Backup dictionaries and language definitions
    Push $UNINSTCMD
    Call GetParent ; Get dirname of path to uninstall.exe, i.e. the install root path
    Pop $R0
    StrCpy $1 $R0 "" 1 ; Remove leading quote
    CreateDirectory "$PLUGINSDIR\dicts"
    CreateDirectory "$PLUGINSDIR\tessdata"
    CopyFiles "$1\share\myspell\dicts\*.dic" "$PLUGINSDIR\dicts\"
    CopyFiles "$1\share\myspell\dicts\*.aff" "$PLUGINSDIR\dicts\"
    CopyFiles "$1\share\myspell\*.dic" "$PLUGINSDIR\dicts\"
    CopyFiles "$1\share\myspell\*.aff" "$PLUGINSDIR\dicts\"
    CopyFiles "$1\share\tessdata\*" "$PLUGINSDIR\tessdata\"

    ClearErrors
    ExecWait '$UNINSTCMD'
    IfErrors uninst_err done

  uninst_err:
    MessageBox MB_OKCANCEL|MB_ICONEXCLAMATION \
      "An error occured while uninstalling.$\n$\nTo continue regardless, press 'OK'. To cancel the installation, press 'Cancel'." \
      IDOK done
    Abort

  done:
FunctionEnd

Function .onInstSuccess
  ; Restore dictionaries and language definitions
  IfFileExists "$PLUGINSDIR\dicts" backupExists
  backupExists:
    CopyFiles "$PLUGINSDIR\dicts\*" "$INSTDIR\share\myspell\"
    CopyFiles "$PLUGINSDIR\tessdata\*" "$INSTDIR\share\tessdata\"
FunctionEnd

Function un.onInit 
  !insertmacro MULTIUSER_UNINIT
FunctionEnd

;********** Pages **********
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "..\..\COPYING"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MULTIUSER_PAGE_INSTALLMODE
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

; Uninstaller pages
!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

; Languages
!insertmacro MUI_LANGUAGE "English"

;********** Installation types **********
InstType "Standard"
InstType "Standard (localized)"
InstType /NOCUSTOM

Section "Standard" MainSection
  SectionIn 1 2

  ; Main program files
  SetOutPath "$INSTDIR"
  File /r /x locale /x translations "root\*.*"

  ; Uninstaller
  WriteRegStr SHCTX "${REG_UNINSTALL}" "DisplayName" "${NAME}"
  WriteRegStr SHCTX "${REG_UNINSTALL}" "Publisher" "Sandro Mani"
  WriteRegStr SHCTX "${REG_UNINSTALL}" "DisplayVersion" "${PROGVERSION}"
  WriteRegStr SHCTX "${REG_UNINSTALL}" "UninstallString" "$\"$INSTDIR\Uninstall.exe$\" /$MultiUser.InstallMode"
  WriteRegDWord SHCTX "${REG_UNINSTALL}" "NoModify" 1
  WriteRegDWord SHCTX "${REG_UNINSTALL}" "NoRepair" 1
  WriteUninstaller "$INSTDIR\Uninstall.exe"

  ; Create application shortcut (first in installation dir to have the correct "start in" target)
  SetOutPath "$INSTDIR\bin"
  CreateShortCut "$INSTDIR\bin\${NAME}.lnk" "$INSTDIR\bin\gimagereader-${IFACE}.exe"
  CreateShortCut "$INSTDIR\bin\Tesseract language definitions.lnk" "$INSTDIR\bin\gimagereader-${IFACE}.exe" tessdatadir
  CreateShortCut "$INSTDIR\bin\Spelling dictionaries.lnk" "$INSTDIR\bin\gimagereader-${IFACE}.exe" spellingdir

  ; Start menu entries
  SetOutPath "$SMPROGRAMS\${NAME}\"
  CopyFiles "$INSTDIR\bin\${NAME}.lnk" "$SMPROGRAMS\${NAME}\"
  CopyFiles "$INSTDIR\bin\Tesseract language definitions.lnk" "$SMPROGRAMS\${NAME}\"
  CopyFiles "$INSTDIR\bin\Spelling dictionaries.lnk" "$SMPROGRAMS\${NAME}\"
  Delete "$INSTDIR\bin\${NAME}.lnk"
  Delete "$INSTDIR\bin\Tesseract language definitions.lnk"
  Delete "$INSTDIR\bin\Spelling dictionaries.lnk"
  CreateShortCut "$SMPROGRAMS\${NAME}\Manual.lnk" "$INSTDIR\share\doc\gimagereader\manual.html"
  CreateShortCut "$SMPROGRAMS\${NAME}\Uninstall.lnk" "$INSTDIR\Uninstall.exe"
SectionEnd

Section "intl" IntlSection
  SectionIn 2

  SetOutPath "$INSTDIR\share\"
  File /r "root\share\locale"
  SetOutPath "$INSTDIR\share\qt4\"
  File /nonfatal /r "root\share\qt4\translations"
  SetOutPath "$INSTDIR\share\qt5\"
  File /nonfatal /r "root\share\qt5\translations"
SectionEnd

; Installation section language strings
LangString DESC_MainSection ${LANG_ENGLISH} "${NAME} files."
LangString DESC_IntlSection ${LANG_ENGLISH} "${NAME} translations."

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
!insertmacro MUI_DESCRIPTION_TEXT ${MainSection} $(DESC_MainSection)
!insertmacro MUI_DESCRIPTION_TEXT ${IntlSection} $(DESC_IntlSection)
!insertmacro MUI_FUNCTION_DESCRIPTION_END

;********** Uninstall **********
Section "Uninstall"
  !include "unfiles.nsi"
  Delete "$INSTDIR\Uninstall.exe"
  RmDir "$INSTDIR"
  RmDir /r "$SMPROGRAMS\${NAME}\"

  DeleteRegKey /ifempty SHCTX "Software\${NAME}"
  DeleteRegKey SHCTX "${REG_UNINSTALL}"
SectionEnd 
