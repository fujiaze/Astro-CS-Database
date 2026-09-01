# P3-005: FITS 输出校验与 provenance

任务 ID: P3-005
Gate: G6
依赖: P3-001; RT-007
平台: Linux
变更类别: algorithm

## 目标

按 V6.1 控制包 `04_TASK_SPECIFICATIONS.md` P3-005：

> request `-32/-64` 必须真实决定 BITPIX 和 buffer；写 CRPIX/CRVAL/CD/CTYPE/CUNIT/
> BUNIT/coverage/provenance。

## 验收项与实现对照

| 验收项 | 实现 | 证据 |
|---|---|---|
| bitpix -32/-64 真实决定 buffer | p3_output_write_atomic 加 bitpix 参数(校验 -32|-64); session 传 request bitpix; 输出 BITPIX 实测匹配 + dtype itemsize 匹配 | c01 #1/#2 |
| CRPIX/CRVAL/CD/CTYPE/CUNIT | FITS header 关键字齐全(独立 astropy 读取) | c01 #3 |
| BUNIT | "ADU"(面亮度, 非 Jy/beam); BSCALE/BZERO | c01 #3 |
| coverage | 扩展 HDU 存在 | c01 #3 |
| provenance | HIPSID/ORDERSEL/SAMPLER/SWVER/RUNID | c01 #3 |
| 值 roundtrip | f64 精度保留覆盖区正值 | c01 #4 |

## 实现文件

- `lib/phase3_session/p3_output.h/.cpp`：p3_output_write_atomic 加 bitpix 参数(原硬编码 -32)
- `lib/phase3_session/p3_session.cpp`：传 request bitpix 到 write
- `tests/backend/p3_output_probe_main.cpp`：probe 加 bitpix 参数(默认 -32)
- `tests/unit/p3_output_test.cpp`、`p3_assembly_test.cpp`：调用补 bitpix=-32
- `tests/backend/test_p3005_fits_output.py`（新）：4 组断言

## 测试结果

- `test_p3005_fits_output.py`: 4/4 PASS
- `test_p3_output.py`: 6/6 PASS(回归); `ctest`: 56/56 PASS

## 说明

- 缺陷修复: p3_output_write_atomic 曾硬编码 BITPIX=-32, request -64 不生效; 已修复。
- FITS 数据为 big-endian(FITS 标准), 断言用 itemsize 而非 dtype 相等。
