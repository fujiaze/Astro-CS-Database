# RELEASE-02 HUB-A 报告 — 汇合点接线（module_adapters.cpp）

> 执行者：hub 接线实现者（HUB-A）。工作目录：/workspace/Astro CS Database。
> 时点：2026-09-18。零 git 写；**未跑 ninja/cmake/ctest**（硬要求），验证 = `g++ -fsyntax-only`。
> 主文件面：`lib/infrastructure/scheduler/src/module_adapters.cpp`（+ `lib/algorithms/coverage/src/stage2_common.cpp`、
> `lib/algorithms/coverage/tools/rejection_cli.cpp`）。另 **越面必要 1 处**：根 `CMakeLists.txt`（权重链源码/头登记，见 §4.4）。
> 证据：`run/RELEASE-02/hub-a/logs/syntax_gate.log`（3 TU rc=0，仅 1 条既有 warning :3819）。

---

## 0 结论摘要

| 项 | 内容 | 状态 |
|---|---|---|
| ① | 排异**逐输出像素几何 n** 路由（support>0 计数 → `p2_reject_plan_resolve_n`，按 n 缓存） | **已闭合（代码）** |
| ② | n=2 档**外部先验** prior_sigma+prior_sky（该帧该 tile 31×31 邻域中位数/MAD，center_mode=0 强制） | **已闭合（代码）** |
| ③ | 稀疏天光面构建（upm-fit）+ 逐像素扣除（upm-apply）接生产 mosaic 链 | **已闭合（代码）** |
| ④ | HiPS 头帧级 SNR → 逆方差权重链；删除 `unit_weight_degraded` 成功路径 | **已闭合（代码，数据面依赖未冻结键）** |
| ⑤ | 方法表/profile/undet_n、p3 原子提交、output_mode、coordinate_frame | 部分完成（见 §5） |
| ⑤ | 4 个 unknown key（CLI 面，禁改）、p3_output 直调 CFITSIO（面外） | **转交**（见 §5.5/§5.6） |

**科学语义零放宽**：未改任何阈值/判据/冻结枚举；未删检查；失败一律显式 fail-closed。

---

## 1 ① 逐像素几何 n 路由（DESIGN §4.5）

### 1.1 旧缺陷
`p2_op_reject` 旧行为（改动前 `:4414-4418`）以 `req.nominal_contributors = frames.size()`
**整组帧数**解析一次 plan（wbpp_current group-level）⇒ 全图一个方法，等价于不按像素路由。

### 1.2 新接线约定
| 环节 | 位置 | 约定 |
|---|---|---|
| 几何 n 来源 | `:4932-4943` | 逐帧 `aio_hips_open(hips_path, AIO_HIPS_RD_SUPPORT)`；**不得**用 corrected 面 finiteness（无法区分"无覆盖"与"覆盖但信号非有限"），**不得**用 `frames.size()`，**不得**用 eligible_count |
| 逐 tile 覆盖 | `:5015-5028` | 逐帧读该 tile support 平面（`aio_hips_read_tile_f32`） |
| 逐像素 n | `:5038-5044` | `geom_n = #{d | isfinite(sup_d[p]) && sup_d[p]>0}`（coverage 覆盖图 = 帧 footprint 覆盖计数） |
| plan 解析 | `:4905-4930` | `p2_reject_plan_resolve_n(n, &req, &p, err, cap)`，n=0..n_max 预解析 + `std::map<n,plan>` 缓存（纯函数，1/N worker 一致）；`req.profile="astrocs_adaptive_pixel"`、`req.underdetermined_n=1`、`req.request=P2_REJECT_AUTO` |
| gate | `:5045, 5073-5075` | `eligible_count > plan.underdetermined_n && eligible_count >= plan.minimum_n`（plan = 该像素 geom_n 档） |
| provenance | `:5170-5208` | 增 `plans[]`（逐 n：nominal_n/method/semantic_id/minimum_n/underdetermined_n/normalization）；`plan` 摘要增 `nominal_n`/`semantic_id`/`fallback`；`geometric_n_source="frame_support_gt0"`；`prior_unavailable_pixels` |

