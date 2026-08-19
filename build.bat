@echo off
set PYTHONIOENCODING=utf-8
python "%~dp0build_native.py"
exit /b %ERRORLEVEL%
