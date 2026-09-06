@echo off
setlocal
cd /d "%~dp0.."
where py >nul 2>&1
if %errorlevel%==0 (
  py -3 Scripts\validate_dg_session9_brand_license.py --self-test || exit /b 1
  py -3 Scripts\validate_dg_session13_course_authoring_pcg.py --self-test || exit /b 1
  py -3 Scripts\validate_dg_session13_course_authoring_pcg.py || exit /b 1
  py -3 Scripts\validate_dg_session14_career_ai.py --self-test || exit /b 1
  py -3 Scripts\validate_dg_session14_career_ai.py || exit /b 1
  py -3 Scripts\validate_dg_session15_vertical_slice.py --self-test || exit /b 1
  py -3 Scripts\validate_dg_session15_vertical_slice.py || exit /b 1
  py -3 Scripts\validate_dg_session16_core_playability.py --self-test || exit /b 1
  py -3 Scripts\validate_dg_session16_core_playability.py || exit /b 1
  py -3 Scripts\validate_dg_session18_poly_haven_provenance.py --self-test || exit /b 1
  py -3 Scripts\validate_dg_session18_poly_haven_provenance.py || exit /b 1
  py -3 Scripts\validate_dg_session19_baked_pcg.py --self-test || exit /b 1
  py -3 Scripts\validate_dg_session19_baked_pcg.py || exit /b 1
  py -3 Scripts\validate_dg_session19_release_scope.py --self-test || exit /b 1
  py -3 Scripts\validate_dg_session19_release_scope.py || exit /b 1
  py -3 Scripts\validate_project.py || exit /b 1
  py -3 Scripts\reference_flight_check.py || exit /b 1
  py -3 Scripts\validate_trajectory_artifacts.py || exit /b 1
  if exist Saved\TrajectoryExports\LatestTrajectory.json py -3 Scripts\validate_presentation_capture.py || exit /b 1
  if exist Saved\PerformanceCaptures\LatestPerformance.json py -3 Scripts\validate_performance_capture.py || exit /b 1
) else (
  python Scripts\validate_dg_session9_brand_license.py --self-test || exit /b 1
  python Scripts\validate_dg_session13_course_authoring_pcg.py --self-test || exit /b 1
  python Scripts\validate_dg_session13_course_authoring_pcg.py || exit /b 1
  python Scripts\validate_dg_session14_career_ai.py --self-test || exit /b 1
  python Scripts\validate_dg_session14_career_ai.py || exit /b 1
  python Scripts\validate_dg_session15_vertical_slice.py --self-test || exit /b 1
  python Scripts\validate_dg_session15_vertical_slice.py || exit /b 1
  python Scripts\validate_dg_session16_core_playability.py --self-test || exit /b 1
  python Scripts\validate_dg_session16_core_playability.py || exit /b 1
  python Scripts\validate_dg_session18_poly_haven_provenance.py --self-test || exit /b 1
  python Scripts\validate_dg_session18_poly_haven_provenance.py || exit /b 1
  python Scripts\validate_dg_session19_baked_pcg.py --self-test || exit /b 1
  python Scripts\validate_dg_session19_baked_pcg.py || exit /b 1
  python Scripts\validate_dg_session19_release_scope.py --self-test || exit /b 1
  python Scripts\validate_dg_session19_release_scope.py || exit /b 1
  python Scripts\validate_project.py || exit /b 1
  python Scripts\reference_flight_check.py || exit /b 1
  python Scripts\validate_trajectory_artifacts.py || exit /b 1
  if exist Saved\TrajectoryExports\LatestTrajectory.json python Scripts\validate_presentation_capture.py || exit /b 1
  if exist Saved\PerformanceCaptures\LatestPerformance.json python Scripts\validate_performance_capture.py || exit /b 1
)
echo.
echo All source-only checks passed.
pause
