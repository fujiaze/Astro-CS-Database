# RELEASE-02 HUB-B 报告 — 测试面更新 / Phase1 帧级 SNR 写侧 / CLI 补键 / CMake 复核

> 执行者：HUB-B 实现者。工作目录：/workspace/Astro CS Database。
> 时点：2026-09-18。零 git 写；**未跑 ninja/cmake/ctest**（硬要求），验证 = `g++ -fsyntax-only`。
> 主文件面：`tests/unit/p2001_real_nodes_test.cpp`、`tests/unit/p2002_unc_rej_prov_test.cpp`、
> `tests/unit/p3002_{real_nodes,uncertainty}_test.cpp`、`lib/infrastructure/aio/{include/aio_hips.h,src/hips/aio_hips_writer.cpp}`、
> `lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.{h,cpp}`、`lib/infrastructure/cli/parser.cpp`。
> **未改** `docs/**`、`lib/algorithms/coverage/{include,src}/rejection.*`、`lib/infrastructure/scheduler/src/module_adapters.cpp`。

---

## 0 结论摘要

| 项 | 内容 | 状态 |
|---|---|---|
| ① | 测试面按 HUB-A 新行为订正（4e/(c)/§30.3 fallback、加权均值 oracle 逐样本掩码、p2002 逐 n plan 复算、p3002 output_mode、p2002 reject_profile） | **已闭合**（不放宽判据） |
| ② | Phase1 写侧补写 `ASTROCS_FRAME_SNR`/`ASTROCS_REFERENCE_FLUX`（AIO 通道 + drizzle 末端 + 新测试） | **已闭合（代码 + 测试）** |
| ③ | CLI `kSessionKeys` 补 `algorithm_weight_mode`/`algorithm_upm_gauge`/`algorithm_psf_model`/`sparse_snr_layer` | **已闭合** |
| ④ | 根 `CMakeLists.txt:764-780` 处置复核 | **结论：妥当**（附更干净备选，见 §4） |
| ⑤ | 语法门（7 TU rc=0） | `run/RELEASE-02/hub-b/logs/syntax_gate.log` |

**科学语义零放宽**：新断言表达的都是**正确行为**（fail-closed / 逐样本剔除 / 逐 n 路由），不是把判据调松。

---

## 1 ① 测试面更新（逐条：改了什么 + 为何新期望正确）

### 1.1 p2001 4e（`test_negative_and_fallback`）—— 旧期望是被删除的假绿路径
- **旧**：`legacy_allow_weight_fallback=true` + 缺 ivar ⇒ 链成功，`p2_integrated.json` 记
  `fallback=true`/`weight_basis="unit_weight_degraded"`/`uncertainty_available=false`。
- **新**：断言 **fail-closed** —— `ff2.failed()`、无 `p2_integrated.json`、无 `p2_final.json`，
  且错误串含 `NOT closed` 与 `legacy_allow_weight_fallback=true`。
- **为何正确**：HUB-A 已删除模块内 `legacy_allow_weight_fallback` 的**成功等权降级**路径
  （weight-chain-report §5/§6.3：该键只登记 provenance）。缺 ivar 且无 HiPS 帧级 SNR 键
  （`ASTROCS_FRAME_SNR`/`ASTROCS_REFERENCE_FLUX`）时，`compute_inverse_variance_weights` 返回
  `unclosed_missing_frame_snr` ⇒ 生产必须失败且不得留伪产物（ASTROCS_DESIGN §9：失败不得留下
  可被误认为正式产品的半成品）。旧断言正是 L3 假绿来源。

### 1.2 p2001 (c)（`test_ivar001_weight_mode_domain_and_audit`）—— 同 4e
- **旧**：同键成功 + `weight_basis="unit_weight_degraded"` + `products.size()==2`。
- **新**：同键 fail-closed（无伪产物），并断言诊断如实报 `ivar` 缺失、`2/2` 帧缺失、
  权重链 `NOT closed`、该键不再是降级出口。
- **为何正确**：与 4e 同一权威（§30.1 规则 2 + weight-chain-report §6.3）。诊断串同时验证
  「诚实登记 ivar 缺失帧数」这一保留行为。

