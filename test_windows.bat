:: Make a directory to work in
mkdir windows_build
cd windows_build

:: Configure cmake with tests enabled
cmake .. -DFIREBASE_CPP_BUILD_TESTS=ON

:: Build the SDK and the tests
cmake --build .

:: Run the tests
ctest

exit /b %ERRORLEVEL%