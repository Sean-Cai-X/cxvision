@echo off
set EXE=D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\AIbuild\Release\cxvision_imgui_acceptance.exe
set ROOT=D:\Codex-WorkDir\Sean_WorkDir\cxvisionai
set REPO=%ROOT%\cxvision_repo
set OUT=%ROOT%\cxscript_runs\find_line_direct

"%EXE%" ^
  --cxscript-headless ^
  --image "%ROOT%\01.jpg" ^
  --script "%REPO%\cxparser\cxscript\module\cximage\find_line_direct_test.cxsc" ^
  --out "%OUT%"

echo exit_code=%ERRORLEVEL%