**`plan` 摘要口径**：`plan` = 最大几何 n 档（确定性；逐 n 全表见 `plans`），
`man.reject_semantic_id` 与之同源（保持既有 artifact↔manifest 一致性断言）。

**语义**：n≤1 → none/UNDERDETERMINED；n=2 → extreme_value_clip_prior_sigma；
3..7 → percentile；8..15 → winsorized；≥16 → linear_fit（kernel 冻结映射，未改）。

---

## 2 ② n=2 档外部先验（FIX-REJ §8 方案 A / REJ-kernel §7）

### 2.1 陷阱与规避
n=2 若只给 `prior_sigma` 不给 `prior_sky`，kernel 会回退**候选栈中位数**（center_mode=1），
单离群使两侧同时超阈 → 全拒 → 触发 n≤4 全接受容错 ⇒ **排异反转成全接受**。

### 2.2 接线约定
| 环节 | 位置 | 约定 |
|---|---|---|
| 强制外部中心 | `:4914-4918`（plan 缓存内） | 对 `method==P2_REJECT_EXTREME_VALUE_PRIOR_SIGMA` 的 plan 置 `extreme_prior.center_mode=0` ⇒ 缺 prior_sky 即 kernel `extreme_prior_valid=false` → UNDERDETERMINED（**绝不回退栈中位数**） |
| 先验计算 | `:4829-4877` `p2_reject_local_prior` | 逐 eligible 样本，取 `src_idx[s]` 对应帧该 tile 内以输出像素为中心的 **31×31 邻域**（clipped 到 tile 边界，仅 finite），`prior_sky=median`、`prior_sigma=1.4826·MAD`（**同源**）；有效邻域样本 < 9 → 返回 false |
| 紧凑序对齐 | `:5084-5098` | `prior_sigma[s]/prior_sky[s]` 按 **eligibility 紧凑序**（与 `stack.values` 同序，用 `src_idx` 映射到原始帧 slot）；任一不可得 → 不传数组（kernel UNDERDETERMINED），并 `++prior_unavailable_pixels` |
| provenance | `:5180-5181, 5193` | `fallback="prior_sigma_unavailable"`（有不可得像素时）/ `"none"` |

**tile 宽度**：`tile_span != 512×512` 即 DATA fail-closed（`:4900-4903`）——邻域与几何 n 都要求标准 HiPS tile。

**性能提示（未闭合，需 benchmark；前台注意）**：先验为**逐像素** 31×31 中位数/MAD
（O(n=2 像素 × 帧 × 961)）。FIX-REJ §8 方案 A 记为"每帧每 tile 一次邻域稳健统计"，但任务 ②
明文"为 n=2 档**逐样本**填 prior"⇒ 本实现取"以输出像素为中心"的逐像素邻域（更忠实于 prior 的局部性）。
**粗估**（L4 m42：n=2 像素 ≈98M，2 帧）：每次 31×31 两次 `std::nth_element` ≈ 5-6 µs
⇒ 约 1000-1200 s（单线程）。若 E2E 预算不允许，备选：按 (帧,tile) 粗网格（如 stride 16）
算稳健统计 + 最近/双线性插值（近似，需前台裁决并登记为显式近似），或向量化窗口中位数。
**本实现未擅自引入近似**（禁放宽判据）。

---

## 3 ③ 天光面接生产 mosaic 链（FIX-A P0-08/P0-09）

### 3.1 旧缺陷
`p2_sky_plane_build` 仅被独立工具 `tools/stage2.cpp:479` 调用；`lib/infrastructure/` 内
`sky_plane` 零命中 ⇒ 生产 `mosaic` 无天光面。

