@echo off
setlocal
cd /d "%~dp0.."
where py >nul 2>&1
if %errorlevel%==0 (
  py -3 Scripts\validate_project.py || exit /b 1
  py -3 Scripts\reference_flight_check.py || exit /b 1
  py -3 Scripts\validate_trajectory_artifacts.py || exit /b 1
  if exist Saved\TrajectoryExports\LatestTrajectory.json py -3 Scripts\validate_presentation_capture.py || exit /b 1
  if exist Saved\PerformanceCaptures\LatestPerformance.json py -3 Scripts\validate_performance_capture.py || exit /b 1
) else (
  python Scripts\validate_project.py || exit /b 1
  python Scripts\reference_flight_check.py || exit /b 1
  python Scripts\validate_trajectory_artifacts.py || exit /b 1
  if exist Saved\TrajectoryExports\LatestTrajectory.json python Scripts\validate_presentation_capture.py || exit /b 1
  if exist Saved\PerformanceCaptures\LatestPerformance.json python Scripts\validate_performance_capture.py || exit /b 1
)
echo.
echo All source-only checks passed.
pause
