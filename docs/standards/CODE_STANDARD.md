# AstroCS Code Standard

权威来源：V19R2 MASTER_CONTROL_SPEC §5；本文件为仓库内单一实现标准。

## MUST

- C++17（正式 toolchain：MSYS2 MinGW64 g++ 16.1.0）；RAII 优先。
- 公共指针必须声明所有权（borrowed / owned / optional）与空值语义。
- 分配前检查尺寸运算；禁止未检查整数乘法后直接分配。
- 禁止异常跨 C ABI；C ABI 失败时输出重置，单出口 cleanup/RAII。
- 稳定 success / recoverable science status / hard error 三层语义；
  禁止 `rc=0 + invalid status` 双语义。
- 禁止 hidden global mutable science state；科学状态只经显式 model handle。
- 禁止 silent config fallback 改变科学语义；默认值两处不一致视为缺陷。
- 禁止重复 production science implementation（单一实现 + oracle）。
- 热路径禁止 per-pixel malloc/new、per-pixel I/O、per-pixel log/clock；
  禁止重复计算 run constant；cache 必须有 capacity/identity/invalidation/thread model。
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
