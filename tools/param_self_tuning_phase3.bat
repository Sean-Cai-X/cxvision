@echo off
setlocal enabledelayedexpansion

set EXE=D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\build01\Release\cxvision_imgui_acceptance.exe
set REPO=D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo
set IMAGE=D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\test_images\L1_high_contrast\line_high_contrast_001.jpg
set SCRIPT=%REPO%\cxparser\cxscript\module\cximage\stage25\param_regression\param_self_tuning_findline_direct.cxsc
set RUN_ROOT=D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxscript_runs\param_self_tuning
set RUN_ID=run_%date:~0,4%%date:~5,2%%date:~8,2%_%time:~0,2%%time:~3,2%%time:~6,2%

set ROI_X0=82
set ROI_Y0=183
set ROI_X1=1210
set ROI_Y1=183

set THRESHOLD=20
set WGAP=8
set HGAP=32
set LINEGAP=6
set METHOD=0
set FILTERPROFILE=0
set TOOL_HALF_WIDTH=50

set PHASE_DIR=%RUN_ROOT%\phase3\%RUN_ID%
mkdir "%PHASE_DIR%" 2>nul

echo ===== Phase 3: Closed-Loop Iteration =====
echo Run ID: %RUN_ID%
echo Initial ROI: (%ROI_X0%,%ROI_Y0%)-(%ROI_X1%,%ROI_Y1%)
echo Initial Parameters: threshold=%THRESHOLD%, wgap=%WGAP%, hgap=%HGAP%, linegap=%LINEGAP%, method=%METHOD%, filterprofile=%FILTERPROFILE%
echo Max iterations: 5
echo.

echo iteration,roi_x0,roi_y0,roi_x1,roi_y1,threshold,wgap,hgap,linegap,filterprofile,points,fit,support,avgdist,action > "%PHASE_DIR%\phase3_results.csv"

set ITER=0
set STABLE_COUNT=0
set LAST_POINTS=0
set CONVERGED=false

:loop_phase3
set /a ITER+=1
if %ITER% GTR 5 goto end_phase3

set CASE_ID=iteration_%ITER%
set OUT_DIR=%PHASE_DIR%\%CASE_ID%
mkdir "%OUT_DIR%" 2>nul

echo Running iteration %ITER%: roi=(%ROI_X0%,%ROI_Y0%)-(%ROI_X1%,%ROI_Y1%), threshold=%THRESHOLD%, wgap=%WGAP%, hgap=%HGAP%, linegap=%LINEGAP%, filterprofile=%FILTERPROFILE%

"%EXE%" ^
  --cxscript-headless ^
  --image "%IMAGE%" ^
  --script "%SCRIPT%" ^
  --out "%OUT_DIR%" ^
  --case-name "%CASE_ID%" ^
  --roi-x0 %ROI_X0% ^
  --roi-y0 %ROI_Y0% ^
  --roi-x1 %ROI_X1% ^
  --roi-y1 %ROI_Y1% ^
  --threshold %THRESHOLD% ^
  --wgap %WGAP% ^
  --hgap %HGAP% ^
  --linegap %LINEGAP% ^
  --method %METHOD% ^
  --filterprofile %FILTERPROFILE% ^
  --tool-half-width %TOOL_HALF_WIDTH%

set POINTS=0
set FIT=false
set SUPPORT=0.0
set AVGDIST=0.0
set ACTION=none

if exist "%OUT_DIR%\result_summary.json" (
  for /f "tokens=1,2 delims=:" %%a in ('findstr "valid_line_points_count" "%OUT_DIR%\result_summary.json"') do (
    for /f "tokens=2 delims=," %%c in ("%%b") do set POINTS=%%c
  )
  for /f "tokens=1,2 delims=:" %%a in ('findstr "has_fit_line" "%OUT_DIR%\result_summary.json"') do (
    for /f "tokens=2 delims=," %%c in ("%%b") do set FIT=%%c
  )
  for /f "tokens=1,2 delims=:" %%a in ('findstr "measured_local_support_score" "%OUT_DIR%\result_summary.json"') do (
    for /f "tokens=2 delims=," %%c in ("%%b") do set SUPPORT=%%c
  )
  for /f "tokens=1,2 delims=:" %%a in ('findstr "measured_local_mean_distance_px" "%OUT_DIR%\result_summary.json"') do (
    for /f "tokens=2 delims=," %%c in ("%%b") do set AVGDIST=%%c
  )
)

echo %ITER%,%ROI_X0%,%ROI_Y0%,%ROI_X1%,%ROI_Y1%,%THRESHOLD%,%WGAP%,%HGAP%,%LINEGAP%,%FILTERPROFILE%,%POINTS%,%FIT%,%SUPPORT%,%AVGDIST%,%ACTION% >> "%PHASE_DIR%\phase3_results.csv"

if %POINTS% EQU %LAST_POINTS% (
  set /a STABLE_COUNT+=1
) else (
  set STABLE_COUNT=0
)
set LAST_POINTS=%POINTS%

if %STABLE_COUNT% GEQ 3 (
  set CONVERGED=true
  goto end_phase3
)

if %POINTS% LSS 2 (
  set /a HGAP+=10
  if %THRESHOLD% GTR 5 set /a THRESHOLD-=5
  set ACTION=increase_hgap_decrease_threshold
) else if %POINTS% GTR 50 (
  if %HGAP% GTR 10 set /a HGAP-=10
  if %THRESHOLD% LSS 30 set /a THRESHOLD+=5
  set ACTION=decrease_hgap_increase_threshold
)

if not %ACTION%==none (
  echo Action: %ACTION%
)

goto loop_phase3

:end_phase3
echo.
echo ===== Phase 3 Complete =====
echo Converged: %CONVERGED%
echo Final ROI: (%ROI_X0%,%ROI_Y0%)-(%ROI_X1%,%ROI_Y1%)
echo Final Parameters: threshold=%THRESHOLD%, wgap=%WGAP%, hgap=%HGAP%, linegap=%LINEGAP%, filterprofile=%FILTERPROFILE%
echo Results written to: %PHASE_DIR%\phase3_results.csv
endlocal