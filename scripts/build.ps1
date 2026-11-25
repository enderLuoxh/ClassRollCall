param([switch]$Test)
$ErrorActionPreference = 'Stop'
$repoPath = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Push-Location $repoPath
try {
    cmake -S . -B build -A Win32 "-DBUILD_TESTING=$($Test.IsPresent.ToString().ToUpperInvariant())"
    if ($LASTEXITCODE) { throw 'CMake configure failed' }
    cmake --build build --config Release --parallel 2
    if ($LASTEXITCODE) { throw 'Release build failed' }
    if ($Test) {
        python -m pip install openpyxl==3.1.5 python-docx==1.1.2 xlwt==1.3.0
        if ($LASTEXITCODE) { throw 'Fixture dependencies failed' }
        python scripts/generate_fixtures.py
        if ($LASTEXITCODE) { throw 'Fixture generation failed' }
        ctest --test-dir build -C Release --output-on-failure
        if ($LASTEXITCODE) { throw 'Tests failed' }
    }
    Write-Output (Join-Path $repoPath 'dist\ClassRollCall.exe')
} finally { Pop-Location }
