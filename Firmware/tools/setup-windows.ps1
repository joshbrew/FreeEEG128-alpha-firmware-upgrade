param([switch]$FlashOnly, [switch]$SerialOnly, [switch]$NoSerialTools)
$ErrorActionPreference = 'Stop'
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
$taskRoot = Split-Path -Parent $PSScriptRoot
if (Test-Path -LiteralPath (Join-Path $taskRoot 'Alpha')) { $taskRoot = Split-Path -Parent $taskRoot }
$store = Join-Path $taskRoot '.tools'
New-Item -ItemType Directory -Force -Path $store | Out-Null
$taskSummary = New-Object 'System.Collections.Generic.List[string]'
$taskMode = if ($SerialOnly) { 'USB controls/update helper' } elseif ($FlashOnly) { 'Flash precompiled firmware' } else { 'Build and flash tools' }
Write-Host "`nFreeEEG128 tool setup - $taskMode"
Write-Host 'Checking installed tools first. Missing tools are installed automatically.'
Write-Host '  Python 3.10+: runs the build, flash and control helpers.'
if (!$FlashOnly -and !$SerialOnly) { Write-Host '  Arm GNU Toolchain: compiler, debugger and C libraries for building STM32 firmware.' }
if (!$SerialOnly) { Write-Host '  OpenOCD: programs/verifies the board through an ST-Link SWD probe.' }
if (!$NoSerialTools) { Write-Host '  pyserial 3.5: lets the USB helper send commands and request UF2 mode.' }
Write-Host "  Project-local tools/cache: $store"
Write-Host ''

function Probe-Python([string]$Command, [string[]]$Arguments) {
    # Windows PowerShell 5.1 can turn native stderr into a terminating error,
    # even with 2>$null. A failed probe must allow the next candidate to run.
    $probePreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $found = & $Command @Arguments -c "import sys; print(sys.executable if sys.version_info >= (3,10) else ''); sys.exit(0 if sys.version_info >= (3,10) else 1)" 2>$null
        $probeExit = $LASTEXITCODE
    } finally { $ErrorActionPreference = $probePreference }
    if ($probeExit -eq 0) { return ($found | Select-Object -Last 1) }
    return $null
}
function Find-Python {
    if (Get-Command py -ErrorAction SilentlyContinue) {
        $found = Probe-Python 'py' @('-3')
        if ($found) { return $found }
    }
    $candidate = Get-Command python -ErrorAction SilentlyContinue
    if ($candidate -and $candidate.Source -notmatch 'WindowsApps') {
        $found = Probe-Python 'python' @()
        if ($found) { return $found }
    }
    return $null
}
function Install-WingetPackage([string]$Id, [string]$Name, [string]$Purpose, [string]$Source) {
    if (!(Get-Command winget -ErrorAction SilentlyContinue)) {
        throw "Install Windows App Installer (winget), then retry: https://learn.microsoft.com/windows/package-manager/winget/"
    }
    Write-Host "[DOWNLOAD/INSTALL] $Name ($Id)"
    Write-Host "  Why: $Purpose"
    Write-Host "  Source: winget catalog; $Source"
    Write-Host '  Destination: standard Windows installation selected by the publisher installer; PATH may be updated.'
    Write-Host '  winget shows the resolved version, exact installer URL, download size and progress below.'
    $savedPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try { & winget install --id $Id --exact --source winget --accept-package-agreements --accept-source-agreements }
    finally { $ErrorActionPreference = $savedPreference }
    if ($LASTEXITCODE -ne 0) { throw "Installation failed: $Id. Install it manually and rerun." }
    $env:Path = [Environment]::GetEnvironmentVariable('Path','Machine') + ';' + [Environment]::GetEnvironmentVariable('Path','User')
}
$python = Find-Python
$pythonInstalled = !$python
if (!$python) {
    Install-WingetPackage 'Python.Python.3.11' 'Python 3.11' 'Runs FreeEEG128 automation and creates the local helper environment.' 'Python installer supplied by python.org.'
    $python = Find-Python
}
if (!$python) { throw 'Python 3.10+ was installed but not found. Reopen the launcher.' }
Write-Host "[READY] Python: $python"
$taskSummary.Add("Python $(if ($pythonInstalled) {'installed'} else {'reused'}): $python")
Set-Content -LiteralPath (Join-Path $store 'python-path.txt') -Value $python -Encoding UTF8

