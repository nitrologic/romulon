@echo off

rem rmdir /s /q bin 2>nul
rem mkdir bin

if not exist bin mkdir bin
pushd bin

resetcoms
cmake -G Ninja ..
ninja && IF EXIST e: copy romulon.uf2 e:

if errorlevel 1 (popd & exit /b 1)

popd

echo Done.