### 1.3 p2001 加权均值 oracle —— 按逐样本掩码剔除（①+② 的科学后果）
- **旧**：`wsum` 覆盖像素恒 `==2.5`；`signal == (2.0·s1 + 0.5·s2)/2.5`（抽查 1000 像素）。
- **新**：读 `p2_rejection_sample_mask.bin` + `tiles[].sample_mask_offset/depth/frame_slots`，
  逐像素**全量**核对：`wsum == Σ_{accepted} ivar_i`、`signal == Σ w_i s_i / Σ w_i`；
  无有效样本（无覆盖或全拒）⇒ `signal/wsum` 必须为 NaN（禁 0 伪装）。掩码全接受时
  自然退化为原 2.5。
- **为何正确**：HUB-A ①（逐输出像素几何 n 路由）+ ②（n=2 外部先验）使 n=2 像素**真正进入
  kernel 排异**；integrate 按掩码逐样本剔除（DATA-UNC-001 §30.2 完备划分）。旧 oracle 固化
  的是「n=2 从不排异」的旧行为，会让「W 含被拒样本 ivar」这类缺陷漏网。

### 1.4 p2002 `test_f_p2002_01_rejection_parity` —— 逐 n 复算（不再 group-level）
- **旧**：以 `wbpp_current` + `nominal_contributors=3` **一次解析**全图 plan 重放。
- **新**：按 provenance `plans[]` 建 `nominal_n→plan` 表；逐像素由各帧 **support>0** 计几何 n，
  调 `p2_reject_plan_resolve_n(n, profile="astrocs_adaptive_pixel", underdetermined_n=1,
  nominal_contributors=0)` 复算 plan，并**逐字段对拍** provenance 的 `method/minimum_n/
  underdetermined_n/semantic_id`；n=2 档按生产同口径算 31×31 邻域中位数/MAD 先验（`center_mode=0`）。
- **为何正确**：生产已改为 `P2_PROFILE_ASTROCS_ADAPTIVE_PIXEL` 逐像素路由（module_adapters
  `:4895-4918`）。group-level 复算与生产不再同参；逐 n 复算 + provenance 对拍才真正锚定生产语义。
  （本 fixture 只有 geom_n∈{0,3}，n=2 先验分支保留为通用能力。）

### 1.5 p2002 §30.3 unavailable 面（原 `:1461`）—— 拆为「fail-closed + 合法出口」
- **旧**：`legacy_allow_weight_fallback=true` 成功降级后验证 unavailable 面五键。
- **新**：**4a** 断言该键 **fail-closed**（无伪产物；诊断含 `NOT closed` 与键名）；
  **4b** 用**唯一合法 unavailable 出口** `weight_mode=1`（§30.1 规则 1：等权非方差面）
  重建 §30.3 五键 + `nused` 投影 + 磁盘一致性覆盖。
- **为何正确**：成功降级路径已删除（同 1.1）；但 `weight_mode=1` 本就是合同定义的
  `uncertainty_available=false` 面，故 §30.3 覆盖不丢失、判据不放松。

### 1.6 p2002 reject profile 期望（`:1360`）—— 默认 profile 已变
- **旧**：`prov["ASTROCS_REJECT_PROFILE"] == "wbpp_current"`。
- **新**：`== "astrocs_adaptive_pixel"`（仍先对拍 `== rej["profile"]`）。
- **为何正确**：`p2_op_reject` 缺省 profile 现为 `astrocs_adaptive_pixel`（module_adapters
  `:4895-4896`）。旧硬编码值编码的是被替换的 group-level 行为。

### 1.7 p3002 配置补 `output_mode`
- `p3002_real_nodes_test.cpp` `node_config`、`p3002_uncertainty_test.cpp`
  `run_session` 与 node-parity 两处配置补 `"output_mode":"surface_brightness"`。
- **为何正确**：`p3_op_resample` 现对缺失 `output_mode` **直接拒绝**（FZ-P3-MODES，禁静默按
  `surface_brightness`）；`surface_brightness` 是 `p3_resample_check_mode` 唯一接受的
  生产模式。node-parity 配置走 `astrocs.phase3.resample2` 节点，缺键必红，故必须补。

