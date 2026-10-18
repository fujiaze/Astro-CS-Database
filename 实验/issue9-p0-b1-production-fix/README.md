# Issue #9（P0-B1）生产修复证据

**基线**：`b8dc684b7`（施工期间另一会话把 #80 P0-A1 与 B09-D2 提交并合入 origin/main；
本单元全部证据在新基线上重跑复现，见下）。施工前工作树含另一会话未提交的 #87/E2-R
改动，未触碰其语义，也不随本提交落盘。
**结论**：Normalize 施加 Gaia XP 测光标度后，signal/variance/ivar/F_ref/模型因子与
`properties`/`p1_stack.json`/`frame_science.json`/P2/P3 的单位身份全部成对；未标定帧
仍为 `ADU/sr`，标定帧为 `XPMODEL/sr`；混合标度的 P2 合成显式 fail-closed。

## 1. 复算命令（真实生产 CLI，非合成夹具）

```bash
# 输入身份 = 同一原始亮帧 + 同一 master bias/dark/flat
# testdata/M42_T2T3_mosaic_Flying_dutchman/T2/M1/M42_M1_T2_flying_dutchman-20251212@012404-300S-Red.fts

./build/linux-sci/acsd normalize --json run/issue9-ab/native.json        -y   # 无测光通道
./build/linux-sci/acsd normalize --json run/issue9-ab/applied.json       -y   # Gaia XP + 空间增益 order=2
./build/linux-sci/acsd normalize --json run/issue9-ab/applied-const.json -y   # Gaia XP + spatial_gain_order=0
./build/linux-sci/acsd export  --json run/issue9-ab/export-<tag>.json    -y
./build/linux-sci/acsd mosaic  --json run/issue9-ab/mosaic-mixed.json    -y   # 负例（native+applied 混帧）

# 机器证据（本目录 evidence/issue9_ab_evidence.json 即其输出）
python3 实验/issue9-p0-b1-production-fix/code/verify_issue9_ab.py \
    > 实验/issue9-p0-b1-production-fix/evidence/issue9_ab_evidence.json
```

## 2. 单位身份 A/B（真实产物读回）

| 面 | native | applied / applied-const |
|---|---|---|
| `p1_stack.json` bunit / linear_unit / scale_state | `ADU/sr` / `ADU` / `native_calibrated` | `XPMODEL/sr` / `XPMODEL` / `relative_scaled` |
| `frame_science.signal` bunit / linear_unit | `ADU/sr` / `ADU` | `XPMODEL/sr` / `XPMODEL` |
| `frame_science.reference.flux_unit` | `ADU` | `XPMODEL`（+ `response_id`） |
| `signal/properties` BUNIT | `ADU/sr` | `XPMODEL/sr` |
| `variance/properties` BUNIT | `ADU^2/sr^2` | `XPMODEL^2/sr^2` |
| `ivar/properties` BUNIT | `sr^2/ADU^2` | `sr^2/XPMODEL^2` |
| 模型因子 `factor_variance_unit` / `derived_covariance_unit` | `ADU^2` / `ADU^2/sr^2` | `XPMODEL^2` / `XPMODEL^2/sr^2` |
| P3 `output_phase3.fits` BUNIT（Astropy 读回） | `ADU/sr` | `XPMODEL/sr` |
| P3 `p3_props.json` bunit / bunit_input | `ADU/sr` / `ADU/sr` | `XPMODEL/sr` / `XPMODEL/sr` |

## 3. 数值/方差律（统计分母 = 同 Norder9 tile 双方有限且 |native|>1e-30 的配对叶，n≈2.39e7）

`α = frame_photscal = 1.5242794038463092e-17`（`applied-const/work/p1_phot.json`）。

| 量 | applied/native 中位 | applied-const/native 中位 | 期望 |
|---|---|---|---|
| signal | `1.527316623e-17` | `1.524279404e-17` | `α`（applied 含空间增益 m(x,y)，故偏离常量 α） |
| variance | `2.272808936e-34` | `2.323427525e-34` | `α²` |
| ivar | `4.399842136e+33` | `4.303986198e+33` | `α⁻²` |

逐像元 `r_v = r_s²` 偏差（`|r_v−r_s²|/r_s²`）：

* `applied-const`：p50 `1.6e-7`、p99 `5.2e-7`（= float32 存储精度；律成立）。
* `applied`：p50 `6.3e-2`、p99 `2.4e-1`（**预期**：空间增益 m(x,y) 使同一叶的贡献像元
  具有不同 α_j，叶级 signal 是 α 的加权平均、variance 是 Σw²α_j² 的组合，二者不相等；
  α² 律是**逐源像元**律，不是逐输出叶律）。

## 4. 失败语义（不倒退）

| 负例 | 结果 |
|---|---|
| P1 signal/properties 缺 `BUNIT`（其余单位键保留）→ `acsd export` | exit **3**，`P3-INPUT-BUNIT-MISSING`，**不产** `output_phase3.fits` |
| P2 被消费帧 native+applied 混合 → `acsd mosaic` | exit **2**，`consumed frames declare incompatible signal units (canonical 'ADU/sr' vs 'XPMODEL/sr' …)`，`p2_samples.json` 不产 |
| 单元测试：`run_p3_input_unit_guard_test.py` | 37/37 PASS（含 XPMODEL 合法/裸 XPMODEL/跨基混合/缺声明等） |
| 单元测试：`run_p3_rsmp_units_test.py` | 33/33 PASS（canonical 链、二次律、跨基不可判） |

## 5. 未覆盖 / 缺口

* 真实观测只有单帧；P2 完整合成（≥2 帧）未跑通（单帧无控制点 → 天空面不可建），
  故 P2 只验证到 `sample` 节点的单位准入与负例，`write` 节点的 `XPMODEL/sr` 落盘
  由代码路径 + `p2_final.json` 单元字段保证，未取得完整 mosaic 产物。
* `hiss_writer.cpp` 白名单、`aio_healpix_io.cpp` 硬编码 `ADU/sr`、`aio/product_io`
  单位代数、`phase1_product`/`phase2_integrate` 与已退役 `p3_export.cpp` 未改；
  这些入口在当前 `acsd normalize/mosaic/export` 生产调用面上不可达，登记为 finding。
