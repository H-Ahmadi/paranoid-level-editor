@echo off
rem Builds DESIGNER.EXE with Open Watcom 2.0 (16-bit DOS target).
rem Set WATCOM to your Open Watcom directory first, e.g.
rem     set WATCOM=C:\WATCOM
rem Loop optimisation (-ol, part of -ox) is left out on purpose: it
rem miscompiles the far-pointer video loops.
set PATH=%WATCOM%\binnt64;%WATCOM%\binnt;%PATH%
set INCLUDE=%WATCOM%\h
wcl -q -bt=dos -ms -0 -obmier -s -w4 -fe=DESIGNER.EXE DESIGNER.C
if exist DESIGNER.OBJ del DESIGNER.OBJ