if (!$FlashOnly -and !$SerialOnly) {
    $findCode = @'
import os,shutil,pathlib
p=os.path.join(os.environ['FREEEEG128_GCC_DIR'],'arm-none-eabi-gcc.exe') if os.environ.get('FREEEEG128_GCC_DIR') else shutil.which('arm-none-eabi-gcc')
if not p:
 for key in ('ProgramW6432','ProgramFiles','ProgramFiles(x86)'):
  for pattern in ('GNU Arm Embedded Toolchain/*/bin/arm-none-eabi-gcc.exe','Arm GNU Toolchain arm-none-eabi/*/bin/arm-none-eabi-gcc.exe'):
   m=sorted(pathlib.Path(os.environ.get(key,'C:/Program Files')).glob(pattern))
   if m: p=str(m[-1]);break
  if p: break
print(p or '')
'@
    $gcc = & $python -c $findCode
    $gccInstalled = !$gcc
    if (!$gcc) {
        Install-WingetPackage 'Arm.GnuArmEmbeddedToolchain' 'Arm GNU Toolchain' 'Compiles edited C firmware for the STM32 Cortex-M7; includes compiler/debugger/runtime libraries, so the download is large.' 'Arm installer supplied by developer.arm.com.'
        $gcc = & $python -c $findCode
    }
    if (!$gcc) { throw 'Arm compiler not found. Set FREEEEG128_GCC_DIR to its bin folder, then rerun.' }
    Write-Host "[READY] Arm compiler: $gcc"
    $taskSummary.Add("Arm compiler $(if ($gccInstalled) {'installed'} else {'reused'}): $gcc")
} else {
    Write-Host '[SKIP] Arm compiler: this action uses an existing firmware image or USB controls.'
}
if (!$SerialOnly) {
    $openocd = Get-Command openocd -ErrorAction SilentlyContinue
    $cached = Get-ChildItem -LiteralPath $store -Filter openocd.exe -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
    $openocdInstalled = !$openocd -and !$cached
    if (!$openocd -and !$cached) {
        $zip = Join-Path $store 'xpack-openocd-0.12.0-7-win32-x64.zip'
        $url = 'https://github.com/xpack-dev-tools/openocd-xpack/releases/download/v0.12.0-7/xpack-openocd-0.12.0-7-win32-x64.zip'
        $expected = '6bfd3c97135aafef8affc9af1acf34fd0e2b9ca26044506f6abd7f95b7630052'
        Write-Host '[DOWNLOAD/INSTALL] xPack OpenOCD 0.12.0-7 (portable Windows x64)'
        Write-Host '  Why: sends programming/verification commands to the STM32 through ST-Link SWD.'
        Write-Host "  Source: $url"
        Write-Host "  Download file: $zip"
        Write-Host "  Extract to: $store"
        Write-Host "  Expected SHA256: $expected"
        if (!(Test-Path -LiteralPath $zip)) {
            Write-Host '[DOWNLOAD] Fetching the OpenOCD ZIP; progress follows.'
            Invoke-WebRequest -UseBasicParsing -Uri $url -OutFile $zip
        } else { Write-Host '[CACHE] Reusing the existing ZIP; checking its SHA256 before extraction.' }
        if ((Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expected) {
            throw "OpenOCD checksum mismatch. Remove $zip and retry."
        }
        Write-Host '[VERIFIED] OpenOCD SHA256 matches. Extracting the portable tool.'
        Expand-Archive -LiteralPath $zip -DestinationPath $store -Force
        $cached = Get-ChildItem -LiteralPath $store -Filter openocd.exe -Recurse | Select-Object -First 1
        if (!$cached) { throw 'OpenOCD archive extracted, but openocd.exe was not found.' }
    }
    $openocdPath = if ($openocd) { $openocd.Source } else { $cached.FullName }
    Write-Host "[READY] OpenOCD: $openocdPath"
    $taskSummary.Add("OpenOCD $(if ($openocdInstalled) {'downloaded and SHA256-verified'} else {'reused'}): $openocdPath")
}
if (!$NoSerialTools) {
    $venv = Join-Path $taskRoot '.venv'
    if (!(Test-Path -LiteralPath (Join-Path $venv 'Scripts/python.exe'))) {
        Write-Host "[CREATE] Python helper environment: $venv (using the Python installation above)."
        & $python -m venv $venv
        if ($LASTEXITCODE -ne 0) { throw 'Could not create the Python environment.' }
    }
    $venvPython = Join-Path $venv 'Scripts/python.exe'
    & $venvPython -c "import importlib.util,sys;sys.exit(0 if importlib.util.find_spec('serial') else 1)"
    if ($LASTEXITCODE -ne 0) {
        Write-Host '[DOWNLOAD/INSTALL] pyserial==3.5'
        Write-Host '  Why: USB serial commands, port discovery and software entry into the UF2 loader.'
        Write-Host '  Source: pip package index (PyPI by default; pip displays its configured source below).'
        Write-Host "  Destination: $venv (project-local Python environment)."
        $savedPreference = $ErrorActionPreference
        $ErrorActionPreference = 'Continue'
        try { & $venvPython -m pip --disable-pip-version-check install -r (Join-Path $PSScriptRoot 'requirements-host.txt') }
        finally { $ErrorActionPreference = $savedPreference }
        if ($LASTEXITCODE -ne 0) { throw 'Could not install pyserial.' }
        $taskSummary.Add("pyserial 3.5 installed: $venv")
    } else { $taskSummary.Add("pyserial reused: $venv") }
    Write-Host "[READY] USB serial helper environment: $venv"
}
$summaryPath = Join-Path $store 'setup-summary.txt'
@("FreeEEG128 setup: $taskMode") + $taskSummary.ToArray() | Set-Content -LiteralPath $summaryPath -Encoding UTF8
Write-Host "`nSetup complete - $taskMode"
foreach ($entry in $taskSummary) { Write-Host "  $entry" }
Write-Host "Summary saved to: $summaryPath"
