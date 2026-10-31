# Instala lo necesario para compilar Hexa Frontera: Visual Studio 2022 Community con las cargas
# de C++ para juegos, el Epic Games Launcher y Git LFS. Usa winget (viene con Windows 10/11).
# Uso, en PowerShell como administrador:
#   powershell -ExecutionPolicy Bypass -File Tools\windows\instalar_requisitos.ps1
# Unreal Engine no se puede instalar por línea de comandos: se instala desde el Epic Games Launcher.

$ErrorActionPreference = 'Stop'

$admin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $admin) {
    Write-Host "Abrí PowerShell como administrador (clic derecho > Ejecutar como administrador) y volvé a correr el script." -ForegroundColor Red
    exit 1
}
if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
    Write-Host "Falta winget. Instalá 'Instalador de aplicación' desde Microsoft Store y volvé a intentar." -ForegroundColor Red
    exit 1
}

$cargas = @(
    'Microsoft.VisualStudio.Workload.NativeGame',     # Desarrollo de juegos con C++ (incluye el instalador de Unreal Engine)
    'Microsoft.VisualStudio.Workload.NativeDesktop',  # Desarrollo para el escritorio con C++
    'Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
    'Microsoft.VisualStudio.Component.Windows11SDK.22621'
)
$argumentosVS = (($cargas | ForEach-Object { "--add $_" }) -join ' ') + ' --includeRecommended --passive --norestart'

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath = if (Test-Path $vswhere) { & $vswhere -version '[17.0,18.0)' -products * -property installationPath | Select-Object -First 1 }

Write-Host "`n1/3 Visual Studio 2022 con C++ para juegos (descarga ~10 GB, puede tardar bastante)..." -ForegroundColor Cyan
if ($vsPath) {
    # Ya está instalado: solo agrega las cargas que falten.
    $instalador = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\setup.exe"
    Start-Process -FilePath $instalador -ArgumentList "modify --installPath `"$vsPath`" $argumentosVS" -Wait
} else {
    winget install --id Microsoft.VisualStudio.2022.Community -e --source winget --accept-package-agreements --accept-source-agreements --override $argumentosVS
}

Write-Host "`n2/3 Epic Games Launcher..." -ForegroundColor Cyan
winget install --id EpicGames.EpicGamesLauncher -e --source winget --accept-package-agreements --accept-source-agreements

Write-Host "`n3/3 Git y Git LFS..." -ForegroundColor Cyan
winget install --id Git.Git -e --source winget --accept-package-agreements --accept-source-agreements
winget install --id GitHub.GitLFS -e --source winget --accept-package-agreements --accept-source-agreements

Write-Host "`nListo. Falta un paso manual:" -ForegroundColor Green
Write-Host "  1. Abrí el Epic Games Launcher e iniciá sesión (la cuenta es gratis)."
Write-Host "  2. Unreal Engine > Biblioteca > botón + > elegí 5.4 o posterior > Instalar (~60 GB)."
Write-Host "  3. Reiniciá la PC y corré Tools\windows\revisar_pc.ps1 para confirmar."
Read-Host "`nApretá Enter para cerrar" | Out-Null
