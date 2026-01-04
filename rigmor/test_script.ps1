cls;

$envSetup = '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64';

New-Item -ItemType Directory -Force -Path build\rigmor | Out-Null
New-Item -ItemType Directory -Force -Path build\tests | Out-Null

# ---- Build rigmor.exe ----
$buildRigmor = 'cl /TP /std:c++17 /EHsc /Zi /Od /DDEBUG rigmor\rigmor.hpp /D RIGMOR_IMPLEMENTATION /Fo:build\rigmor\ /Fe:build\rigmor\rigmor.exe /Fd:build\rigmor\rigmor.pdb';

# ---- Build rigmor_e2e.exe ----
$buildTests = 'cl /TP /std:c++17 /EHsc /Zi /Od /DDEBUG rigmor\test_rigmor.hpp /Fo:build\tests\ /Fe:build\tests\rigmor_e2e.exe /Fd:build\tests\rigmor_e2e.pdb';

cmd /c "$envSetup && $buildRigmor";
if ($LASTEXITCODE -ne 0) { throw "Build rigmor failed with exit code $LASTEXITCODE" }

cmd /c "$envSetup && $buildTests";
if ($LASTEXITCODE -ne 0) { throw "Build tests failed with exit code $LASTEXITCODE" }

# Unique test dir for this run
$testDir = Join-Path (Resolve-Path "build\tests") ("run_" + [Guid]::NewGuid().ToString("N"))

try {
    New-Item -ItemType Directory -Force -Path $testDir | Out-Null

    $rigmorExe = Resolve-Path "build\rigmor\rigmor.exe"
    $testExe   = Resolve-Path "build\tests\rigmor_e2e.exe"

    # Run tests. We pass rigmor.exe path + test dir path.
    & $testExe $rigmorExe $testDir
    if ($LASTEXITCODE -ne 0) { throw "Tests failed with exit code $LASTEXITCODE" }

    Write-Host "Tests OK"
}
finally {
    # Cleanup even if the test process crashed
    if (Test-Path $testDir) {
        Remove-Item -Recurse -Force $testDir -ErrorAction SilentlyContinue
    }
}
