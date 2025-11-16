@echo off
echo Building Binance Order Book Demo...

REM Create build directory
if not exist build mkdir build
cd build

REM Configure with CMake
echo Configuring with CMake...
cmake .. -G "MinGW Makefiles"

if %errorlevel% neq 0 (
    echo CMake configuration failed. Trying with Visual Studio generator...
    cmake ..
)

REM Build the project
echo Building project...
cmake --build .

if %errorlevel% equ 0 (
    echo Build completed successfully!
    echo Executable location: build\bin\BinanceOrderBookDemo.exe
) else (
    echo Build failed!
)

cd ..