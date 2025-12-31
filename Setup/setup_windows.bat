@echo off
setlocal enabledelayedexpansion

:: Shiritori Game - Windows Setup Script
:: Save this as: setup_shiritori.bat
:: Run as Administrator for best results

color 0B
echo ========================================
echo   Shiritori Game Setup for Windows
echo ========================================
echo.

:: Check if running as admin
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo [WARNING] Not running as Administrator
    echo Some installations may fail without admin rights
    echo.
    pause
)

:: ===========================================
:: STEP 1: Check and Install Git
:: ===========================================
echo [1/5] Checking for Git...
git --version >nul 2>&1
if %errorLevel% equ 0 (
    echo [OK] Git is installed
    git --version
) else (
    echo [!] Git not found!
    echo.
    echo Installing Git using winget...
    winget install --id Git.Git -e --source winget >nul 2>&1
    
    if %errorLevel% neq 0 (
        echo [ERROR] Automatic installation failed
        echo.
        echo Please install Git manually:
        echo 1. Download from: https://git-scm.com/download/win
        echo 2. Run the installer
        echo 3. Restart this script
        echo.
        start https://git-scm.com/download/win
        pause
        exit /b 1
    )
    
    echo [OK] Git installed successfully
    echo Please close and reopen this terminal, then run this script again
    pause
    exit /b 0
)

:: ===========================================
:: STEP 2: Check for Qt
:: ===========================================
echo.
echo [2/5] Checking for Qt...

:: Common Qt installation paths
set "QT_FOUND=0"
set "QT_PATH="

for %%V in (6.8 6.7 6.6 6.5 6.4 6.3 5.15) do (
    if exist "C:\Qt\%%V.0\mingw_64\bin\qmake.exe" (
        set "QT_PATH=C:\Qt\%%V.0\mingw_64\bin"
        set "QT_FOUND=1"
        goto :qt_found
    )
    if exist "C:\Qt\%%V\mingw_64\bin\qmake.exe" (
        set "QT_PATH=C:\Qt\%%V\mingw_64\bin"
        set "QT_FOUND=1"
        goto :qt_found
    )
)

:qt_found
if %QT_FOUND% equ 1 (
    echo [OK] Qt found at: !QT_PATH!
    set "PATH=!QT_PATH!;!PATH!"
) else (
    echo [!] Qt not found!
    echo.
    echo Qt cannot be installed automatically.
    echo.
    echo Please install Qt manually:
    echo 1. Download Qt Online Installer: https://www.qt.io/download-qt-installer
    echo 2. During installation, select:
    echo    - Qt 6.5 or higher
    echo    - MinGW 64-bit compiler
    echo    - Qt Quick components
    echo 3. Install to C:\Qt (default location)
    echo 4. Restart this script
    echo.
    start https://www.qt.io/download-qt-installer
    pause
    exit /b 1
)

:: ===========================================
:: STEP 3: Check for MinGW
:: ===========================================
echo.
echo [3/5] Checking for MinGW...

set "MINGW_FOUND=0"
set "MINGW_PATH="

:: Check Qt Tools directory
for %%D in (mingw1120_64 mingw1020_64 mingw900_64 mingw810_64 mingw730_64) do (
    if exist "C:\Qt\Tools\%%D\bin\g++.exe" (
        set "MINGW_PATH=C:\Qt\Tools\%%D\bin"
        set "MINGW_FOUND=1"
        goto :mingw_found
    )
)

:: Check standalone MinGW
if exist "C:\mingw64\bin\g++.exe" (
    set "MINGW_PATH=C:\mingw64\bin"
    set "MINGW_FOUND=1"
)

:mingw_found
if %MINGW_FOUND% equ 1 (
    echo [OK] MinGW found at: !MINGW_PATH!
    set "PATH=!MINGW_PATH!;!PATH!"
) else (
    echo [!] MinGW not found!
    echo MinGW is usually installed with Qt in C:\Qt\Tools\
    echo Please reinstall Qt and ensure MinGW is selected
    pause
    exit /b 1
)

:: ===========================================
:: STEP 4: Clone Repository
:: ===========================================
echo.
echo [4/5] Setting up project...

set "PROJECT_DIR=Shiritori"

if exist "%PROJECT_DIR%" (
    echo Directory already exists: %PROJECT_DIR%
    choice /C YN /M "Delete and re-clone"
    if !errorlevel! equ 1 (
        echo Removing existing directory...
        rmdir /s /q "%PROJECT_DIR%"
    ) else (
        echo Using existing directory...
        cd "%PROJECT_DIR%"
        goto :skip_clone
    )
)

echo Cloning repository from GitHub...
git clone https://github.com/mr-hiyodori/shiritori.git "%PROJECT_DIR%"

if %errorLevel% neq 0 (
    echo [ERROR] Failed to clone repository
    echo Check your internet connection and try again
    pause
    exit /b 1
)

echo [OK] Repository cloned successfully
cd "%PROJECT_DIR%"

:skip_clone

:: ===========================================
:: STEP 5: Build Project
:: ===========================================
echo.
echo [5/5] Building project...

if not exist "shiritori.pro" (
    echo [ERROR] shiritori.pro not found!
    echo Make sure you're in the correct directory
    pause
    exit /b 1
)

echo Running qmake...
qmake shiritori.pro

if %errorLevel% neq 0 (
    echo [ERROR] qmake failed!
    echo Check that Qt is properly installed
    pause
    exit /b 1
)

echo [OK] qmake completed

echo.
echo Running mingw32-make (this may take a few minutes)...
mingw32-make

if %errorLevel% equ 0 (
    echo.
    echo ========================================
    echo   BUILD SUCCESSFUL!
    echo ========================================
    echo.
    echo Game compiled successfully!
    echo.
    echo To run the game:
    echo   1. Navigate to: %cd%\release
    echo   2. Run: Shiritori.exe
    echo.
    echo Or double-click: release\Shiritori.exe
    echo.
    
    choice /C YN /M "Do you want to run the game now"
    if !errorlevel! equ 1 (
        if exist "release\Shiritori.exe" (
            start "" "release\Shiritori.exe"
        ) else if exist "debug\Shiritori.exe" (
            start "" "debug\Shiritori.exe"
        ) else (
            echo Game executable not found in expected location
        )
    )
) else (
    echo.
    echo [ERROR] Build failed!
    echo.
    echo Common issues:
    echo - Missing Qt modules
    echo - Incorrect Qt version
    echo - Missing source files
    echo - Check error messages above
    echo.
    pause
    exit /b 1
)

echo.
pause
