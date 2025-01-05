@echo off
set "currentPath=%~dp0"
set "subdir=input"
set "program=example.exe"
set "programPath=%currentPath%%subdir%\%program%"
"%programPath%" %currentPath%