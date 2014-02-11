SetCompressor /SOLID /FINAL lzma

!define MUI_ABORTWARNING
!define MUI_ICON "gimagereader.ico"
!define MULTIUSER_MUI
!define MULTIUSER_EXECUTIONLEVEL Highest
!define MULTIUSER_INSTALLMODE_COMMANDLINE
!define MULTIUSER_INSTALLMODE_DEFAULT_REGISTRY_KEY "Software\${NAME}"
!define MULTIUSER_INSTALLMODE_DEFAULT_REGISTRY_VALUENAME "InstallMode"
!define MULTIUSER_INSTALLMODE_INSTDIR_REGISTRY_KEY "Software\${NAME}"
!define MULTIUSER_INSTALLMODE_INSTDIR_REGISTRY_VALUENAME "InstallPath"
!define MULTIUSER_INSTALLMODE_INSTDIR "${NAME}"
!define REG_UNINSTALL "Software\Microsoft\Windows\CurrentVersion\Uninstall\${NAME}"

!include "MUI2.nsh"
!include "MultiUser.nsh"

;********** General **********
Name "${NAME} ${PROGVERSION}"
OutFile "${NAME}_${PROGVERSION}.exe"

;********** Functions **********
Function .onInit
  !insertmacro MULTIUSER_INIT

  ; Remove previous versions before installing new one
  ReadRegStr $R0 SHCTX "${REG_UNINSTALL}" "UninstallString"
  StrCmp $R0 "" done

  MessageBox MB_OKCANCEL|MB_ICONEXCLAMATION \
    "${NAME} is already installed.$\n$\nTo remove the installed version, press 'OK'. To cancel the installation, press 'Cancel'." \
    IDOK uninst
  Abort

  uninst:
    ClearErrors
    ExecWait '$R0 _?=$INSTDIR'
    IfErrors uninst_err done

  uninst_err:
    MessageBox MB_OKCANCEL|MB_ICONEXCLAMATION \
      "An error occured while uninstalling.$\n$\nTo continue regardless, press 'OK'. To cancel the installation, press 'Cancel'." \
      IDOK done
    Abort

  done:
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
InstType "Standard (intl)"
InstType /NOCUSTOM

Section "Standard" MainSection
  SectionIn 1 2

  ; Main program files
  SetOutPath "$INSTDIR"
  File /r /x locale "root/*.*"

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
  CreateShortCut "$INSTDIR\bin\${NAME}.lnk" "$INSTDIR\bin\gimagereader.exe"

  ; Start menu entries
  SetOutPath "$SMPROGRAMS\${NAME}\"
  CopyFiles "$INSTDIR\bin\${NAME}.lnk" "$SMPROGRAMS\${NAME}\"
  Delete "$INSTDIR\bin\${NAME}.lnk"
  CreateShortCut "$SMPROGRAMS\${NAME}\Manual.lnk" "$INSTDIR\share\gimagereader\manual.html"
  CreateShortCut "$SMPROGRAMS\${NAME}\Spelling dictionaries.lnk" "$INSTDIR\share\myspell\dicts\"
  CreateShortCut "$SMPROGRAMS\${NAME}\Tesseract language definitions.lnk" "$INSTDIR\share\tessdata\"
  CreateShortCut "$SMPROGRAMS\${NAME}\Uninstall.lnk" "$INSTDIR\Uninstall.exe"
SectionEnd

Section "intl" IntlSection
  SectionIn 2

  SetOutPath "$INSTDIR"
  File /r "root/share/locale"
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
  Delete "$INSTDIR\*.*"
  RmDir /r "$SMPROGRAMS\${NAME}\"
  RmDir /r "$INSTDIR"

  DeleteRegKey /ifempty SHCTX "Software\${NAME}"
  DeleteRegKey SHCTX "${REG_UNINSTALL}"
SectionEnd 
