@echo off
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 (echo Gagal memuat environment MSVC & exit /b 1)
rc /nologo /fo %TEMP%\csvcommander_res.res src\resources.rc
if errorlevel 1 (echo RC GAGAL & exit /b 1)
cl /nologo /std:c++17 /utf-8 /O2 /GL /EHsc /DUNICODE /D_UNICODE /W3 src\main.cpp ^
   /Fe:CSVCommander.exe /Fo:%TEMP%\csvcommander_main.obj ^
   /link /LTCG /SUBSYSTEM:WINDOWS /MANIFEST:EMBED %TEMP%\csvcommander_res.res
if errorlevel 1 (echo BUILD GAGAL & exit /b 1)
echo BUILD OK: CSVCommander.exe
