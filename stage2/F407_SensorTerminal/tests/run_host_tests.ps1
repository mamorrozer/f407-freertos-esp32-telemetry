$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$gcc = (Get-Command gcc -ErrorAction SilentlyContinue).Source
$testSource = Join-Path $PSScriptRoot 'st7735_scaled_text_test.c'
$driverSource = Join-Path $projectRoot 'Core\Src\st7735.c'
$formatTestSource = Join-Path $PSScriptRoot 'tft_format_test.c'
$formatSource = Join-Path $projectRoot 'Core\Src\tft_format.c'
$testBinaryDirectory = Join-Path $projectRoot 'build\host_tests'
New-Item -ItemType Directory -Path $testBinaryDirectory -Force | Out-Null
$binary = Join-Path $testBinaryDirectory 'st7735_scaled_text_test.exe'
$formatBinary = Join-Path $testBinaryDirectory 'tft_format_test.exe'

if (-not $gcc) {
    throw 'Host GCC was not found on PATH. Install a C compiler such as MinGW-w64 and reopen the terminal.'
}

& $gcc -std=c11 -Wall -Wextra -Werror `
    -I (Join-Path $projectRoot 'Core\Inc') `
    $testSource $driverSource -o $binary
if ($LASTEXITCODE -ne 0) {
    throw "Host test compilation failed with exit code $LASTEXITCODE"
}

& $binary
if ($LASTEXITCODE -ne 0) {
    throw "Host tests failed with exit code $LASTEXITCODE"
}

& $gcc -std=c11 -Wall -Wextra -Werror `
    -I (Join-Path $projectRoot 'Core\Inc') `
    $formatTestSource $formatSource -o $formatBinary
if ($LASTEXITCODE -ne 0) {
    throw "TFT formatting test compilation failed with exit code $LASTEXITCODE"
}

& $formatBinary
if ($LASTEXITCODE -ne 0) {
    throw "TFT formatting tests failed with exit code $LASTEXITCODE"
}
