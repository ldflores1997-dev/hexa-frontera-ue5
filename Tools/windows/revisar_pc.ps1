# Revisa si esta PC tiene lo necesario para compilar y abrir Hexa Frontera (Unreal Engine 5 + C++).
# Uso: clic derecho > "Ejecutar con PowerShell", o en PowerShell:
#   powershell -ExecutionPolicy Bypass -File Tools\windows\revisar_pc.ps1
# Solo lee información; no instala ni cambia nada. Al final guarda un informe en revisar_pc.txt.

$ErrorActionPreference = 'SilentlyContinue'
$informe = New-Object System.Collections.Generic.List[string]
$faltantes = New-Object System.Collections.Generic.List[string]

function Linea($estado, $texto) {
    $marca = @{ OK = '[OK]   '; FALTA = '[FALTA]'; AVISO = '[AVISO]' }[$estado]
    $color = @{ OK = 'Green'; FALTA = 'Red'; AVISO = 'Yellow' }[$estado]
    Write-Host "$marca $texto" -ForegroundColor $color
    $informe.Add("$marca $texto")
}

Write-Host "`n=== Hexa Frontera: revisión de la PC ===`n" -ForegroundColor Cyan

# Windows
$os = Get-CimInstance Win32_OperatingSystem
$build = [int]$os.BuildNumber
if ($build -ge 19041) { Linea OK "Windows: $($os.Caption) (build $build)" }
else { Linea FALTA "Windows: $($os.Caption) (build $build). Unreal 5.4 necesita Windows 10 20H1 o posterior"; $faltantes.Add('windows') }

# Memoria
$ramGB = [math]::Round($os.TotalVisibleMemorySize / 1MB)
if ($ramGB -ge 32) { Linea OK "Memoria RAM: $ramGB GB" }
elseif ($ramGB -ge 16) { Linea AVISO "Memoria RAM: $ramGB GB (alcanza; con 32 GB compila y abre bastante más rápido)" }
else { Linea FALTA "Memoria RAM: $ramGB GB. El editor de Unreal necesita al menos 16 GB"; $faltantes.Add('ram') }

# Procesador
$cpu = Get-CimInstance Win32_Processor | Select-Object -First 1
Linea OK "Procesador: $($cpu.Name.Trim()) ($($cpu.NumberOfLogicalProcessors) hilos)"

# Placa de video
$gpus = Get-CimInstance Win32_VideoController | Where-Object { $_.Name -notmatch 'Basic|Remote|Virtual' }
if (-not $gpus) { Linea FALTA "No se encontró una placa de video dedicada"; $faltantes.Add('gpu') }
foreach ($gpu in $gpus) {
    # AdapterRAM se corta en 4 GB; el registro tiene el valor real.
    $vram = $null
    Get-ChildItem 'HKLM:\SYSTEM\ControlSet001\Control\Class\{4d36e968-e325-11ce-bfc1-08002be10318}' | ForEach-Object {
        $p = Get-ItemProperty $_.PSPath
        if ($p.DriverDesc -eq $gpu.Name -and $p.'HardwareInformation.qwMemorySize') { $vram = [math]::Round($p.'HardwareInformation.qwMemorySize' / 1GB, 1) }
    }
    if (-not $vram) { $vram = [math]::Round($gpu.AdapterRAM / 1GB, 1) }
    $texto = "Placa de video: $($gpu.Name), $vram GB, driver $($gpu.DriverVersion)"
    if ($gpu.Name -match 'Intel.*(UHD|HD Graphics|Iris)') { Linea AVISO "$texto. Es integrada: el juego va a abrir, pero Lumen va a andar lento" }
    elseif ($vram -ge 6) { Linea OK $texto }
    else { Linea AVISO "$texto. Con menos de 6 GB conviene bajar la calidad (Lumen en modo bajo)" }
}

# Espacio en disco
$discos = Get-CimInstance Win32_LogicalDisk -Filter "DriveType=3"
$mejor = $discos | Sort-Object FreeSpace -Descending | Select-Object -First 1
foreach ($d in $discos) { $informe.Add("        Disco $($d.DeviceID) libre: $([math]::Round($d.FreeSpace/1GB)) GB") }
$libreGB = [math]::Round($mejor.FreeSpace / 1GB)
if ($libreGB -ge 150) { Linea OK "Espacio libre: $libreGB GB en $($mejor.DeviceID)" }
else { Linea FALTA "Espacio libre: $libreGB GB en $($mejor.DeviceID). Unreal (~60 GB), Visual Studio (~30 GB) y el proyecto compilado necesitan unos 150 GB"; $faltantes.Add('disco') }