### 3.2 新接线约定
| 环节 | 位置 | 约定 |
|---|---|---|
| 构建 | `:4492-4571`（`p2_op_upm_fit`） | 由 control observations 映射 `P2SkySample`（`value=o.value`、`variance=o.control_variance`、`snr=|value|/σ`，与 sampler patch estimator 同源）→ `p2_sky_plane_build` → `p2_sky_plane_save(out_dir/p2_sky_plane.bin)`；config `doc["sky_plane"]`（默认 enabled=true，与 `stage2_common.h:54-60` 默认一致：spline_degree=3 / node_spacing_deg=1.0 / gradient_order=1 / gauge=0 / weight=0 / roughness=1e-3） |
| 失败面 | 同上 | build/save 失败 → stderr 记 `[sky_plane] build FAILED rc=…` + `man.sky_plane_status="fallback_*"`，**保留 UPM C 场**（显式降级，不静默、不写半成品） |
| 扣除 | `:4625-4648`（`p2_op_upm_apply`） | 存在 `p2_sky_plane.bin` 即 `p2_sky_plane_open`；open 失败 = 产物损坏 → DATA fail-closed。逐 tile 预计算 ra/dec（`pix2ang_nest`，nside=2^(coverage.target_order+9)），逐帧 `p2_sky_plane_eval_block` → `corrected = raw − C_k(x) − b_k(x)`（越域点不扣，与 stage2 生产接线同口径） |
| provenance | `:4796-4804` | corrected artifact 增 `sky_plane_applied`/`sky_plane_artifact`；manifest 增 `sky_plane_applied`；upm-fit manifest 增 `sky_plane_status/model_hash/n_nodes/n_frames/n_used` |

**未接线（转交）**：FIX-A P0-10 的**乘法响应 g_k**（`p2_upm_ma_build`，`corrected=(raw−C−b)/g`）仍未进生产；
本项只接天光面（任务 ③ 明文范围）。生产 corrected 当前 = `raw−C−b`（g=1）。

---

## 4 ④ 权重链接线 + 删除静默降级

### 4.1 旧缺陷
`weight_mode=2` 缺 ivar 时 `legacy_allow_weight_fallback=true` 走**等权降级并继续跑**
（`weight_basis="unit_weight_degraded"`, `uncertainty_available=false`）= L3 假绿来源。

### 4.2 新接线约定（weight-chain-report §6）
| 环节 | 位置 | 约定 |
|---|---|---|
| 帧级 SNR 读取 | `:5223-5245` `p2_hips_prop_double`；`:5344-5366` | 逐帧 open signal，读 HiPS properties 文本键 `ASTROCS_FRAME_SNR`（通量型 F_ref/σ_F）与 `ASTROCS_REFERENCE_FLUX`（组内公共 F_ref） |
| 构造输入 | `:5337-5366` | 逐帧 `FrameWeightInput{kind=kFluxTypeUnweightedSnr, has_frame_snr, frame_snr, sparse=nullptr}`；F_ref 逐帧一致性（rtol 1e-9），不一致 → `unclosed_invalid_reference_flux` |
| 权重链 | `:5368-5372` | `compute_inverse_variance_weights(winputs, F_ref)`；`!wres.ok` → **DATA 错误 + closure token**（`weight_closure_token(wres.closure)`），错误串含缺失帧数 `m/N frames missing ivar`、`legacy_allow_weight_fallback=true`（供既有负例断言） |
| 逐样本 ivar | `:5580-5600` | `use_snr_chain` 时 `w = snr_weights[slot[d]]`（= wres.weights[k]，逐帧常量）；非有限/≤0 → DATA 错误 |
| 删除成功降级 | `:5306` | `fallback` 恒 false；**`weight_basis="unit_weight_degraded"` 成功路径已删除**；`legacy_allow_weight_fallback` 仅登记为 provenance（no-op） |
| provenance | `:5489-5497, 5715-5726` | 增 `weight_source`/`snr_chain_closure`/`snr_chain_used`；`weight_basis="frame_snr_ivar"`（SNR 链闭合时），`uncertainty_available=true`（SNR 权重 = 合法逆方差面 → variance/ivar 正常落盘） |

### 4.3 数据面未闭合（**上呈/转交**）
Phase1 写侧当前**不写** HiPS properties 帧级 SNR 键（weight-chain-report §7 已登记键名未冻结；
`lib/infrastructure/aio/**` 为禁改面，且仅有 5 键 provenance 通道）。因此：
- ivar 齐备时行为不变（`per_sample_ivar`，production 主路径）；
- ivar 缺失时本实现**必然 fail-closed**（`unclosed_missing_frame_snr`）——这是"禁静默降级"的正确行为，
  但**SNR 权重链在生产上要真正产出权重，须 Phase1 写入端补键 + 合同冻结键名**（不在本文件面）。

