# 就地应用指南

## 1. 目标仓库基准

| 项 | 值 |
|---|---|
| 仓库根 | `f:\Astro dev\Astro CS Normalization Database` |
| 远端 | https://github.com/fujiaze/Astro-CS-Database.git |
| 默认分支 | main |
| 审计基准 commit | 183558ad6907d1e13a56a01c33c708913d7bbdc3 |
| 基准 commit 信息 | feat: 按Wiki规范修改K计算和CLI事件名 (2026-07-30 21:48:40 +0800) |

## 2. 应用方式

提供两种应用方式, 任选其一:

### 方式 A: 使用 git patch (推荐)

```bash
# 在仓库根目录执行
cd "f:\Astro dev\Astro CS Normalization Database"
git checkout 183558ad6907d1e13a56a01c33c708913d7bbdc3
git apply --check AstroCS_Stage1_HISS_Delivery/git_diff.patch
git apply AstroCS_Stage1_HISS_Delivery/git_diff.patch
```

**注意**: git_diff.patch 仅包含对已跟踪文件的修改, 不含新增文件。新增文件需使用方式 B 复制。

### 方式 B: 直接复制 changed_files/

```bash
# 在仓库根目录执行
# 将 changed_files/ 下的文件按相对路径复制到仓库对应位置
Copy-Item -Path "AstroCS_Stage1_HISS_Delivery\changed_files\*" -Destination . -Recurse -Force
```

`changed_files/` 目录保持仓库相对路径结构, 直接递归复制即可。

### 方式 C: 结合使用 (最完整)

1. 先应用 git_diff.patch 修改已跟踪文件
2. 再复制 changed_files/ 中的新增文件
3. 复制 wiki/ 到 Wiki 仓库 (如有)
4. 复制 tests/ 到测试目录

## 3. 删除旧文件

参见 `DELETE_LIST.txt` 获取应删除的文件列表。

当前审计结论: **暂无必须删除的文件**。Python 原型保留为迁移参考, 已归档代码保持归档状态。

## 4. 构建步骤

### 4.1 前置依赖

- 编译器: g++ 16.1.0 (MSYS2 MinGW64) 或兼容版本
- C++ 标准: C++17
- 第三方库: LZ4, Zstd (用于 codec)
- OpenMP: 必须 (校准和 Drizzle 并行化)
- Qt6 + OpenGL 3.3 Core (仅浏览器模块需要, 非 Stage1 必需)

### 4.2 构建校准模块

```bash
cd lib/calibration
./build.ps1
```
超时: 120 秒
终止条件: 编译错误或超时

### 4.3 构建 HISS I/O 模块

```bash
cd lib/astro_image_io
./build.ps1
```
超时: 120 秒
终止条件: 编译错误或超时

### 4.4 构建 Drizzle 模块

Drizzle 引擎已包含在 healpix_db 模块中, 随主构建流程编译。如需单独编译:

```bash
cd lib/healpix_db/healpix_drizzle
# 使用项目既有的 CMake 或 Makefile
```
超时: 180 秒
终止条件: 编译错误或超时

## 5. 运行代表性测试

### 5.1 正确性测试 (21 个用例)

```bash
cd lib/astro_image_io/tests
g++ -std=c++17 -O2 -fopenmp -DHAS_LZ4 -DHAS_ZSTD \
  -I../include -I../src \
  -I../../calibration/include \
  hiss_correctness_test.cpp \
  ../src/hiss_codec.cpp ../src/hiss_common.cpp \
  ../src/hiss_writer.cpp ../src/hiss_reader.cpp \
  ../../calibration/src/dark_optimizer.cpp ../../calibration/src/calibrator.cpp \
  -llz4 -lzstd -o hiss_correctness_test.exe
./hiss_correctness_test.exe
```
- 超时: 60 秒 (编译) + 30 秒 (运行)
- 终止条件: 编译错误或运行超时
- 预期结果: 21/21 通过, 退出码 0
- 测试输出: `lib/astro_image_io/tests/test_output.txt`

### 5.2 验收标准

| 测试组 | 用例数 | 验收标准 |
|--------|--------|---------|
| 校准 | 5 (01-05) | (L-D)/F 公式, [L-B-k(D-B)]/F 公式, 最优 Dark 成功/回退/硬失败 |
| Drizzle | 6 (06-11) | 通量守恒, support 量化, 自动 NSIDE |
| HISS 格式 | 10 (12-21) | 占用模式往返, 子块校验, 原子提交, ipix 恢复 |

## 6. 环境要求

| 项 | 值 |
|---|---|
| 操作系统 | Windows 11 |
| 编译器 | g++ 16.1.0 (Rev4, MSYS2 MinGW64) |
| C++ 标准 | C++17 |
| 编译选项 | -O2 -fopenmp -DHAS_LZ4 -DHAS_ZSTD |
| 链接库 | -llz4 -lzstd |
| OpenMP 线程数 | 16 (固定, 可调整) |
| PowerShell | 7 (构建脚本) |

## 7. 回滚

如需回滚变更:

```bash
cd "f:\Astro dev\Astro CS Normalization Database"
git checkout 183558ad6907d1e13a56a01c33c708913d7bbdc3 -- .
# 或
git reset --hard 183558ad6907d1e13a56a01c33c708913d7bbdc3
```

新增文件需手动删除, 参见 `MANIFEST.json` 中的文件列表。

## 8. 验证清单

应用变更后, 依次验证:

- [ ] `git status` 显示预期的修改和新增文件
- [ ] 校准模块构建成功
- [ ] HISS I/O 模块构建成功
- [ ] 正确性测试 21/21 通过
- [ ] 检查 `reports/decision_queue.md` 中的未决项
- [ ] 确认 Stage2 代码未被修改
- [ ] 确认未触发 710 帧回归
