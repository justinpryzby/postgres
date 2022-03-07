REM @echo off
REM call /ProgramData/chocolatey/bin/ccache.exe cl.exe %*
@echo off
SET args=%*
call bash src\tools\ci\ccache-wrapper.sh %args%

