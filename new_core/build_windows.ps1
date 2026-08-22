[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",

    [switch]$SkipTests
)

$ErrorActionPreference = "Stop"

function Find-CMake {
    $cmakeCommand = Get-Command cmake.exe -ErrorAction SilentlyContinue
    if ($cmakeCommand) {
        return $cmakeCommand.Source
    }

    $visualStudioRoots = @(
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Professional",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Enterprise",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\BuildTools"
    )

    foreach ($visualStudioRoot in $visualStudioRoots) {
        $candidate = Join-Path $visualStudioRoot "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }

    throw "CMake was not found. Install the Visual Studio C++ CMake tools component."
}

$cmake = Find-CMake
$newCoreRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$buildPreset = "windows-x86-$($Configuration.ToLowerInvariant())"

Push-Location $newCoreRoot
try {
    & $cmake --preset windows-x86
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configure failed with exit code $LASTEXITCODE."
    }

    & $cmake --build --preset $buildPreset
    if ($LASTEXITCODE -ne 0) {
        throw "CMake build failed with exit code $LASTEXITCODE."
    }

    if (-not $SkipTests) {
        $ctest = Join-Path (Split-Path -Parent $cmake) "ctest.exe"
        & $ctest --preset $buildPreset
        if ($LASTEXITCODE -ne 0) {
            throw "CTest failed with exit code $LASTEXITCODE."
        }
    }
}
finally {
    Pop-Location
}
