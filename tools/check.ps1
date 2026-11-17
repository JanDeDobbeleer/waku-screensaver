[CmdletBinding()]
param(
    [ValidateSet('x64', 'arm64')][string]$Architecture = 'arm64',
    [switch]$Analyze
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
    ForEach-Object {
        & $formatter --dry-run --Werror $_.FullName
        if ($LASTEXITCODE -ne 0) { throw "Formatting failed: $($_.FullName)" }
    }

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$visualStudio = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $visualStudio) { throw 'Visual Studio C++ tools were not found.' }
$batch = if ($Architecture -eq 'arm64') { 'vcvarsarm64.bat' } else { 'vcvars64.bat' }
$vcvars = Join-Path $visualStudio "VC\Auxiliary\Build\$batch"
$build = Join-Path $root "build\$Architecture-release"

function Invoke-VsCommand([string]$Command) {
    & $env:ComSpec /d /c "call `"$vcvars`" >nul && cd /d `"$root`" && $Command 2>&1"
    if ($LASTEXITCODE -ne 0) { throw "Command failed (exit ${LASTEXITCODE}): $Command" }
}

if (-not $Analyze) {
    Invoke-VsCommand "cmake -S . -B `"$build`" -G Ninja -DCMAKE_BUILD_TYPE=Release -DWAKU_ANALYZE=OFF && cmake --build `"$build`" && ctest --test-dir `"$build`" --output-on-failure --timeout 120"
    return
}

$tidy = Join-Path $env:ProgramFiles 'LLVM\bin\clang-tidy.exe'
if (-not (Test-Path -LiteralPath $tidy)) {
    $tidy = (Get-Command clang-tidy -ErrorAction Stop).Source
}
$build += '-analyze'
Invoke-VsCommand "cmake -S . -B `"$build`" -G Ninja -DCMAKE_BUILD_TYPE=Release -DWAKU_ANALYZE=ON"
$analysisLog = Join-Path $build 'msvc-analyze.log'
# A clean build is required because incremental no-op builds do not run /analyze.
Invoke-VsCommand "cmake --build `"$build`" --target waku-screensaver --clean-first" |
    Tee-Object -FilePath $analysisLog
if (Select-String -LiteralPath $analysisLog -Pattern '\bwarning C\d{4,5}:') {
    throw "MSVC analysis reported warnings. See $analysisLog"
}

$entries = Get-Content -LiteralPath (Join-Path $build 'compile_commands.json') -Raw |
    ConvertFrom-Json
# clang-cl cannot consume MSVC analyzer or anglebrackets options.
foreach ($entry in $entries) {
    $entry.command = $entry.command -replace '(?<=\s)/analyze(?::\S+)?(?=\s)', '' `
        -replace '(?<=\s)/external:anglebrackets(?=\s)', ''
}
$tidyDir = Join-Path $build 'clang-tidy'
New-Item -ItemType Directory -Force -Path $tidyDir | Out-Null
$database = ConvertTo-Json -InputObject @($entries) -Depth 10
[IO.File]::WriteAllText((Join-Path $tidyDir 'compile_commands.json'), $database,
    [Text.UTF8Encoding]::new($false))
$sourceRoot = [IO.Path]::GetFullPath((Join-Path $root 'src')) + [IO.Path]::DirectorySeparatorChar
$units = @($entries | Where-Object {
    [IO.Path]::GetFullPath($_.file).StartsWith($sourceRoot, [StringComparison]::OrdinalIgnoreCase)
} | Select-Object -ExpandProperty file -Unique)
if ($units.Count -eq 0) { throw 'No production compilation units found for clang-tidy.' }
$triple = if ($Architecture -eq 'arm64') { 'aarch64-pc-windows-msvc' } else { 'x86_64-pc-windows-msvc' }
$tidyLog = Join-Path $build 'clang-tidy.log'
"Analyzing $($units.Count) production units" | Set-Content -LiteralPath $tidyLog
foreach ($unit in $units) {
    "Analyzing $unit" | Tee-Object -FilePath $tidyLog -Append
    Invoke-VsCommand "`"$tidy`" -p `"$tidyDir`" --extra-arg=--target=$triple `"$unit`"" |
        Tee-Object -FilePath $tidyLog -Append
}
Write-Host "MSVC /analyze and clang-tidy passed for $Architecture. Logs: $build"
