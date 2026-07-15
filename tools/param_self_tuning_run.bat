@echo off
setlocal

echo ===== Param Self-Tuning Runner =====
echo.

echo Step 1: Running Phase 1 - Gauge ROI Scan
call "%~dp0param_self_tuning_phase1.bat"
echo.

echo Step 2: Running Phase 2 - Parameter Grid Search
call "%~dp0param_self_tuning_phase2.bat"
echo.

echo Step 3: Running Phase 3 - Closed-Loop Iteration
call "%~dp0param_self_tuning_phase3.bat"
echo.

echo ===== All Phases Complete =====
echo Results are in: D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxscript_runs\param_self_tuning
echo.
echo Next steps:
echo 1. Analyze phase1_results.csv to find best ROI
echo 2. Analyze phase2_results.csv to find best parameters
echo 3. Analyze phase3_results.csv to check convergence
echo 4. Open software to verify best candidate manually
echo 5. Only freeze as baseline after human review
endlocal