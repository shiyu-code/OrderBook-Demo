@echo off
echo === Binance Order Book Demo Test Suite ===
echo.

REM Check if the executable exists
if not exist "bin\BinanceOrderBookDemo.exe" (
    echo Error: BinanceOrderBookDemo executable not found!
    echo Please build the project first with: cmake --build .
    exit /b 1
)

echo 1. Testing help functionality...
bin\BinanceOrderBookDemo.exe --help
echo.

echo 2. Testing basic strategy (5 seconds)...
timeout /t 10 >nul 2>nul || ping -n 10 127.0.0.1 >nul
start /wait bin\BinanceOrderBookDemo.exe --duration 5 --strategy basic
echo.

echo 3. Testing strategy comparison (5 seconds per strategy)...
timeout /t 30 >nul 2>nul || ping -n 30 127.0.0.1 >nul
start /wait bin\BinanceOrderBookDemo.exe --compare --duration 5
echo.

echo 4. Testing different symbols...
echo Testing BTCUSDT...
timeout /t 10 >nul 2>nul || ping -n 10 127.0.0.1 >nul
start /wait bin\BinanceOrderBookDemo.exe --symbol BTCUSDT --duration 3
echo.

echo Testing ETHUSDT...
timeout /t 10 >nul 2>nul || ping -n 10 127.0.0.1 >nul
start /wait bin\BinanceOrderBookDemo.exe --symbol ETHUSDT --duration 3
echo.

echo === Test Suite Completed ===
echo Note: The demo uses simulated data for demonstration purposes.
echo In a real deployment, it would connect to the actual Binance WebSocket API.
pause