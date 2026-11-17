[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateSet('x64', 'arm64')][string]$Architecture,
    [switch]$Sign,
    [string]$SigningToolsVersion = '1.0.92'
)

$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $true
$root = Split-Path $PSScriptRoot -Parent
$binary = Join-Path $root "build\$Architecture-release\waku-screensaver.exe"
if (-not (Test-Path -LiteralPath $binary)) { throw "Build output not found: $binary" }
$out = Join-Path $root 'out'
$stage = Join-Path $out "stage-$Architecture"
if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
New-Item -ItemType Directory -Path $stage -Force | Out-Null
$exe = Join-Path $stage 'waku-screensaver.exe'
Copy-Item -LiteralPath $binary -Destination $exe

if ($Sign) {
    foreach ($name in 'AZURE_CLIENT_ID', 'AZURE_CLIENT_SECRET', 'AZURE_TENANT_ID') {
        if (-not [Environment]::GetEnvironmentVariable($name)) {
            throw "Signing requires the $name repository secret."
        }
    }
    $kits = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\bin'
    $signtool = Get-ChildItem -LiteralPath $kits -Directory -Filter '10.*' |
        Sort-Object { [version]$_.Name } -Descending |
        ForEach-Object { Join-Path $_.FullName 'x64\signtool.exe' } |
        Where-Object { Test-Path -LiteralPath $_ } |
        Select-Object -First 1
    if (-not $signtool) { throw 'signtool.exe was not found in the Windows SDK.' }
    $work = Join-Path $out 'signing-tools'
    nuget.exe install Microsoft.Trusted.Signing.Client -Version $SigningToolsVersion -OutputDirectory $work -ExcludeVersion -NonInteractive
    $dlib = Join-Path $work 'Microsoft.Trusted.Signing.Client\bin\x64\Azure.CodeSigning.Dlib.dll'
    if (-not (Test-Path -LiteralPath $dlib)) { throw "Signing library not found: $dlib" }
    & $signtool sign /v /d 'WAKU Screensaver' /fd SHA256 /tr 'http://timestamp.acs.microsoft.com' /td SHA256 /dlib $dlib /dmdf (Join-Path $PSScriptRoot 'signing.json') $exe
    & $signtool verify /pa /all $exe
}

# Both extensions contain the same signed PE; signing precedes the copy.
$scr = Join-Path $stage 'waku-screensaver.scr'
Copy-Item -LiteralPath $exe -Destination $scr
if ($Sign) { & $signtool verify /pa /all $scr }
Copy-Item -LiteralPath (Join-Path $root 'README.md'), (Join-Path $root 'LICENSE') -Destination $stage
$archive = Join-Path $out "waku-screensaver-$Architecture.zip"
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $archive -Force
Write-Host "Created $archive (signed: $($Sign.IsPresent))"
