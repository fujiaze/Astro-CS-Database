# phase3_session — Phase3 WCS/Resample/Output (L2 模块 README)

- 合同: `P3-002..006` / ALG-P3-003/004 (docs/contracts/INDEX.yaml)
- Headers: `p3_wcs.h` / `p3_resample.h` / `p3_output.h` / `p3_session.h`
- Tests: `tests/unit/p3_wcs_test.cpp` / `p3_interp_test.cpp` / `p3_coverage_test.cpp` / `p3_output_test.cpp` / `p3_assembly_test.cpp`

## 职责
p3_wcs: WCS TAN + 尺寸溢出检查 (uint64) + 配置合同上限 (ASTROCS_P3_MAX_SIDE)。
p3_resample: leaf 级重采样 + 8-tile 缓存; coverage 缺失 → NaN。
p3_output: FITS 原子写 + 重开验证 (dims/WCS/BUNIT/checksum/mask)。
p3_session: facade 委托 (仅 Runtime 编排, 不复制算法)。

## 合同要点
- 最大尺寸来自配置合同, 不硬编码 20000 (P3-002)。
- 失败不留貌似有效的空产物 (P3-005 原子写)。
