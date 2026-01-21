; VEIL VPN Windows Installer Script (NSIS)
;
; This script creates a Windows installer for VEIL VPN client.
; It handles:
; - Installing application files
; - Installing Wintun driver
; - Creating Start Menu shortcuts
; - Creating Desktop shortcut
; - Registering the Windows service
; - Setting up uninstaller
;
; Build with: makensis veil-vpn.nsi

!include "MUI2.nsh"
!include "FileFunc.nsh"
!include "x64.nsh"
!include "LogicLib.nsh"

; ============================================================================
; General Configuration
; ============================================================================

!define PRODUCT_NAME "VEIL VPN"
!define PRODUCT_VERSION "1.0.0"
!define PRODUCT_PUBLISHER "VEIL Project"
!define PRODUCT_WEB_SITE "https://github.com/VisageDvachevsky/veil-win-client"
!define PRODUCT_DIR_REGKEY "Software\Microsoft\Windows\CurrentVersion\App Paths\veil-client-gui.exe"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
!define PRODUCT_UNINST_ROOT_KEY "HKLM"

; Installer attributes
Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"
OutFile "veil-vpn-${PRODUCT_VERSION}-setup.exe"
InstallDir "$PROGRAMFILES64\VEIL VPN"
InstallDirRegKey HKLM "${PRODUCT_DIR_REGKEY}" ""
RequestExecutionLevel admin
ShowInstDetails show
ShowUnInstDetails show

; Compression
SetCompressor /SOLID lzma

; ============================================================================
; Modern UI Configuration
; ============================================================================

!define MUI_ABORTWARNING
!define MUI_ICON "${NSISDIR}\Contrib\Graphics\Icons\modern-install.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico"

; Welcome page
!insertmacro MUI_PAGE_WELCOME

; License page
!insertmacro MUI_PAGE_LICENSE "LICENSE.txt"

; Directory page
!insertmacro MUI_PAGE_DIRECTORY

; Components page
!insertmacro MUI_PAGE_COMPONENTS

; Install files page
!insertmacro MUI_PAGE_INSTFILES

; Finish page
!define MUI_FINISHPAGE_RUN "$INSTDIR\veil-client-gui.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Launch ${PRODUCT_NAME}"
!insertmacro MUI_PAGE_FINISH

; Uninstaller pages
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

; Languages
!insertmacro MUI_LANGUAGE "English"

; ============================================================================
; Installer Sections
; ============================================================================

Section "VEIL VPN Client (required)" SecMain
  SectionIn RO

  SetOutPath "$INSTDIR"
  SetOverwrite on

  ; Main application files
  File "bin\veil-client-gui.exe"
  ; Note: veil-client.exe and veil-service.exe are not built on Windows
  ; They require the transport layer which is currently Linux-only

  ; Qt DLLs (if not using static build)
  File /nonfatal "bin\Qt6Core.dll"
  File /nonfatal "bin\Qt6Gui.dll"
  File /nonfatal "bin\Qt6Widgets.dll"
  File /nonfatal "bin\Qt6Network.dll"

  ; Qt plugins
  SetOutPath "$INSTDIR\platforms"
  File /nonfatal "bin\platforms\qwindows.dll"

  SetOutPath "$INSTDIR\styles"
  File /nonfatal "bin\styles\qwindowsvistastyle.dll"

  ; Other dependencies
  SetOutPath "$INSTDIR"
  File /nonfatal "bin\libsodium.dll"
  File /nonfatal "bin\spdlog.dll"
  File /nonfatal "bin\fmt.dll"

  ; Documentation
  SetOutPath "$INSTDIR\docs"
  File "docs\README.md"
  File /nonfatal "docs\CHANGELOG.md"

  ; Configuration templates
  SetOutPath "$INSTDIR\config"
  File /nonfatal "config\client.json.example"

  ; Translations
  SetOutPath "$INSTDIR\translations"
  File /nonfatal "translations\veil_*.qm"

  ; Write installation directory to registry
  WriteRegStr HKLM "${PRODUCT_DIR_REGKEY}" "" "$INSTDIR\veil-client-gui.exe"

  ; Create uninstaller
  WriteUninstaller "$INSTDIR\uninstall.exe"

  ; Add/Remove Programs entry
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayName" "${PRODUCT_NAME}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "UninstallString" "$INSTDIR\uninstall.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayIcon" "$INSTDIR\veil-client-gui.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "URLInfoAbout" "${PRODUCT_WEB_SITE}"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "Publisher" "${PRODUCT_PUBLISHER}"

  ; Calculate and store installed size
  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  IntFmt $0 "0x%08X" $0
  WriteRegDWORD ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "EstimatedSize" "$0"

SectionEnd

Section "Wintun Driver" SecWintun
  SetOutPath "$INSTDIR\driver"

  ; Download Wintun if not bundled
  ; For production, the driver should be bundled
  File "driver\wintun.dll"

  ; Copy to System32 for system-wide access
  ${If} ${RunningX64}
    CopyFiles /SILENT "$INSTDIR\driver\wintun.dll" "$SYSDIR\wintun.dll"
  ${EndIf}

SectionEnd

