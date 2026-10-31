@echo off
setlocal
cd /d "%~dp0"

if not exist Data mkdir Data

where ruby >nul 2>nul
if errorlevel 1 (
  echo ruby was not found on PATH; pack Data/Scripts.rxdata with a Ruby that has zlib.
  exit /b 1
)
ruby pack_scripts.rb main.rb Data/Scripts.rxdata
if errorlevel 1 exit /b 1

if not exist Game.exe (
  echo Copy Game.exe into this directory first.
  exit /b 1
)

Game.exe
