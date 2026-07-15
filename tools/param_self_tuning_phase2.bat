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

set METHOD=0
set TOOL_HALF_WIDTH=50

set PHASE_DIR=%RUN_ROOT%\phase2\%RUN_ID%
mkdir "%PHASE_DIR%" 2>nul

echo ===== Phase 2: Parameter Grid Search =====
echo Run ID: %RUN_ID%
echo ROI: (%ROI_X0%,%ROI_Y0%)-(%ROI_X1%,%ROI_Y1%)
echo Method: %METHOD%, ToolHalfWidth: %TOOL_HALF_WIDTH%
echo Coarse grid: threshold=10,20,30; wgap=8,14,20; hgap=20,40,60; linegap=6,12; filterprofile=0,1
echo.

echo candidate_id,threshold,wgap,hgap,linegap,filterprofile,points,fit,support,avgdist,timeout,failure_stage > "%PHASE_DIR%\phase2_results.csv"

set CANDIDATE_ID=0

for %%t in (10 20 30) do (
  for %%w in (8 14 20) do (
    for %%h in (20 40 60) do (
      for %%l in (6 12) do (
        for %%f in (0 1) do (
          set /a CANDIDATE_ID+=1
          set CASE_ID=candidate_!CANDIDATE_ID!
          set OUT_DIR=%PHASE_DIR%\!CASE_ID!
          mkdir "!OUT_DIR!" 2>nul

          echo Running: !CASE_ID! - threshold=%%t, wgap=%%w, hgap=%%h, linegap=%%l, filterprofile=%%f

          "%EXE%" ^
            --cxscript-headless ^
            --image "%IMAGE%" ^
            --script "%SCRIPT%" ^
            --out "!OUT_DIR!" ^
            --case-name "!CASE_ID!" ^
            --roi-x0 %ROI_X0% ^
            --roi-y0 %ROI_Y0% ^
            --roi-x1 %ROI_X1% ^
            --roi-y1 %ROI_Y1% ^
            --threshold %%t ^
            --wgap %%w ^
            --hgap %%h ^
            --linegap %%l ^
            --method %METHOD% ^
            --filterprofile %%f ^
            --tool-half-width %TOOL_HALF_WIDTH%

          set POINTS=0
          set FIT=false
          set SUPPORT=0.0
          set AVGDIST=0.0
          set TIMEOUT=false
          set FAILURE_STAGE=none

          if exist "!OUT_DIR!\result_summary.json" (
            for /f "tokens=1,2 delims=:" %%a in ('findstr "valid_line_points_count" "!OUT_DIR!\result_summary.json"') do (
              for /f "tokens=2 delims=," %%c in ("%%b") do set POINTS=%%c
            )
            for /f "tokens=1,2 delims=:" %%a in ('findstr "has_fit_line" "!OUT_DIR!\result_summary.json"') do (
              for /f "tokens=2 delims=," %%c in ("%%b") do set FIT=%%c
            )
            for /f "tokens=1,2 delims=:" %%a in ('findstr "measured_local_support_score" "!OUT_DIR!\result_summary.json"') do (
              for /f "tokens=2 delims=," %%c in ("%%b") do set SUPPORT=%%c
            )
            for /f "tokens=1,2 delims=:" %%a in ('findstr "measured_local_mean_distance_px" "!OUT_DIR!\result_summary.json"') do (
              for /f "tokens=2 delims=," %%c in ("%%b") do set AVGDIST=%%c
            )
          )

          echo !CASE_ID!,%%t,%%w,%%h,%%l,%%f,!POINTS!,!FIT!,!SUPPORT!,!AVGDIST!,!TIMEOUT!,!FAILURE_STAGE! >> "%PHASE_DIR%\phase2_results.csv"
        )
      )
    )
  )
)

echo.
echo ===== Phase 2 Complete =====
echo Results written to: %PHASE_DIR%\phase2_results.csv
endlocal