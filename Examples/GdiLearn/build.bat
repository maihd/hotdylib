@echo off

set errno=0
set INCDIR=../../
set CFLAGS=-Wall -I%INCDIR% -Wl,-subsystem=console
set LFLAGS=-luser32 -lgdi32

if exist gdi.exe (
   del /Q /F gdi.exe
)

REM test if gcc is install
echo "Checking GCC is installed"
cmd /C "gcc -dumpversion" >NUL 2>NUL

REM begin build
REM if %ERRORLEVEL% neq 0 (
   echo "=> Build with TCC"
   tcc\tcc.exe -o app.dll  app.c -shared %CFLAGS% %LFLAGS%
   tcc\tcc.exe -o gdi.exe main.c app.def %CFLAGS% %LFLAGS%
REM ) else (
REM  echo "=> Build with GCC"
REM   gcc -o app.dll app.c --shared %CFLAGS% %LFLAGS%
REM   gcc -o gdi.exe main.c app.dll %CFLAGS% %LFLAGS%
REM )

if %ERRORLEVEL% neq 0 (
   pause
   set /A errno=1
)
