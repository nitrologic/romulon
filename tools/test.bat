set erroe

mkdir bin
pushd bin
cmake -G Ninja ..
ninja 

if %errorlevel% neq 0 (
    echo cmake ninja failure errorlevel:%errorlevel%
    popd
    exit /b %errorlevel%
)

rem reset.exe
rem echo.exe
rpc.exe
rem win32tool.exe
rem tx "{\"jsonrpc\":\"2.0\",\"method\":\"rtc.set\",\"params\":{\"time\":347155200},\"id\":1}"
popd
