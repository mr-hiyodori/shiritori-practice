# Shiritori Game
# Run this in PowerShell with: .\setup_windows.ps1

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  Shiritori Game  " -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Function to check if a command exists
function Test-CommandExists {
    param($command)
    $null = Get-Command $command -ErrorAction SilentlyContinue
    return $?
}

# Step 1: Check for Git
Write-Host "[1/7] Checking for Git..." -ForegroundColor Yellow
if (Test-CommandExists "git") {
    Write-Host "✓ Git is installed" -ForegroundColor Green
} else {
    Write-Host "✗ Git not found!" -ForegroundColor Red
    Write-Host "Please install Git from: https://git-scm.com/download/win" -ForegroundColor Yellow
    Write-Host "After installing, restart PowerShell and run this script again." -ForegroundColor Yellow
    exit 1
}

# Step 2: Check for Qt
Write-Host "`n[2/7] Checking for Qt..." -ForegroundColor Yellow
$qtPaths = @(
    "C:\Qt\6.5.0\mingw_64\bin",
    "C:\Qt\6.4.0\mingw_64\bin",
    "C:\Qt\6.3.0\mingw_64\bin",
    "C:\Qt\5.15.2\mingw81_64\bin",
    "C:\Qt\5.15.0\mingw81_64\bin"
)

$qtFound = $false
$qtPath = ""

foreach ($path in $qtPaths) {
    if (Test-Path "$path\qmake.exe") {
        $qtFound = $true
        $qtPath = $path
        break
    }
}

if ($qtFound) {
    Write-Host "✓ Qt found at: $qtPath" -ForegroundColor Green
    $env:PATH = "$qtPath;$env:PATH"
} else {
    Write-Host "✗ Qt not found!" -ForegroundColor Red
    Write-Host "Please install Qt from: https://www.qt.io/download-qt-installer" -ForegroundColor Yellow
    Write-Host "Recommended: Qt 6.5.0 with MinGW 64-bit" -ForegroundColor Yellow
    Write-Host "After installing, update the qtPaths in this script if needed." -ForegroundColor Yellow
    
    $response = Read-Host "Continue anyway? (y/n)"
    if ($response -ne "y") {
        exit 1
    }
}

# Step 3: Check for MinGW
Write-Host "`n[3/7] Checking for MinGW..." -ForegroundColor Yellow
$mingwPaths = @(
    "C:\Qt\Tools\mingw1120_64\bin",
    "C:\Qt\Tools\mingw1020_64\bin",
    "C:\Qt\Tools\mingw810_64\bin",
    "C:\mingw64\bin",
    "C:\MinGW\bin"
)

$mingwFound = $false
$mingwPath = ""

foreach ($path in $mingwPaths) {
    if (Test-Path "$path\g++.exe") {
        $mingwFound = $true
        $mingwPath = $path
        break
    }
}

if ($mingwFound) {
    Write-Host "✓ MinGW found at: $mingwPath" -ForegroundColor Green
    $env:PATH = "$mingwPath;$env:PATH"
} else {
    Write-Host "✗ MinGW not found!" -ForegroundColor Red
    Write-Host "MinGW is usually installed with Qt. Check C:\Qt\Tools\" -ForegroundColor Yellow
}

# Step 4: Create project directory
Write-Host "`n[4/7] Setting up project directory..." -ForegroundColor Yellow
$projectName = "Shiritori"
$projectPath = Join-Path (Get-Location) $projectName

if (Test-Path $projectPath) {
    Write-Host "Directory '$projectName' already exists." -ForegroundColor Yellow
    $response = Read-Host "Delete and recreate? (y/n)"
    if ($response -eq "y") {
        Remove-Item -Recurse -Force $projectPath
        Write-Host "✓ Removed existing directory" -ForegroundColor Green
    } else {
        Write-Host "Using existing directory..." -ForegroundColor Yellow
    }
}

if (-not (Test-Path $projectPath)) {
    New-Item -ItemType Directory -Path $projectPath | Out-Null
    Write-Host "✓ Created directory: $projectPath" -ForegroundColor Green
}

Set-Location $projectPath

# Step 5: Clone or setup files
Write-Host "`n[5/7] Setting up source files..." -ForegroundColor Yellow
Write-Host "Choose an option:" -ForegroundColor Cyan
Write-Host "  1. Clone from GitHub (https://github.com/mr-hiyodori/shiritori)" -ForegroundColor White
Write-Host "  2. I'll add files manually" -ForegroundColor White
$choice = Read-Host "Enter choice (1 or 2)"

