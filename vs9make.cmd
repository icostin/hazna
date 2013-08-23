@echo off

:: hazna - virtual execution environment
::
:: Build script for 32-bit Windows platforms using Visual Studio 9
::
:: Changelog:
::	* 2013/08/21 Costin Ionescu: initial release

set N=hazna
set D=HAZNA
set CSRC=src\core.c
set CSRC_CLI=src\cli.c src\test.c
call %VS90COMNTOOLS%\vsvars32.bat

if not exist out\win32-rls-sl mkdir out\win32-rls-sl
if not exist out\win32-dbg-sl mkdir out\win32-dbg-sl
if not exist out\win32-rls-dl mkdir out\win32-rls-dl
if not exist out\win32-dbg-dl mkdir out\win32-dbg-dl

echo Building dynamic release library...  
cl.exe /nologo /Ox /LD /Iinclude /I..\c41\include /D%D%_DL_BUILD /DNDEBUG /Foout\win32-rls-dl\ /Feout\win32-rls-dl\%N%.dll %CSRC% /link ..\c41\out\win32-rls-dl\c41.lib
echo Building static release library...
cl.exe /nologo /Ox /c  /Iinclude /I..\c41\include /D%D%_STATIC /DC41_STATIC /DNDEBUG /Foout\win32-rls-sl\ %CSRC% /link ..\c41\out\win32-rls-sl\c41.lib
set O=%CSRC:.c=.obj%
set O=%O:src=out\win32-rls-sl%
lib.exe /nologo /out:out\win32-rls-sl\%N%.lib %O%

echo Building dynamic debug library...  
cl.exe /nologo /Od /LDd /Iinclude /I..\c41\include /D%D%_DL_BUILD /D_DEBUG /Foout\win32-dbg-dl\ /Feout\win32-dbg-dl\%N%.dll %CSRC% /link ..\c41\out\win32-dbg-dl\c41.lib
echo Building static debug library...
cl.exe /nologo /Od /c  /Iinclude /I..\c41\include /D%D%_STATIC /DC41_STATIC /D_DEBUG /Foout\win32-dbg-sl\ %CSRC% /link ..\c41\out\win32-dbg-sl\c41.lib
set O=%CSRC:.c=.obj%
set O=%O:src=out\win32-dbg-sl%
lib.exe /nologo /out:out\win32-dbg-sl\%N%.lib %O%

echo Building static-deps cli program...
cl.exe /nologo /Ox /MT /Foout\win32-rls-sl\ /Feout\win32-rls-sl\%N%.exe %CSRC_CLI% /Iinclude /I..\c41\include /DC41_STATIC /DHBS1_STATIC /DHAZNA_STATIC /link ..\c41\out\win32-rls-sl\c41.lib ..\hbs1\out\win32-rls-sl\hbs1.lib ..\hbs1\out\win32-rls-sl\hbs1clis.lib out\win32-rls-sl\hazna.lib /subsystem:console
out\win32-rls-sl\%N%.exe

echo Building dynamic debug test program...
cl.exe /nologo /Od /Iinclude /I..\c41\include /I..\hbs1\include /D_DEBUG /Feout\win32-dbg-dl\%N%.exe /Foout\win32-dbg-dl\ %CSRC_CLI% /link ..\c41\out\win32-dbg-dl\c41.lib ..\hbs1\out\win32-dbg-dl\hbs1.lib ..\hbs1\out\win32-rls-dl\hbs1clid.lib out\win32-dbg-dl\%N%.lib /subsystem:console
set PATH=%PATH%;..\c41\out\win32-dbg-dl;..\hbs1\out\win32-dbg-dl;out\win32-dbg-dl
out\win32-dbg-dl\%N%.exe