### 4.4 越面必要改动：根 CMakeLists.txt
`astrocs_module_adapters` 原未链权重链。**不链 `astrocs_v6_phase2_integrate`**（其重复编译
upm/rejection/sampler/coverage，与已链的 `astrocs_phase2` 重复符号），而是把
`lib/algorithms/integration/v6/src/weight_chain.cpp`（纯 FP64 + std，仅含自身头）直接编入本 target，
并登记 `lib/algorithms/integration/v6/include` 头面。改动 2 处：
- `CMakeLists.txt:764-770`（add_library 源集）；
- `CMakeLists.txt:773-780`（target_include_directories）。
**这是 ④ 能编译链接的最小必要改动，请前台复核是否接受**（本任务文件面未列根 CMake，但 ④ 明文要求调用权重链）。

---

## 5 ⑤ 第二批进度与转交

### 5.1 已完成
| 项 | 位置 | 内容 |
|---|---|---|
| 方法名表（新方法） | `lib/algorithms/coverage/src/stage2_common.cpp:244-246` | 增 `extreme_value_clip_prior_sigma` → `P2_REJECT_EXTREME_VALUE_PRIOR_SIGMA` |
| profile 白名单 | `stage2_common.cpp:258-263` | 增 `astrocs_adaptive_pixel` |
| underdetermined_n 默认 | `stage2_common.cpp:264-272` | `astrocs_adaptive_pixel` 默认 1，其余默认 2；显式值优先 |
| 方法名表（CLI） | `lib/algorithms/coverage/tools/rejection_cli.cpp:144-147` | 同步新方法名 |
| p3 原子提交 | `module_adapters.cpp:3983-3998`（helper）、`:6594,6642,6980,7106,7205` | 5 个 p3 JSON 产物 + `p3_resampled.bin` 走 `aio_atomic::write_file_atomic_stream`（临时文件→fflush→fsync→原子 rename） |
| `output_mode` 生产消费 | `module_adapters.cpp:6654-6669`（`p3_op_resample`） | 缺失/空 → DATA 拒绝（`FZ-P3-MODES`，禁静默按 surface_brightness）；值域经 `p3_resample_check_mode` |
| `coordinate_frame` 分叉同步 | `module_adapters.cpp:3961, 5970, 7101, 7123` | 4 处 `"icrs"` → `"equatorial"`，与 P0-19 后 properties 的 `hips_frame=equatorial`（IVOA REC-HIPS-1.0 §4.4.1）同源 |

### 5.2 测试面影响（**必须由前台更新；不在本文件面**）
| 测试 | 断言 | 失效原因 |
|---|---|---|
| `tests/unit/p2001_real_nodes_test.cpp` 4e（:796-822）与 (c)（:1041-1072） | `legacy_allow_weight_fallback=true` 成功 + `weight_basis=="unit_weight_degraded"` | ④ 明文删除该成功路径（现 fail-closed） |
| `tests/unit/p2002_unc_rej_prov_test.cpp` `test_f_p2002_01`（:600-660） | 以 group-level `wbpp_current` nominal=3/undet=2 复算生产 bins | ① 改为逐像素 `astrocs_adaptive_pixel`/geom_n（复算须按 `plans[]` 逐 n） |
| `tests/unit/p2002_unc_rej_prov_test.cpp:1461` | `legacy_allow_weight_fallback=true` 链 | 同 ④ |
| `tests/unit/p3002_real_nodes_test.cpp`、`p3002_uncertainty_test.cpp` | p3 链配置无 `output_mode` | ⑤ `output_mode` 缺失即拒绝 |
| `tests/unit/p2001_real_nodes_test.cpp` 加权均值 oracle（:489-504） | `signal == (ivar1·cor1+ivar2·cor2)/(ivar1+ivar2)` | ①+② 使 n=2 像素真正进入排异；oracle 须按 `p2_rejection_sample_mask.bin` 逐样本剔除（科学正确口径） |

**建议前台按上表逐条订正测试**（不是放宽判据，而是让断言与新科学语义一致）。

