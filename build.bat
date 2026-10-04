@echo off
rem STRANDED: 72 Hours - build script
rem   build.bat           build the game (Release) into build\
rem   build.bat debug     build with debug info and a console window
rem   build.bat test      build and run the logic self-test
rem   build.bat run       build and start the game
rem   build.bat release   build and package dist\STRANDED-72-Hours.zip
setlocal
cd /d "%~dp0"
set PATH=C:\mingw64\bin;%PATH%
set TYPE=Release
if /I "%1"=="debug" set TYPE=Debug

cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=%TYPE% >nul || goto :fail
cmake --build build -j 8 || goto :fail

if /I "%1"=="test" (
  build\selftest.exe
  goto :eof
)
if /I "%1"=="run" (
  pushd build
  STRANDED.exe
  popd
  goto :eof
)
if /I "%1"=="release" (
  if exist dist rmdir /s /q dist
  mkdir dist\STRANDED-72-Hours
  copy /y build\STRANDED.exe dist\STRANDED-72-Hours\ >nul
  xcopy /e /i /y data dist\STRANDED-72-Hours\data >nul
  copy /y README.md dist\STRANDED-72-Hours\ >nul
  powershell -NoProfile -Command "Compress-Archive -Path dist\STRANDED-72-Hours -DestinationPath dist\STRANDED-72-Hours.zip -Force"
  echo Packaged dist\STRANDED-72-Hours.zip
  goto :eof
)
echo Built build\STRANDED.exe
goto :eof

:fail
echo Build failed.
exit /b 1
