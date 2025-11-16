#!/bin/bash

echo "Building Binance Order Book Demo..."

# Create build directory
mkdir -p build
cd build

# Configure with CMake
echo "Configuring with CMake..."
cmake ..

# Build the project
echo "Building project..."
make -j$(nproc)

if [ $? -eq 0 ]; then
    echo "Build completed successfully!"
    echo "Executable location: build/bin/BinanceOrderBookDemo"
else
    echo "Build failed!"
fi

cd ..