; Windows Service section disabled - veil-service.exe is not built on Windows yet
; Section "Windows Service" SecService
;   ; Install the Windows service
;   DetailPrint "Installing VEIL VPN Service..."
;   nsExec::ExecToLog '"$INSTDIR\veil-service.exe" --install'
;   Pop $0
;
;   ${If} $0 != 0
;     DetailPrint "Warning: Failed to install service (error code: $0)"
;     MessageBox MB_OK|MB_ICONEXCLAMATION "Failed to install Windows service. You may need to install it manually using Administrator privileges."
;   ${Else}
;     DetailPrint "Service installed successfully"
;
;     ; Start the service
;     DetailPrint "Starting VEIL VPN Service..."
;     nsExec::ExecToLog '"$INSTDIR\veil-service.exe" --start'
;     Pop $0
;   ${EndIf}
;
; SectionEnd

Section "Start Menu Shortcuts" SecStartMenu
  CreateDirectory "$SMPROGRAMS\${PRODUCT_NAME}"
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\${PRODUCT_NAME}.lnk" "$INSTDIR\veil-client-gui.exe"
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\Uninstall.lnk" "$INSTDIR\uninstall.exe"
SectionEnd

Section "Desktop Shortcut" SecDesktop
  CreateShortCut "$DESKTOP\${PRODUCT_NAME}.lnk" "$INSTDIR\veil-client-gui.exe"
SectionEnd

Section "Auto-start with Windows" SecAutoStart
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "${PRODUCT_NAME}" "$INSTDIR\veil-client-gui.exe --minimized"
SectionEnd

; ============================================================================
; Section Descriptions
; ============================================================================

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SecMain} "Core application files (required)"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecWintun} "Wintun network driver for VPN connectivity"
  ; SecService disabled - service not built on Windows yet
  ; !insertmacro MUI_DESCRIPTION_TEXT ${SecService} "Install and start the VPN background service"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecStartMenu} "Create Start Menu shortcuts"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecDesktop} "Create Desktop shortcut"
  !insertmacro MUI_DESCRIPTION_TEXT ${SecAutoStart} "Automatically start VEIL VPN when Windows starts"
!insertmacro MUI_FUNCTION_DESCRIPTION_END

; ============================================================================
; Uninstaller Section
; ============================================================================

Section "Uninstall"
  ; Note: Service removal disabled - veil-service.exe is not built on Windows yet
  ; Stop and remove the service
  ; DetailPrint "Stopping VEIL VPN Service..."
  ; nsExec::ExecToLog '"$INSTDIR\veil-service.exe" --stop'
  ;
  ; DetailPrint "Uninstalling VEIL VPN Service..."
  ; nsExec::ExecToLog '"$INSTDIR\veil-service.exe" --uninstall'

  ; Remove files
  Delete "$INSTDIR\veil-client-gui.exe"
  ; Note: veil-client.exe and veil-service.exe are not built on Windows
  ; Delete "$INSTDIR\veil-client.exe"
  ; Delete "$INSTDIR\veil-service.exe"
  Delete "$INSTDIR\Qt6Core.dll"
  Delete "$INSTDIR\Qt6Gui.dll"
  Delete "$INSTDIR\Qt6Widgets.dll"
  Delete "$INSTDIR\Qt6Network.dll"
  Delete "$INSTDIR\libsodium.dll"
  Delete "$INSTDIR\spdlog.dll"
  Delete "$INSTDIR\fmt.dll"
  Delete "$INSTDIR\uninstall.exe"

  ; Remove directories
  RMDir /r "$INSTDIR\platforms"
  RMDir /r "$INSTDIR\styles"
  RMDir /r "$INSTDIR\docs"
  RMDir /r "$INSTDIR\config"
  RMDir /r "$INSTDIR\driver"
  RMDir /r "$INSTDIR\translations"
  RMDir "$INSTDIR"

  ; Remove shortcuts
  Delete "$SMPROGRAMS\${PRODUCT_NAME}\${PRODUCT_NAME}.lnk"
  Delete "$SMPROGRAMS\${PRODUCT_NAME}\Uninstall.lnk"
  RMDir "$SMPROGRAMS\${PRODUCT_NAME}"
  Delete "$DESKTOP\${PRODUCT_NAME}.lnk"

  ; Remove registry entries
  DeleteRegKey ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}"
  DeleteRegKey HKLM "${PRODUCT_DIR_REGKEY}"
  DeleteRegValue HKCU "Software\Microsoft\Windows\CurrentVersion\Run" "${PRODUCT_NAME}"

  ; Remove Wintun from System32 (only if we installed it)
  ${If} ${RunningX64}
    Delete "$SYSDIR\wintun.dll"
  ${EndIf}

  SetAutoClose true

SectionEnd

; ============================================================================
; Installer Functions
; ============================================================================

Function .onInit
  ; Check for admin rights
  UserInfo::GetAccountType
  Pop $0
  StrCmp $0 "Admin" +3
    MessageBox MB_OK|MB_ICONSTOP "Administrator privileges are required to install ${PRODUCT_NAME}."
    Abort

FunctionEnd

Function un.onInit
  MessageBox MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2 "Are you sure you want to completely remove ${PRODUCT_NAME}?" IDYES +2
  Abort
FunctionEnd

Function un.onUninstSuccess
  HideWindow
  MessageBox MB_ICONINFORMATION|MB_OK "${PRODUCT_NAME} has been successfully removed from your computer."
FunctionEnd