### 1.8 新增 p2001 `test_p1_frame_snr_keys`（② 的写侧 Oracle）
- 用真实 Phase1 `drizzle`+`writer` 节点链 + 上游 `p1_snr.json` sidecar：
  - **正例**：`snr_f=42.5,flux_adu=1000` ⇒ `signal/properties` 含
    `ASTROCS_FRAME_SNR=42.5` 与 `ASTROCS_REFERENCE_FLUX=1000`（%.17g round-trip）；
  - **负例**：`snr_f=0,flux_adu=0` ⇒ 两键**整体不写**（禁伪造；Phase2 权重链 fail-closed）。

---

## 2 ② Phase1 写侧帧级 SNR 键

### 2.1 键名与语义（冻结，照前台裁决）
| 键 | 值 | 定义 |
|---|---|---|
| `ASTROCS_FRAME_SNR` | `SNR_k = F_ref/σ_F` | **帧级未加权通量型信噪比**（不是权重；信号经独立局部背景扣除、不被加性天光虚高；天光散粒噪声计入 σ_n） |
| `ASTROCS_REFERENCE_FLUX` | `F_ref` | 组内公共参考通量（Phase2 `w = SNR²/F_ref² = 1/σ_F²` 的公共标度） |

### 2.2 实现（三处，均在允许文件面内）
1. **AIO 通道** `lib/infrastructure/aio`：
   - `aio_hips_set_frame_snr(ps, frame_snr, reference_flux)`：全或无 + 禁伪造
     （非有限/≤0 立即返回非 0，不置标志）；
   - `finalize_image_product` 在 `frame_snr_set` 时把两键写入每个 image 子产品 properties
     （与既有 §30.3 五键同面；`%.17g` 保证 double round-trip，满足 Phase2 逐帧 F_ref
     一致性门 rtol 1e-9）。
2. **Phase1 末端** `astro_sphere_sink.cpp::write_hips_phase1`：在 `finalize` 前读
   **`<output_dir>/p1_snr.json`**（本帧目录的父目录），按 `frame_key` 匹配本帧
   （兼容 `cleaned_`/`calibrated_` 前缀），取 `snr_reference.{snr_f,flux_adu}` 调 setter。
   缺失/不匹配/非正 ⇒ **不写键**（Phase2 权重链 fail-closed，禁伪造）。
3. **测试**：见 §1.8。

### 2.3 为何这样接（约束下的唯一干净路径）
- 帧级 SNR 由 `p1_op_noise` 算出并落 `<output_dir>/p1_snr.json`（`snr_reference` 字段，
  `snr_f = reference_snr_f = F_ref/σ_F`，与 §4.1 定义一致）；drizzle 节点随后逐帧直写 HiPS。
- **`module_adapters.cpp` 是禁改面**，无法把值从 `p1_op_drizzle` 直接传进 sink，故 sink 读
  同目录约定产物 `p1_snr.json`。这是确定性、可核对的约定（frame_key 与产品目录同名）。
- **性能提示（未闭合，转 benchmark）**：sink 逐帧解析 `p1_snr.json`（O(N × 文件大小)）。
  数据量远小于 drizzle 本体，但大批帧时应定档；更彻底的做法是把值经 `hp_drizzle_run_phase1_hips`
  参数直接下发（需改 module_adapters）。**本实现未改科学语义**。

---

## 3 ③ CLI 补 4 键

`lib/infrastructure/cli/parser.cpp` 的 `kSessionKeys`（session 平铺白名单）补：
`algorithm_weight_mode`、`algorithm_upm_gauge`（`phase_config_mosaic.schema.json` 声明）、
`algorithm_psf_model`、`sparse_snr_layer`（`phase_config_normalize.schema.json` 声明）。
- 透传：`runtime_client.cpp::phase_config` 对 phase1/2/3 平铺形态是 `pdoc = doc` **零改动直通**，
  故补白名单即完成「识别 + 透传」；科学消费仍在 scheduler 面。
- 未改 `session_commands.h` 的 help/模板（避免动 CLI golden 输出；白名单缺键才是退出 3 的实际缺陷）。

---

## 4 ④ 根 CMakeLists `:764-780` 复核结论

**处置妥当**（可直接保留）。
- `weight_chain.cpp` **自包含**：仅 `#include "astrocs/v6/weight_chain.h"` + `<algorithm>/<cmath>/<limits>`，
  不依赖 `astrocs_v6_phase2_integrate` 的其余源。
