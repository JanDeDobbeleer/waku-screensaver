[CmdletBinding()]
param(
    [ValidateSet('x64', 'arm64')][string]$Architecture = 'arm64'
)

$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $true
$root = Split-Path $PSScriptRoot -Parent

$formatter = Join-Path $env:ProgramFiles 'LLVM\bin\clang-format.exe'
if (-not (Test-Path -LiteralPath $formatter)) {
    $formatter = (Get-Command clang-format -ErrorAction Stop).Source
}
Get-ChildItem -LiteralPath (Join-Path $root 'src'), (Join-Path $root 'tests') -Recurse -File |
    Where-Object Extension -In '.cpp', '.h' |
    ForEach-Object { & $formatter --dry-run --Werror $_.FullName }

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$visualStudio = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $visualStudio) { throw 'Visual Studio C++ tools were not found.' }
$batch = if ($Architecture -eq 'arm64') { 'vcvarsarm64.bat' } else { 'vcvars64.bat' }
$vcvars = Join-Path $visualStudio "VC\Auxiliary\Build\$batch"
$build = Join-Path $root "build\$Architecture-release"

& $env:ComSpec /d /c "call `"$vcvars`" && cd /d `"$root`" && cmake -S . -B `"$build`" -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build `"$build`" && ctest --test-dir `"$build`" --output-on-failure --timeout 120"
