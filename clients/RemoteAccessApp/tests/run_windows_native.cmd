@echo off
setlocal
if not "%PBL4_ISOLATED_WINDOWS%"=="1" (
  echo Run only in a dedicated Windows test VM and set PBL4_ISOLATED_WINDOWS=1.
  exit /b 1
)
if not defined PBL4_QT_ROOT (
  echo Set PBL4_QT_ROOT to the MinGW Qt installation directory.
  exit /b 1
)
if not defined PBL4_MINGW_ROOT (
  echo Set PBL4_MINGW_ROOT to the matching MinGW compiler directory.
  exit /b 1
)
if not defined PBL4_NATIVE_BUILD_DIR set "PBL4_NATIVE_BUILD_DIR=%TEMP%\pbl4-windows-native-build"
set "PATH=%PBL4_MINGW_ROOT%\bin;%PBL4_QT_ROOT%\bin;%PATH%"
if defined PBL4_CMAKE_ROOT set "PATH=%PBL4_CMAKE_ROOT%\bin;%PATH%"
cmake -S "%~dp0.." -B "%PBL4_NATIVE_BUILD_DIR%" -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=%PBL4_QT_ROOT%" "-DPBL4_LRELEASE=%PBL4_QT_ROOT%\bin\lrelease.exe" -DBUILD_TESTING=ON -DENABLE_WINDOWS_INPUT_INTEGRATION=ON
if errorlevel 1 exit /b 2
cmake --build "%PBL4_NATIVE_BUILD_DIR%" -j 2
if errorlevel 1 exit /b 3
set "QT_PLUGIN_PATH=%PBL4_QT_ROOT%\plugins"
set QT_QPA_PLATFORM=windows
set QT_SCALE_FACTOR=1
"%PBL4_NATIVE_BUILD_DIR%\WindowsInputIntegrationTest.exe" -nocrashhandler > "%PBL4_NATIVE_BUILD_DIR%\native-100.log" 2>&1
if errorlevel 1 (
  type "%PBL4_NATIVE_BUILD_DIR%\native-100.log"
  exit /b 4
)
set QT_SCALE_FACTOR=2
"%PBL4_NATIVE_BUILD_DIR%\WindowsInputIntegrationTest.exe" -nocrashhandler > "%PBL4_NATIVE_BUILD_DIR%\native-200.log" 2>&1
if errorlevel 1 (
  type "%PBL4_NATIVE_BUILD_DIR%\native-200.log"
  exit /b 5
)
set QT_SCALE_FACTOR=1
set QT_QPA_PLATFORM=offscreen
ctest --test-dir "%PBL4_NATIVE_BUILD_DIR%" --output-on-failure
if errorlevel 1 exit /b 6
echo PASS: Windows native 100%%/200%% and desktop CTest suite
exit /b 0
