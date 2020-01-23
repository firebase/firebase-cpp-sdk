@echo off

REM !/bin/bash
REM
REM Copyright 2019 Google LLC
REM
REM Licensed under the Apache License, Version 2.0 (the "License");
REM you may not use this file except in compliance with the License.
REM You may obtain a copy of the License at
REM
REM     http://www.apache.org/licenses/LICENSE-2.0
REM
REM Unless required by applicable law or agreed to in writing, software
REM distributed under the License is distributed on an "AS IS" BASIS,
REM WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
REM See the License for the specific language governing permissions and
REM limitations under the License.
REM
REM Builds and packs firebase unity meant to be used on a Windows environment.

SETLOCAL

SET status=0
set PROTOBUF_SRC_ROOT_FOLDER=%PROTOBUF_SRC_ROOT_FOLDER:\=/%

IF EXIST "C:\Program Files\OpenSSL-Win64" (
  SET OPENSSL_x64=C:/Program Files/OpenSSL-Win64
) ELSE (
  ECHO ERROR: Cant find open ssl x64
  EXIT /B -1
)

CALL :BUILD x64, "%OPENSSL_x64%", "-A x64"
if %errorlevel% neq 0 (SET status=%errorlevel%)

EXIT /B %status%

:BUILD
  ECHO #################################################################
  DATE /T
  TIME /T
  ECHO Building config '%~1' with option '%~3'.
  ECHO #################################################################

  @echo on

  mkdir windows_%~1
  pushd windows_%~1

  cmake .. -DFIREBASE_CPP_BUILD_TESTS=ON -DPROTOBUF_SRC_ROOT_FOLDER=%PROTOBUF_SRC_ROOT_FOLDER% -DOPENSSL_ROOT_DIR="%~2" %~3

  :: Check for errors, and return if there were any
  if %errorlevel% neq 0 (
    popd
    @echo off
    exit /b %errorlevel%
  )

  cmake --build . --config Debug

  :: Again, check for errors, and return if there were any
  if %errorlevel% neq 0 (
    popd
    @echo off
    exit /b %errorlevel%
  )

  :: Run the tests
  ctest --verbose %CTEST_SKIP_FILTER%

  popd
  @echo off
EXIT /B %errorlevel%