- 若改链 `astrocs_v6_phase2_integrate`：该库重编译 `upm.cpp/rejection.cpp/sampler.cpp/coverage.cpp`
  （`astrocs_phase2` 已含）+ `aio_hips_reader.cpp/aio_upm.cpp`（`astrocs_hips` 已含）+
  `healpix_core.cpp/sha256.cpp`（`astrocs_common` 已含）+ cfitsio 源；而
  `astrocs_module_adapters` 已链 `astrocs_phase2/astrocs_drizzle/astrocs_hips/astrocs_aio`，
  链接期会重复定义。HUB-A 的规避有据。
- 当前图内 `astrocs_v6_phase2_integrate` 仅被 `v6_p2_integrate_test` 链接（不链 module_adapters），
  故 `weight_chain.cpp` 双编译**不产生冲突**；`astrocs` 主可执行不含该库。
- **更干净的备选（未实施，供前台定夺）**：把 `weight_chain.cpp` 抽成独立小 target
  `astrocs_v6_weight_chain`（+ 其 include 目录），由 `astrocs_module_adapters`、
  `astrocs_v6_phase2_integrate`、oracle 共同链接。收益 = 消除 3 处重复编译 + 明确 target 边界；
  代价 = 动 `lib/algorithms/integration/v6/CMakeLists.txt` 与根 CMake，改动面更大。
  在「最小改动面」约束下，HUB-A 的现状可接受。

---

## 5 科学语义护栏（未放宽清单）

- 未改任何阈值/判据/冻结枚举；`rejection.*`、`docs/**`、`module_adapters.cpp` 零改动。
- `n=2` 先验保持**逐输出像素**（SD-17；本任务未引入任何 tile 级近似）。
- `coordinate_frame=equatorial`：HUB-A 已改 4 处（`:3961/5970/7101/7123`），本次复核未发现遗漏
  （本任务面内无新引入 `icrs` 的 properties/frame 写入点）。
- 权重链键名只写不读侧改动；SNR 仍是**信噪比不是权重**。

---

## 6 证据索引

| 证据 | 路径 |
|---|---|
| 语法门（7 TU rc=0） | `run/RELEASE-02/hub-b/logs/syntax_gate.log` |
| 改动面 | `git status --porcelain`：aio_hips.h / aio_hips_writer.cpp / astro_sphere_sink.{h,cpp} / parser.cpp / p2001 / p2002 / p3002×2 |
| 权威锚 | DESIGN §3.4:151-175 / §4.3:255-261 / §4.5；07_noise_snr.md §4.1:36-70；weight-chain-report §5-§6；工程控制/RELEASE-02/ACCEPTANCE.md SD-15..SD-17 |

---

## 7 未闭合项（上呈）

1. **写侧 sidecar 耦合**：sink 读 `<output_dir>/p1_snr.json` 是禁改 `module_adapters.cpp` 下的
   折中；更干净是 `p1_op_drizzle` 把值经 `hp_drizzle_run_phase1_hips` 参数下发。若前台愿意
   开 module_adapters 口，建议改参数下发。
2. **写侧性能**：逐帧解析 `p1_snr.json`（O(N×文件大小)）待 benchmark 定档；可加进程内解析缓存
   或改参数下发消除。
3. **p2002 §30.3 覆盖口径**：以 `weight_mode=1` 作为唯一合法 unavailable 出口重建；请前台确认
   此解释（若认为应彻底删除该测试，请指示）。
4. **导出面探针列表**：`tests/unit/p1_hips/adapter_test.c` 的 legacy 符号清单（9 项）未含
   `aio_hips_set_frame_snr`；version-script 白名单已自动隐藏（不影响断言），是否补列由前台定。
5. **E2E 未跑**：受硬要求限制未构建/未跑真实 normalize 链；本报告的 Phase1 写侧证据为语法门 +
   新增节点级测试。真实 `ASTROCS_FRAME_SNR` 落盘须由前台构建后跑 `p2001_real_nodes` 与一次
   Phase1→Phase2 权重链 E2E 复核。
6. **文档口径冲突**：`CONTROL_WEIGHT_SNR.md` §2a/§7 vs `07_noise_snr.md` §4.1（帧级 SNR 定义）
   仍为 UNRESOLVED（weight-chain-report §7）；不在本任务文件面。