### 5.3 未做/转交清单
1. **4 个 unknown key**（`algorithm_weight_mode`/`algorithm_upm_gauge`/`algorithm_psf_model`/`sparse_snr_layer`）：
   合同 schema 已声明，但 `lib/infrastructure/cli/parser.cpp:275-316` 的 `kSessionKeys` 白名单缺这 4 键
   ⇒ CLI 报 `unknown key` 退出 3。**修复点在 CLI 面（本任务禁改）**，转交前台/CLI 分片补白名单并明确消费/拒绝语义。
2. **`fits_output/p3_output.cpp` 直调 CFITSIO（§9.1）**：`lib/algorithms/fits_output/**` 不在本文件面，转交。
3. **FIX-A P0-10 乘法响应 g_k** 未接生产（见 §3.2 未接线）。
4. **n=2 先验性能**：逐像素 31×31 稳健统计，需 E2E benchmark（见 §2.2）。
5. **① 逐像素路由的 `reject` 单线程成本**：几何 n 需逐帧读 support 平面（新增 I/O 与内存 depth×512²×4B/tile），
   大帧数 × 大 tile 下需 benchmark。

---

## 6 已裁决事项（原「上呈（需负责人裁决）」；2026-09-20 订正）

> **订正留痕（2026-09-20）**：标题旧文 =「## 6 上呈（需负责人裁决）」；原三项均已由 SD-15/SD-16/SD-17 裁决（`工程控制/RELEASE-02/ACCEPTANCE.md:113-115`），**原三条正文保留为历史留痕**：
> 1. 权重链键名冻结 ⇒ **已裁决**：冻结 `ASTROCS_FRAME_SNR` + `ASTROCS_REFERENCE_FLUX`（SD-15，`ACCEPTANCE.md:113`）；Phase1 写侧补写。
> 2. `coordinate_frame` 取值 ⇒ **已裁决**：取 `equatorial`（SD-16，`ACCEPTANCE.md:114`）。
> 3. n=2 先验窗口口径 ⇒ **已裁决**：**保持逐像素**（SD-17，`ACCEPTANCE.md:115`）。
> ⚠ 另注（不推翻第 3 项的裁决本身）：n=2 排异路径随后被 **SD-18**（`ACCEPTANCE.md:117`）收回为「低 n 走保守（不排异+加权积分）」——逐像素先验计算已从生产路径移除。

1. **权重链键名冻结**：`ASTROCS_FRAME_SNR` / `ASTROCS_REFERENCE_FLUX` 为 weight-chain-report §7 建议名，
   尚无合同冻结、Phase1 写侧未实现。请裁决键名 + 由 Phase1 分片补写 HiPS properties（本实现已按该名读取，缺失即 fail-closed）。
2. **`coordinate_frame` 取值**：`equatorial` vs `icrs`。SCI-AUDIT 定案 IVOA REC-HIPS-1.0 §4.4.1 标准值 = `equatorial`（`icrs` 非法）；
   LEDGER-P1 残留② 曾把 scheduler manifest 写成 `icrs`。本项按 phase3-aio-report §58 的同步要求改为 `equatorial`。
   注意 coverage 跨帧一致性门要求旧产品(`icrs`)与新产品(`equatorial`)不混用（迁移期 fail-closed）。
3. **n=2 先验窗口口径**：任务/报告写"该帧该 tile 的 31×31 邻域"，本实现取"以输出像素为中心"的逐像素邻域。
   若前台意图是每 tile 单一统计量，请裁决；当前实现更忠实于 prior 的局部性。

---

## 7 证据索引

| 证据 | 路径 |
|---|---|
| 语法门（3 TU rc=0） | `run/RELEASE-02/hub-a/logs/syntax_gate.log` |
| 改动面 | `git status --porcelain`：CMakeLists.txt / stage2_common.cpp / rejection_cli.cpp / module_adapters.cpp |
| 权威锚 | DESIGN §4.3:255-261 / §4.4:270 / §4.5:272-319；weight-chain-report §6；REJ-kernel-fix-report §1/§7；FIX-REJ §8；FIX-A-report §1/§3；phase3-aio-report §58 |
