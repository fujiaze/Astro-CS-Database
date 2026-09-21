# phase3_session — Phase3 WCS/Resample/Output (L2 模块 README)

- 合同: `P3-002..006` / ALG-P3-003/004 (docs/contracts/INDEX.yaml)
- Headers: `p3_session.h`（本目录）/ `p3_wcs.h`（W4-A9 批次 1 → `lib/algorithms/projection/`）/ `p3_resample.h`（批次 2 → `lib/algorithms/resample/`，TU 暂留本库，批次 4 收口）/ `p3_output.h`（批次 3 → `lib/algorithms/fits_output/`）/ `hips_properties.h`（本目录，批次 4 → `lib/algorithms/coverage/`）
- Tests: `p3_wcs_test.cpp`（批次 1 → `lib/algorithms/projection/tests/p3wcs/`）/ `p3_interp_test.cpp` / `p3_coverage_test.cpp` / `p3_output_test.cpp` / `p3_assembly_test.cpp`（后四条在 `eng/tests/unit/`）

## 职责
> W4-A9（2026-09-17）后本目录只保留会话 facade 与尚未迁出的 `hips_properties*`；
> 下列内核的**实现文件**已按 `ASTROCS_DESIGN §7.1` 迁入各自算法模块（源逐字节等价，
> 公共符号与命名空间零改动），此处仅登记职责与去向。

p3_wcs: WCS TAN + 尺寸溢出检查 (uint64) + 配置合同上限 (ASTROCS_P3_MAX_SIDE)。→ `lib/algorithms/projection/`
p3_resample: leaf 级重采样 + 8-tile 缓存; coverage 缺失 → NaN。→ `lib/algorithms/resample/`
p3_output: FITS 原子写 + 重开验证 (dims/WCS/BUNIT/checksum/mask)。→ `lib/algorithms/fits_output/`
p3_session: facade 委托 (仅 Runtime 编排, 不复制算法)（本目录留存）。

## 合同要点
- 最大尺寸来自配置合同, 不硬编码 20000 (P3-002)。
- 失败不留貌似有效的空产物 (P3-005 原子写)。