# Visual Studio 2022
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = $null
if (Test-Path $vswhere) { $vs = & $vswhere -version '[17.0,18.0)' -products * -format json | ConvertFrom-Json | Select-Object -First 1 }
if ($vs) {
    Linea OK "Visual Studio: $($vs.displayName) $($vs.catalog.productDisplayVersion)"
    $tieneJuegos = & $vswhere -version '[17.0,18.0)' -products * -requires Microsoft.VisualStudio.Workload.NativeGame -property instanceId
    $tieneCpp = & $vswhere -version '[17.0,18.0)' -products * -requires Microsoft.VisualStudio.Workload.NativeDesktop -property instanceId
    if ($tieneJuegos) { Linea OK "  Carga 'Desarrollo de juegos con C++' instalada" } else { Linea FALTA "  Falta la carga 'Desarrollo de juegos con C++'"; $faltantes.Add('vs-juegos') }
    if ($tieneCpp) { Linea OK "  Carga 'Desarrollo para el escritorio con C++' instalada" } else { Linea FALTA "  Falta la carga 'Desarrollo para el escritorio con C++'"; $faltantes.Add('vs-cpp') }
} else {
    Linea FALTA "Visual Studio 2022 no está instalado"; $faltantes.Add('vs')
}

# Epic Games Launcher y Unreal Engine
$launcher = @("${env:ProgramFiles(x86)}\Epic Games\Launcher\Portal\Binaries\Win64\EpicGamesLauncher.exe",
              "$env:ProgramFiles\Epic Games\Launcher\Portal\Binaries\Win64\EpicGamesLauncher.exe") | Where-Object { Test-Path $_ } | Select-Object -First 1
if ($launcher) { Linea OK "Epic Games Launcher instalado" } else { Linea FALTA "Epic Games Launcher no está instalado"; $faltantes.Add('epic') }

$versiones = @()
$dat = "$env:ProgramData\Epic\UnrealEngineLauncher\LauncherInstalled.dat"
if (Test-Path $dat) {
    $versiones = (Get-Content $dat -Raw | ConvertFrom-Json).InstallationList | Where-Object { $_.AppName -match '^UE_5' }
}
Get-ChildItem 'HKLM:\SOFTWARE\EpicGames\Unreal Engine' | ForEach-Object {
    $dir = (Get-ItemProperty $_.PSPath).InstalledDirectory
    if ($dir -and -not ($versiones | Where-Object { $_.InstallLocation -eq $dir })) { $versiones += [pscustomobject]@{ AppName = "UE_$($_.PSChildName)"; InstallLocation = $dir } }
}
$compatibles = $versiones | Where-Object { [version]($_.AppName -replace '^UE_', '') -ge [version]'5.4' }
if ($compatibles) { foreach ($v in $compatibles) { Linea OK "Unreal Engine $($v.AppName -replace '^UE_','') en $($v.InstallLocation)" } }
elseif ($versiones) { Linea FALTA "Hay Unreal $(( $versiones | ForEach-Object { $_.AppName -replace '^UE_','' }) -join ', '), pero el juego necesita 5.4 o posterior"; $faltantes.Add('ue') }
else { Linea FALTA "Unreal Engine 5.4 o posterior no está instalado"; $faltantes.Add('ue') }

# Git y Git LFS
if (Get-Command git) {
    Linea OK "Git: $((git --version) -replace 'git version ','')"
    if (git lfs version 2>$null) { Linea OK "Git LFS instalado" } else { Linea FALTA "Falta Git LFS (para modelos y texturas)"; $faltantes.Add('lfs') }
} elseif (Test-Path "$env:LOCALAPPDATA\GitHubDesktop") {
    Linea AVISO "Git no está en la línea de comandos (GitHub Desktop trae el suyo, con LFS incluido). Alcanza para clonar"
} else { Linea FALTA "Git no está instalado"; $faltantes.Add('git') }

# Resumen
Write-Host ""
if ($faltantes.Count -eq 0) {
    Write-Host "Todo listo. Seguí los pasos de Docs\COMPILAR.md para compilar y jugar." -ForegroundColor Green
} else {
    Write-Host "Falta: $($faltantes -join ', ')" -ForegroundColor Red
    if ($faltantes | Where-Object { $_ -in 'vs', 'vs-juegos', 'vs-cpp', 'epic', 'lfs', 'git' }) {
        Write-Host "Para instalarlo: Tools\windows\instalar_requisitos.ps1 (como administrador)." -ForegroundColor Yellow
    }
    if ($faltantes -contains 'ue') {
        Write-Host "Unreal se instala desde el Epic Games Launcher: Unreal Engine > Biblioteca > + > 5.4 o posterior." -ForegroundColor Yellow
    }
}
$informe.Add(""); $informe.Add("Falta: " + ($(if ($faltantes.Count) { $faltantes -join ', ' } else { 'nada' })))
$salida = Join-Path $PSScriptRoot 'revisar_pc.txt'
$informe | Set-Content -Path $salida -Encoding UTF8
Write-Host "`nInforme guardado en $salida (podés pegarlo en el chat)." -ForegroundColor Cyan
if ($Host.Name -eq 'ConsoleHost') { Read-Host "`nApretá Enter para cerrar" | Out-Null }
