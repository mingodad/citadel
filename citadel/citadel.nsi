; $Id$
; NOTE: this .NSI script is designed for NSIS v2.0b0+
; Get NSIS at http://www.nullsoft.com/

!include "${NSISDIR}\Contrib\Modern UI\System.nsh"
!define MUI_PRODUCT "Citadel/UX"
!define MUI_VERSION "6.06"
!define MUI_WELCOMEPAGE
!define MUI_LICENSEPAGE
!define MUI_COMPONENTSPAGE
!define MUI_COMPONENTSPAGE_SMALLDESC
!define MUI_DIRECTORYPAGE
!define MUI_FINISHPAGE
!define MUI_UNINSTALLER
!define MUI_UNCONFIRMPAGE
!insertmacro MUI_LANGUAGE "English"
;!define MUI_UI "${NSISDIR}\Contrib\UIs\modern3.exe"
;!define MUI_ICON "${NSISDIR}\Contrib\Icons\modern-install.ico"
;!define MUI_UNICON "${NSISDIR}\Contrib\Icons\modern-uninstall.ico"

OutFile "citadel-6.06.exe"
BGGradient off

LangString $DESC_Citadel ${LANG_ENGLISH} "Citadel/UX client and core libraries (required)"
LangString $DESC_CitadelServer ${LANG_ENGLISH} "Citadel/UX server"
LangString $DESC_CitadelUtils ${LANG_ENGLISH} "Citadel/UX utilities"

SetCompress auto
SetDatablockOptimize on
BrandingText "Citadel/UX "
CRCCheck force
AutoCloseWindow true
ShowInstDetails hide
ShowUninstDetails hide
SetDateSave on

LicenseData "C:\cygwin\home\error\copying.txt"

InstallDir "$PROGRAMFILES\Citadel"
InstallDirRegKey HKEY_LOCAL_MACHINE "SOFTWARE\Citadel\Citadel\CurrentVersion" "InstallDir"
DirShow show

Section "Citadel/UX client and core libraries (required)" Citadel ; (default section)
SetShellVarContext all
SetOutPath "$INSTDIR"
File C:\cygwin\home\error\cvs\citadel\citadel.exe
File C:\cygwin\home\error\citadel.rc
File C:\cygwin\bin\libW11.dll
File C:\cygwin\bin\cygwin1.dll
File C:\cygwin\bin\cygcrypto-0.9.7.dll
File C:\cygwin\bin\cygncurses7.dll
File C:\cygwin\bin\cygssl-0.9.7.dll
File C:\cygwin\bin\rxvt.exe
Delete "$DESKTOP\Citadel.lnk"
WriteUninstaller "$INSTDIR\uninst.exe"
CreateShortCut "$DESKTOP\Citadel.lnk" \
	"$INSTDIR\rxvt.exe" "-fg white -bg black -sl 1000 -sr -fn 8x16 -e ./citadel.exe" \
	"telnet.exe" "0"
CreateDirectory "$SMPROGRAMS\Citadel"
CreateShortcut "$SMPROGRAMS\Citadel\Citadel.lnk" \
	"$INSTDIR\rxvt.exe" "-fg white -bg black -sl 1000 -sr -fn 8x16 -e ./citadel.exe" \
	"telnet.exe" "0"
WriteRegStr HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Citadel" "DisplayName" "Citadel (remove only)"
WriteRegStr HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Citadel" "UninstallString" '"$INSTDIR\uninst.exe"'
WriteRegStr HKEY_LOCAL_MACHINE "SOFTWARE\Citadel\Citadel\CurrentVersion" "InstallDir" "$INSTDIR"
SectionEnd ; end of default section

Section "Citadel/UX Server" CitadelServer
SetOutPath "$INSTDIR"
File C:\cygwin\home\error\cvs\citadel\citserver.exe
File C:\cygwin\home\error\cvs\citadel\base64.exe
File C:\cygwin\home\error\cvs\citadel\weekly
File C:\cygwin\home\error\cvs\citadel\setup.exe
File /r C:\cygwin\home\error\cvs\citadel\bitbucket
File /r C:\cygwin\home\error\cvs\citadel\help
File /r C:\cygwin\home\error\cvs\citadel\messages
WriteRegStr HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows\CurrentVersion\RunServices" "Citadel" '"$INSTDIR\citserver.exe" -x9 -t"$INSTDIR\citadel-debug.txt" -h"$INSTDIR"'
SectionEnd

