:: Make a directory to work in
mkdir windows_build
cd windows_build

:: Configure cmake with tests enabled
:: TODO: Configure the dependencies for Database and Remote Config tests.
cmake .. -DFIREBASE_CPP_BUILD_TESTS=ON^
 -DFIREBASE_INCLUDE_REMOTE_CONFIG=OFF -DFIREBASE_INCLUDE_DATABASE=OFF

:: Check for errors, and return if there were any
if %errorlevel% neq 0 exit /b %errorlevel%

:: Build the SDK and the tests
cmake --build .

:: Again, check for errors, and return if there were any
if %errorlevel% neq 0 exit /b %errorlevel%

:: Run the tests
ctest --verbose

exit /b %ERRORLEVEL%