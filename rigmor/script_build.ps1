cls;
$envSetup = '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64';
$buildCmd = 'cl /TP /std:c++17 /EHsc /Zi /Od /DDEBUG rigmor\rigmor.hpp /D RIGMOR_IMPLEMENTATION /Fo:build\rigmor\ /Fe:build\rigmor\rigmor.exe /Fd:build\rigmor\rigmor.pdb';
New-Item -ItemType Directory -Force -Path build\rigmor | Out-Null;
cmd /c "$envSetup && $buildCmd";
if ($LASTEXITCODE -ne 0) { throw "Build failed with exit code $LASTEXITCODE" }
