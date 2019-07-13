@echo off

set EXEName=re.exe
set SRC=..\code\main.cpp
set WindowsLibs=kernel32.lib user32.lib

set TestEXEName=test_re.exe
set TestSRC=..\code\tests\tester.cpp

rem -------------------------------------

set CompilerOptions=-DDFRE_WIN32

rem Disables extra printing
set CompilerOptions=%CompilerOptions% -nologo

rem Disables checking for minimal rebuild
set CompilerOptions=%CompilerOptions% -Gm-

rem Disables RTTI
set CompilerOptions=%CompilerOptions% -GR-

rem Disables inserting extra range check functions in the code (requires CRT)
set CompilerOptions=%CompilerOptions% -GS-

rem Disables exception handling
set CompilerOptions=%CompilerOptions% -EHa-

rem Don't put CRT references in the obj file
set CompilerOptions=%CompilerOptions% -Zl

rem Output full file paths paths in error messages
set CompilerOptions=%CompilerOptions% -FC

rem Enable all warnings and treat warnings as errors
rem Disables unreferenced formal parameter warning
rem Disables constant conditional expression warning (for do { } while(0) macros)
set CompilerOptions=%CompilerOptions% /wd4100 /wd4127 -W4 -WX

rem Put debug info in the obj file (don't make pdb files)
set CompilerOptions=%CompilerOptions% -Z7

rem We're a console app
set LinkerOptions=-subsystem:console

rem Don't link the CRT
set LinkerOptions=%LinkerOptions% -nodefaultlib

rem Disable incremental linking
set LinkerOptions=%LinkerOptions% -incremental:no

rem Remove functions and data that aren't referenced
set LinkerOptions=%LinkerOptions% -opt:ref

rem Provides __chkstk function, inserted to expand the stack if it goes past 1 page
set LinkerOptions=%LinkerOptions% chkstk.obj

rem Windows libs we use
set LinkerOptions=%LinkerOptions% %WindowsLibs%

rem -------------------------------------

cl.exe %CompilerOptions% %SRC% -Fe%EXEName% /link %LinkerOptions%
cl.exe %CompilerOptions% %TestSRC% /I ..\code\ -Fe%TestEXEName% /link %LinkerOptions%
