@echo off
REM Daftarkan CSV Commander (portable) sebagai pembuka file .csv/.tsv untuk user ini.
REM Jalankan dari folder yang sama dengan CSVCommander.exe. Tanpa hak admin.
set EXE=%~dp0CSVCommander.exe
if not exist "%EXE%" (echo CSVCommander.exe tidak ditemukan di folder ini. & pause & exit /b 1)
reg add "HKCU\Software\Classes\CSVCommander.Document" /ve /d "File CSV" /f >nul
reg add "HKCU\Software\Classes\CSVCommander.Document\DefaultIcon" /ve /d "%EXE%,1" /f >nul
reg add "HKCU\Software\Classes\CSVCommander.Document\shell\open\command" /ve /d "\"%EXE%\" \"%%1\"" /f >nul
reg add "HKCU\Software\Classes\.csv" /ve /d "CSVCommander.Document" /f >nul
reg add "HKCU\Software\Classes\.csv\OpenWithProgids" /v "CSVCommander.Document" /d "" /f >nul
reg add "HKCU\Software\Classes\.tsv\OpenWithProgids" /v "CSVCommander.Document" /d "" /f >nul
echo.
echo Selesai. File .csv kini memakai icon dan pembuka CSV Commander.
echo Jika belum jadi default, klik kanan file .csv ^> Open with ^> CSV Commander ^> Always.
pause
