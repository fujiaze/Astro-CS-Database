# AstroCS Public API Standard

## 公共 API（C ABI DLL 边界）

- 头文件必须 extern "C" + 导出宏（AC_API / AIO_* / P2_API 等）。
- 每个函数 Doxygen/项目等价注释：inputs/units、outputs、ownership、
  failure（返回值语义）、thread safety、precision/lifetime。
- 禁止 magic number 状态；稳定 enum/status + 字符串 error 可并存。
- 失败时输出参数必须重置或文档化"未写入"。
- public raw pointer 必须注明 borrowed / owned / optional。

## API 契约 ID

每个稳定公共 API 关联 API-* ID（见 docs/TRACEABILITY.csv），例如
API-AIO-001（FITS/XISF/HiPS 读写）、API-P2-REJECT-001（rejection 规划）、
API-P2-UPM-001（UPM build/save/open/calibrate）。

## 变更纪律

- 接口冻结后（W2/V15/V19 冻结）禁止无声改语义；
- 行为变化：Contract first → code → tests → diagnostics → docs。
