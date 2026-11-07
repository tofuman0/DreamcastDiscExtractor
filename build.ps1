param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [string]$BuildDirectory = (Join-Path $PSScriptRoot 'build/windows')
)

$ErrorActionPreference = 'Stop'
foreach ($tool in 'cmake', 'ctest', 'git') {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
        throw "Install $tool and add it to PATH."
    }
}

cmake -S $PSScriptRoot -B $BuildDirectory "-DCMAKE_BUILD_TYPE=$Configuration" -DBUILD_TESTING=ON
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

cmake --build $BuildDirectory --config $Configuration --parallel
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

ctest --test-dir $BuildDirectory -C $Configuration --output-on-failure
exit $LASTEXITCODE
