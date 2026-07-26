param(
    [string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot),
    [string]$Syscfg,
    [string]$OutDir,
    [ValidateSet("gcc", "ticlang", "iar")]
    [string]$Compiler = "gcc"
)

$ErrorActionPreference = "Stop"
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)

function Find-SysConfigCli {
    $candidates = @()

    if ($env:SYSCONFIG_CLI) {
        $candidates += $env:SYSCONFIG_CLI
    }
    if ($env:SYSCONFIG_ROOT) {
        $candidates += (Join-Path $env:SYSCONFIG_ROOT "sysconfig_cli.bat")
    }

    foreach ($root in @("C:\TI", "D:\TI")) {
        if (Test-Path -LiteralPath $root) {
            # CCS keeps its bundled SysConfig under
            # <TI root>\ccs<version>\ccs\utils\sysconfig_<version>.
            $candidates += Get-ChildItem -LiteralPath $root -Directory -Filter "ccs*" -ErrorAction SilentlyContinue |
                ForEach-Object {
                    $utilsRoot = Join-Path $_.FullName "ccs\utils"
                    if (Test-Path -LiteralPath $utilsRoot) {
                        Get-ChildItem -LiteralPath $utilsRoot -Directory -Filter "sysconfig_*" -ErrorAction SilentlyContinue |
                            Sort-Object Name -Descending |
                            ForEach-Object { Join-Path $_.FullName "sysconfig_cli.bat" }
                    }
                }

            $candidates += Get-ChildItem -LiteralPath $root -Directory -Filter "sysconfig_*" -ErrorAction SilentlyContinue |
                Sort-Object Name -Descending |
                ForEach-Object { Join-Path $_.FullName "sysconfig_cli.bat" }
        }
    }

    return $candidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
}

function Find-Mspm0Product {
    $candidates = @()

    if ($env:MSPM0_SDK_ROOT) {
        $candidates += (Join-Path $env:MSPM0_SDK_ROOT ".metadata\product.json")
    }

    $searchRoots = @("C:\TI", "D:\TI")
    if (Test-Path -LiteralPath "D:\") {
        $searchRoots += Get-ChildItem -LiteralPath "D:\" -Directory -Force -ErrorAction SilentlyContinue |
            Where-Object {
                ($_.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -eq 0
            } |
            ForEach-Object FullName
    }

    foreach ($root in $searchRoots) {
        if ([System.IO.Directory]::Exists($root)) {
            $candidates += Get-ChildItem -LiteralPath $root -Directory -Filter "mspm0_sdk_*" -ErrorAction SilentlyContinue |
                Sort-Object Name -Descending |
                ForEach-Object { Join-Path $_.FullName ".metadata\product.json" }
        }
    }

    return $candidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
}

$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path

if (-not $Syscfg) {
    $Syscfg = Join-Path $ProjectRoot "empty_mspm0g3507.syscfg"
}
if (-not $OutDir) {
    $OutDir = Join-Path $ProjectRoot "source"
}

if (-not (Test-Path -LiteralPath $Syscfg)) {
    throw "SysConfig file not found: $Syscfg"
}

$sysConfigCli = Find-SysConfigCli
if (-not $sysConfigCli) {
    throw "sysconfig_cli.bat not found. Install TI SysConfig or set SYSCONFIG_CLI."
}

$productJson = Find-Mspm0Product
if (-not $productJson) {
    throw "MSPM0 SDK product.json not found. Install the SDK or set MSPM0_SDK_ROOT."
}

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

Write-Host "SysConfig CLI : $sysConfigCli"
Write-Host "MSPM0 SDK    : $productJson"
Write-Host "Config file   : $Syscfg"
Write-Host "Output dir    : $OutDir"

& $sysConfigCli -s $productJson --script $Syscfg -o $OutDir --compiler $Compiler
if ($LASTEXITCODE -ne 0) {
    throw "SysConfig generation failed with exit code $LASTEXITCODE"
}

Write-Host "SysConfig generation completed."
