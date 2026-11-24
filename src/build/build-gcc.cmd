@echo off
setlocal
set "gccCompiler=gcc.exe"
if not "%~1"=="" set "gccCompiler=%~1"
set "sourceDir=%~dp0.."
set "outputFile=%~dp0StudentInfoManager.exe"

"%gccCompiler%" -std=c11 -finput-charset=UTF-8 -DCJSON_HIDE_SYMBOLS -municode -static ^
    "%sourceDir%\main.c" ^
    "%sourceDir%\app\Application.c" ^
    "%sourceDir%\ui\ConsolePage.c" ^
    "%sourceDir%\ui\Dashboard.c" ^
    "%sourceDir%\ui\PageInput.c" ^
    "%sourceDir%\ui\StudentTable.c" ^
    "%sourceDir%\students\StudentPages.c" ^
    "%sourceDir%\files\FilePages.c" ^
    "%sourceDir%\files\StudentStore.c" ^
    "%sourceDir%\lib\cJSON\cJSON.c" ^
    -o "%outputFile%" -luser32
if errorlevel 1 exit /b %errorlevel%
echo Built "%outputFile%"
