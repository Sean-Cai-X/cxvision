@echo off
setlocal enabledelayedexpansion

set EXE=D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\build01\Release\cxvision_imgui_acceptance.exe
set REPO=D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo
set IMAGE=D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\test_images\L1_high_contrast\line_high_contrast_001.jpg
set SCRIPT=%REPO%\cxparser\cxscript\module\cximage\stage25\param_regression\param_self_tuning_findline_direct.cxsc
set RUN_ROOT=D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxscript_runs\param_self_tuning
set RUN_ID=run_%date:~0,4%%date:~5,2%%date:~8,2%_%time:~0,2%%time:~3,2%%time:~6,2%

set BASE_X0=82
set BASE_Y0=183
set BASE_X1=1210
set BASE_Y1=183

set BASE_THRESHOLD=20
set BASE_WGAP=8
set BASE_HGAP=32
set BASE_LINEGAP=6
set BASE_METHOD=2
set BASE_FILTERPROFILE=1
set BASE_TOOL_HALF_WIDTH=50

set PHASE_DIR=%RUN_ROOT%\phase1\%RUN_ID%
mkdir "%PHASE_DIR%" 2>nul

echo ===== Phase 1: Gauge ROI Scan =====
echo Run ID: %RUN_ID%
echo Base ROI: (%BASE_X0%,%BASE_Y0%)-(%BASE_X1%,%BASE_Y1%)
echo Base Parameters: threshold=%BASE_THRESHOLD%, wgap=%BASE_WGAP%, hgap=%BASE_HGAP%, linegap=%BASE_LINEGAP%, method=%BASE_METHOD%, filterprofile=%BASE_FILTERPROFILE%, tool_half_width=%BASE_TOOL_HALF_WIDTH%
echo.

echo offset_y,points,fit,avgdist,timeout,failure_stage > "%PHASE_DIR%\phase1_results.csv"

set OFFSET=-50
:loop_phase1
if %OFFSET% GTR 50 goto end_phase1

set ROI_Y0=%BASE_Y0%
set /a ROI_Y0=%ROI_Y0%+%OFFSET%
set ROI_Y1=%BASE_Y1%
set /a ROI_Y1=%ROI_Y1%+%OFFSET%

set CASE_ID=offset_%OFFSET%
set OUT_DIR=%PHASE_DIR%\%CASE_ID%
mkdir "%OUT_DIR%" 2>nul

echo Running: offset_y=%OFFSET%, roi_y0=%ROI_Y0%, roi_y1=%ROI_Y1%

"%EXE%" ^
  --cxscript-headless ^
  --image "%IMAGE%" ^
  --script "%SCRIPT%" ^
  --out "%OUT_DIR%" ^
  --case-name "%CASE_ID%" ^
  --roi-x0 %BASE_X0% ^
  --roi-y0 %ROI_Y0% ^
  --roi-x1 %BASE_X1% ^
  --roi-y1 %ROI_Y1% ^
  --threshold %BASE_THRESHOLD% ^
  --wgap %BASE_WGAP% ^
  --hgap %BASE_HGAP% ^
  --linegap %BASE_LINEGAP% ^
  --method %BASE_METHOD% ^
  --filterprofile %BASE_FILTERPROFILE% ^
  --tool-half-width %BASE_TOOL_HALF_WIDTH%

set POINTS=0
set FIT=false
set AVGDIST=0.0
set TIMEOUT=false
set FAILURE_STAGE=none

if exist "%OUT_DIR%\result_summary.json" (
  for /f "tokens=1,2 delims=:" %%a in ('findstr "valid_points_count" "%OUT_DIR%\result_summary.json"') do (
    for /f "tokens=2 delims=," %%c in ("%%b") do (
      set POINTS=%%c
      set POINTS=!POINTS:"=!
    )
  )
  for /f "tokens=1,2 delims=:" %%a in ('findstr "has_fit_line" "%OUT_DIR%\result_summary.json"') do (
    for /f "tokens=2 delims=," %%c in ("%%b") do (
      set FIT=%%c
      set FIT=!FIT:"=!
    )
  )
  for /f "tokens=1,2 delims=:" %%a in ('findstr "avgdist" "%OUT_DIR%\result_summary.json"') do (
    for /f "tokens=2 delims=," %%c in ("%%b") do (
      set AVGDIST=%%c
      set AVGDIST=!AVGDIST:"=!
    )
  )
  for /f "tokens=1,2 delims=:" %%a in ('findstr "failure_stage" "%OUT_DIR%\result_summary.json"') do (
    for /f "tokens=2 delims=," %%c in ("%%b") do (
      set FAILURE_STAGE=%%c
      set FAILURE_STAGE=!FAILURE_STAGE:"=!
    )
  )
)

echo %OFFSET%,%POINTS%,%FIT%,%AVGDIST%,%TIMEOUT%,%FAILURE_STAGE% >> "%PHASE_DIR%\phase1_results.csv"

set /a OFFSET=%OFFSET%+5
goto loop_phase1

:end_phase1
echo.
echo ===== Phase 1 Complete =====
echo Results written to: %PHASE_DIR%\phase1_results.csv
endlocal