@echo off
set PYTHONIOENCODING=utf-8
python "%~dp0build.py"
exit /b %ERRORLEVEL%