if ($choice -eq "1") {
    $repoUrl = "https://github.com/mr-hiyodori/shiritori.git"
    
    Write-Host "Cloning repository from: $repoUrl" -ForegroundColor Yellow
    git clone $repoUrl .
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "✓ Repository cloned successfully" -ForegroundColor Green
    } else {
        Write-Host "✗ Failed to clone repository" -ForegroundColor Red
        exit 1
    }
} else {
    Write-Host "Please add your source files to: $projectPath" -ForegroundColor Yellow
    Write-Host "Required files:" -ForegroundColor Cyan
    Write-Host "  - main.cpp" -ForegroundColor White
    Write-Host "  - shiritori.pro" -ForegroundColor White
    Write-Host "  - gamecontroller.h/cpp" -ForegroundColor White
    Write-Host "  - shiritorigame.h/cpp" -ForegroundColor White
    Write-Host "  - main.qml" -ForegroundColor White
    Write-Host "  - resources.qrc" -ForegroundColor White
    Write-Host "  - Assets/ folder with images and sounds" -ForegroundColor White
    Write-Host "  - Dictionary/ folder with .txt files" -ForegroundColor White
    
    $response = Read-Host "`nPress Enter when files are ready..."
}

# Step 6: Create Assets and Dictionary folders if they don't exist
Write-Host "`n[6/7] Checking project structure..." -ForegroundColor Yellow

$folders = @("Assets", "Dictionary")
foreach ($folder in $folders) {
    $folderPath = Join-Path $projectPath $folder
    if (-not (Test-Path $folderPath)) {
        New-Item -ItemType Directory -Path $folderPath | Out-Null
        Write-Host "✓ Created folder: $folder" -ForegroundColor Green
        
        if ($folder -eq "Assets") {
            Write-Host "  Please add these files to Assets/:" -ForegroundColor Yellow
            Write-Host "    - player_avatar.jpg" -ForegroundColor White
            Write-Host "    - AI_avatar.png" -ForegroundColor White
            Write-Host "    - heart.png" -ForegroundColor White
            Write-Host "    - upload.png" -ForegroundColor White
            Write-Host "    - word_chain.png" -ForegroundColor White
            Write-Host "    - word_choice.png" -ForegroundColor White
            Write-Host "    - type_sound.wav" -ForegroundColor White
            Write-Host "    - ran_out_of_time.wav" -ForegroundColor White
            Write-Host "    - submit.wav" -ForegroundColor White
            Write-Host "    - top_sound.wav" -ForegroundColor White
        } elseif ($folder -eq "Dictionary") {
            Write-Host "  Please add these files to Dictionary/:" -ForegroundColor Yellow
            Write-Host "    - last_letter.txt (word dictionary)" -ForegroundColor White
            Write-Host "    - rare_prefix.txt (patterns file)" -ForegroundColor White
        }
    } else {
        Write-Host "✓ Folder exists: $folder" -ForegroundColor Green
    }
}

# Step 7: Build the project
Write-Host "`n[7/7] Building project..." -ForegroundColor Yellow

if (-not (Test-Path "shiritori.pro")) {
    Write-Host "✗ shiritori.pro not found!" -ForegroundColor Red
    Write-Host "Please ensure all source files are in place." -ForegroundColor Yellow
    exit 1
}

Write-Host "Running qmake..." -ForegroundColor Yellow
qmake shiritori.pro

if ($LASTEXITCODE -ne 0) {
    Write-Host "✗ qmake failed!" -ForegroundColor Red
    Write-Host "Make sure Qt is properly installed and in PATH." -ForegroundColor Yellow
    exit 1
}

Write-Host "✓ qmake completed" -ForegroundColor Green

Write-Host "`nRunning make..." -ForegroundColor Yellow
mingw32-make

if ($LASTEXITCODE -eq 0) {
    Write-Host "✓ Build successful!" -ForegroundColor Green
    Write-Host "`n========================================" -ForegroundColor Cyan
    Write-Host "  Setup Complete!" -ForegroundColor Green
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host "`nTo run the game:" -ForegroundColor Yellow
    Write-Host "  .\release\Shiritori.exe" -ForegroundColor White
    Write-Host "`nOr in Qt Creator:" -ForegroundColor Yellow
    Write-Host "  1. Open shiritori.pro" -ForegroundColor White
    Write-Host "  2. Configure project with MinGW kit" -ForegroundColor White
    Write-Host "  3. Press Ctrl+R to build and run" -ForegroundColor White
} else {
    Write-Host "✗ Build failed!" -ForegroundColor Red
    Write-Host "Check the error messages above." -ForegroundColor Yellow
    Write-Host "`nCommon issues:" -ForegroundColor Yellow
    Write-Host "  - Missing Qt modules (add 'qml' to shiritori.pro)" -ForegroundColor White
    Write-Host "  - Wrong Qt version" -ForegroundColor White
    Write-Host "  - Missing source files" -ForegroundColor White
}

Write-Host "`nPress any key to exit..."
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
