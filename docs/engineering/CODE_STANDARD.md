# Astro Celestial Sphere Database（ACSD） Code Standard

> 上游：ASTROCS_DESIGN.md §8.4（模块与 ABI）

权威来源：`ENGINEERING_SPEC.md` §1（C++17 双平台工具链：**Windows = MSVC v143**；Linux = GCC 或 Clang）
与 §4（每模块必备项）+ `ASTROCS_DESIGN.md` §10.2（官方 Windows 工具链 = MSVC）。本文件是这些要求的实现级展开。

## MUST

- C++17（**正式 toolchain：Windows = MSVC v143；Linux = GCC 或 Clang**，见 `ENGINEERING_SPEC.md` §1
  与 `ASTROCS_DESIGN.md` §10.2「官方 Windows 工具链 = MSVC」）；RAII 优先。
  MinGW64 的定位 = **本地开发/兼容性验证**工具链；正式工具链取上条所列工具链、
  发布物构建依据同样取正式工具链。
- 公共指针必须声明所有权（borrowed / owned / optional）与空值语义。
- 分配前检查尺寸运算；整数乘法一律经检查后再分配。
- 异常边界 = C++ 侧；C ABI 失败时输出重置，单出口 cleanup/RAII。
- 稳定 success / recoverable science status / hard error 三层语义；
  三层语义各占其一（`rc=0 + invalid status` 即双语义违规）。
- 科学状态只经显式 model handle；hidden global mutable science state 不作承载面。
- config fallback 一律显式并保持科学语义；默认值两处不一致视为缺陷。
- production science implementation 单一（单一实现 + oracle）。
- 热路径按无 per-pixel malloc/new、无 per-pixel I/O、无 per-pixel log/clock 的口径编写；
  run constant 只计算一次；cache 必须有 capacity/identity/invalidation/thread model。
- 每个 fast path 必须有 reference path + equivalence oracle + failure fallback。
- 每个数值科学量文档化单位/坐标系/归一化/精度需求/有限域。
- 外部进程/网络/硬件等待必须显式 timeout。
- science product 写盘：temp write → validate → atomic promote。

## SHOULD

- immutable run context；thread-local 可复用 scratch。
- typed config；enum/status 替代 magic number；窄接口。
- 确定性测试。

## MAY

- compatibility reader（仅当与 production solver/writer 隔离）。

## 关联

- docs/standards/NUMERIC_STANDARD.md、C_ABI_STANDARD.md、
  CONCURRENCY_STANDARD.md、ERROR_HANDLING_STANDARD.md、IO_STANDARD.md。
