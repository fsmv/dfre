@echo off


rem Gm- disables checking for minimal rebuild
rem GR- disables RTTI
rem EHa- disables exception handling
rem GS- disables buffer bounds checking
rem Zi  debug info
cl.exe -nologo -Gm- -GR- -EHa- -GS- -Zi winmain.cpp /link -nodefaultlib -subsystem:console kernel32.lib user32.lib chkstk.obj
