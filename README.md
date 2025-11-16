# Binance订单簿演示程序

一个用于通过WebSocket API获取和分析Binance订单簿数据的C++实现，具有性能优化策略。

## 功能特性

- 从Binance WebSocket API实时获取订单簿数据
- 延迟分析和统计收集
- 多种WebSocket优化策略
- 性能对比和报告

## 依赖项

- CMake 3.16+
- C++17兼容编译器
- Windows: MinGW或Visual Studio
- Linux: GCC 7+或Clang 5+

## 构建方法

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

## 使用方法

```bash
./bin/BinanceOrderBookDemo
```

## 项目结构

```
├── include/          # 头文件
├── src/             # 源文件
├── build/           # 构建输出
└── CMakeLists.txt   # 构建配置
```