Section "Citadel/UX Utilities" CitadelUtils
SetOutPath $INSTDIR
File C:\cygwin\home\error\cvs\citadel\aidepost.exe
File C:\cygwin\home\error\cvs\citadel\citmail.exe
File C:\cygwin\home\error\cvs\citadel\migratenet.exe
File C:\cygwin\home\error\cvs\citadel\msgform.exe
File C:\cygwin\home\error\cvs\citadel\sendcommand.exe
File C:\cygwin\home\error\cvs\citadel\userlist.exe
File C:\cygwin\home\error\cvs\citadel\whobbs.exe
SectionEnd
!insertmacro MUI_SECTIONS_FINISHHEADER

!insertmacro MUI_FUNCTIONS_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${Citadel} ${DESC_Citadel}
  !insertmacro MUI_DESCRIPTION_TEXT ${CitadelServer} ${DESC_CitadelServer}
  !insertmacro MUI_DESCRIPTION_TEXT ${CitadelFiles} $(DESC_CitadelFiles}
  !insertmacro MUI_DESCRIPTION_TEXT ${CitadelUtils} $(DESC_CitadelUtils}
!insertmacro MUI_FUNCTIONS_DESCRIPTION_END

; begin uninstall settings/section

Section Uninstall
SetShellVarContext all
SetDetailsView hide
SetAutoClose false
Delete "$INSTDIR\uninst.exe"
Delete /rebootok "$INSTDIR\citadel.exe"
Delete /rebootok "$INSTDIR\libW11.dll"
Delete /rebootok "$INSTDIR\cygwin1.dll"
Delete /rebootok "$INSTDIR\cygcrypto-0.9.7.dll"
Delete /rebootok "$INSTDIR\cygncurses7.dll"
Delete /rebootok "$INSTDIR\cygssl-0.9.7.dll"
Delete /rebootok "$INSTDIR\rxvt.exe"
Delete /rebootok "$INSTDIR\citserver.exe"
Delete /rebootok "$INSTDIR\base64.exe"
Delete /rebootok "$INSTDIR\weekly"
Delete /rebootok "$INSTDIR\setup.exe"
Delete /rebootok "$INSTDIR\aidepost.exe"
Delete /rebootok "$INSTDIR\citmail.exe"
Delete /rebootok "$INSTDIR\migratenet.exe"
Delete /rebootok "$INSTDIR\msgform.exe"
Delete /rebootok "$INSTDIR\sendcommand.exe"
Delete /rebootok "$INSTDIR\userlist.exe"
Delete /rebootok "$INSTDIR\whobbs.exe"

RMDir /r "$INSTDIR\help"
RMDir /r "$INSTDIR\messages"
RMDir /r "$INSTDIR\bitbucket"
RMDir "$INSTDIR"

Delete "$SMPROGRAMS\Citadel\Citadel.lnk"
RMDir "$SMPROGRAMS\Citadel"
Delete "$DESKTOP\Citadel.lnk"
DeleteRegValue HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows\CurrentVersion\RunServices" "Citadel"
DeleteRegKey HKEY_LOCAL_MACHINE "SOFTWARE\Citadel\Citadel"
DeleteRegKey HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Citadel"
DeleteRegValue HKEY_LOCAL_MACHINE "SOFTWARE\Citadel\Citadel\CurrentVersion" "InstallDir"
DeleteRegKey /ifempty HKEY_LOCAL_MACHINE "SOFTWARE\Citadel\Citadel\CurrentVersion"
DeleteRegKey /ifempty HKEY_LOCAL_MACHINE "SOFTWARE\Citadel\Citadel"
DeleteRegKey /ifempty HKEY_LOCAL_MACHINE "SOFTWARE\Citadel"
!insertmacro MUI_UNFINISHHEADER
SectionEnd ; end of uninstall section

; eof
