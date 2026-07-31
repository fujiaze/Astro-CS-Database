# 实验环境信息

## 编译器

- 编译器: GCC 16.1.0
- C++ 标准: C++17
- 优化级别: -O2
- 编译宏: HAS_LZ4 HAS_ZSTD

## 压缩库

- LZ4: 可用
- Zstd: 可用
- RAW: 始终可用 (内置)

## 操作系统

- OS: Windows
- 平台: x86_64
- 详情: (见系统属性)

## 硬件

- CPU 核心数: 16
- CPU 架构: x86_64
- 总物理内存: 65446 MB
- 可用物理内存: 44354 MB

## 实验参数

- 预热轮数: 3
- 测量轮数: 10 (取中位数)
- 数据集: 高分辨率大Tile / 常规中心 / 边缘部分覆盖 / 极稀疏

## 复现命令

```bash
g++ -std=c++17 -O2 -DHAS_LZ4 -DHAS_ZSTD \
    -I../include hiss_benchmark.cpp ../src/hiss_codec.cpp \
    -llz4 -lzstd -lpsapi -o hiss_benchmark.exe
./hiss_benchmark.exe [output_dir]
```
