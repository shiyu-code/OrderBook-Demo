#!/bin/bash

# Binance Order Book Demo Test Script
# This script demonstrates the key features of the application

echo "=== Binance Order Book Demo Test Suite ==="
echo

# Check if the executable exists
if [ ! -f "bin/BinanceOrderBookDemo" ]; then
    echo "Error: BinanceOrderBookDemo executable not found!"
    echo "Please build the project first with: cmake --build ."
    exit 1
fi

echo "1. Testing help functionality..."
./bin/BinanceOrderBookDemo --help
echo

echo "2. Testing basic strategy (5 seconds)..."
timeout 10s ./bin/BinanceOrderBookDemo --duration 5 --strategy basic
echo

echo "3. Testing strategy comparison (5 seconds per strategy)..."
timeout 30s ./bin/BinanceOrderBookDemo --compare --duration 5
echo

echo "4. Testing different symbols..."
echo "Testing BTCUSDT..."
timeout 10s ./bin/BinanceOrderBookDemo --symbol BTCUSDT --duration 3
echo

echo "Testing ETHUSDT..."
timeout 10s ./bin/BinanceOrderBookDemo --symbol ETHUSDT --duration 3
echo

echo "=== Test Suite Completed ==="
echo "Note: The demo uses simulated data for demonstration purposes."
echo "In a real deployment, it would connect to the actual Binance WebSocket API."