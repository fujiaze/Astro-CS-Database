# eng/tests/integration/p2_integrate — P2-INTEGRATE-001 端到端集成测试

写域：`eng/tests/integration/p2_integrate/`。自包含构建：

```bash
cmake -S eng/tests/integration/p2_integrate -B <build> -DCMAKE_BUILD_TYPE=Release
cmake --build <build> -j4
ctest --test-dir <build> --output-on-failure
```

## 用例

| ctest | 内容 |
|---|---|
| `p2_routing` | 三模式路由：production 接受；`psf_snr_power`/`auto`/`support_x_snr2`/`0`/未知 REJECT；`equal`/`pixel_ivar` 为 baseline |
| `p2_write` | 真实 Phase1 产物 → point/surface/psfsw 三模式 → 写盘 → 重开校验；相关帧联合 C；UPM/REJ/SAMP 接线 |
| `p2_negative` | 逐条违反冻结即红（12 记录 mutation + FITS bit flip + 相关帧朴素 Σ + pixel-ivar epsilon/结构门 + 单帧不足），含未篡改正控制 |
| `p2_halfproduct` | 重开验证注入失败 / BUNIT 不可判 → 无可见半成品、无 staging 残留 |
| `p2_oracle` | 独立 NumPy Oracle：不调用被测实现，重算 Q/W、联合 GLS、surface GLS、psfsw C_out；结构门 + non-vacuity mutation |

## 独立 Oracle 边界

`oracle/p2_oracle.py` 只用 NumPy/Astropy 从磁盘 FITS + 输入原值重算；不 import/link/exec 任何被测实现。
它同时执行 (`non-vacuity`)：对副本注入 `variance_from=psfsw_robust_weight` 与 P33 键，断言自身检查器变红。

## 证据

`EVIDENCE.md` 记录实测命令、rc、用例数与通过数、负向 mutation 明细。运行产物落
`run/quality/P2-INTEGRATE-001/`（gitignore）。
