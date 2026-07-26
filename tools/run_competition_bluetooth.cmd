@echo off
chcp 65001 >nul
py -3 "%~dp0competition_bluetooth_console.py" %*
