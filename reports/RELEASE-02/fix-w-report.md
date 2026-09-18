# RELEASE-02 权重链 F_ref 约定修复报告（FIX-W）

> 修复者：权重链 F_ref 约定修复 SubAgent（FIX-W）。工作目录：/workspace/Astro CS Database。
> 时点：2026-09-18。**零 git 写**；**未跑 ninja/cmake/ctest**（仅 `g++ -fsyntax-only`）。
> 依据：前台已采纳裁决 **WEIGHT-SCI-001**（reports/RELEASE-02/weight-sci-ruling.md，配对性定理），
> 及 工程控制/RELEASE-02/ACCEPTANCE.md SD-19。
> 产物：本报告；run/RELEASE-02/fix-w/fix-w.patch；run/RELEASE-02/fix-w/logs/*.syntax.log。

---

## 0 摘要

**F_ref 现为「组内公共参考通量」，存头 `ASTROCS_FRAME_SNR` 与其配对。** 逐帧检出通量中位数回退已删除；
Phase1 节点为整个 `input_lights` 数据块选定**一个** F0 并对所有帧使用同一值。Phase2 闸门/权重链
（读侧）**未改语义**，仍 fail-closed（容差 1e-9 相对不放宽、检查不删除），仅错误串补 expected/actual/frame_id。

- 改了什么：6 个文件（4 生产 + 2 测试/构建），见 §1；净 +273/−61。
- 公共 F0 策略：显式 `snr.reference_flux_adu`（首选）→ 块级两遍法中位数（次选）→ 逐帧 fail-closed，见 §2。
- fail-closed：**全部保持**，并新增「显式非法 F_ref ⇒ DATA fail」与「缺 F0 ⇒ 帧级 fail-closed」两处，见 §3。
- 测试：把固化旧「逐帧 F_ref」行为的用例改为显式断言**组内公共**，并新增负例，见 §4。

---

## 1 逐条改动（file:line）

### C1 — 删除逐帧中位数回退，缺 F_ref 即 fail-closed
`lib/algorithms/noise_snr/wrapper_phase1/snr_frame_science.cpp`

| 位置 | 改动 |
|---|---|
| :121-131 | 删除 `used_flux`（声明/`reserve`/`push_back`）——逐帧回退的输入已无消费者（`-Wall -Wextra -Wpedantic -Wconversion` 下零告警）。 |
| :162-173 | 新增 WEIGHT-SCI-001 注释块：配对性定理、逐帧回退为何错（丢 `a_f²`、存头 SNR 混入本帧检出亮度）、禁止恢复。 |
| :174-180 | **新增 fail-closed**：`reference_flux_adu` 缺失/非有限/≤0 ⇒ `out.valid=false`；`out.reason="reference_flux_adu required (group-common F_ref; per-frame median fallback removed)"`；`return out`。 |
| :181-182 | `med_fwhm=median_of(used_fwhm)`；`ref_flux = cfg.reference_flux_adu`（不再有 `?: median_of(used_flux)`）。 |

删除的旧代码（原 :167-170）：
```cpp
const double ref_flux =
    (std::isfinite(cfg.reference_flux_adu) && cfg.reference_flux_adu > 0.0)
        ? cfg.reference_flux_adu
        : median_of(used_flux);          // ← 逐帧检出通量中位数（已删除）
```

### C1b — 头文件注释同步
`lib/algorithms/noise_snr/wrapper_phase1/snr_frame_science.h`
- :51-53 `SnrFrameScienceConfig::reference_flux_adu` 注释由「<=0 -> 有效源通量中位数」改为
  「组内公共参考通量 F_ref；缺失/非有限/<=0 -> fail-closed（回退已删除）」。
- :74-75 `SnrFrameScienceResult::reference_flux_adu` 注释标明「= 定义 `reference_snr_f` 的同一通量」。

### C2 — 节点级组内公共 F0
`lib/infrastructure/scheduler/src/module_adapters.cpp`（`p1_op_noise`）

| 位置 | 改动 |
|---|---|
| :3150-3155 | 新增 `configured_ref_flux` / `has_configured_ref_flux` 解析状态。 |
| :3165-3174 | 只解析显式值；**显式给出但非有限/≤0 ⇒ `ErrorDomain::DATA` fail-closed**（不静默回退）。 |
| :3176-3191 | `find_src_frame(base)` lambda（主循环与两遍法共用，替换原内联查找）。 |
| :3192-3198 | `median_of_vec` lambda（确定性：复制后排序，偶数取中两值平均）。 |
| :3199-3203 | `all_sources_of(src_frame)` lambda。 |
| :3204-3243 | `build_snr_rows` lambda：测光有效判据 `flux>0 ∧ fwhm_px>0` + `snr.max_sources` 截断（与 compute 内部判据一致）。 |
| :3245-3278 | F0 决策：`config`（显式）→ `group_median`（两遍法）→ 保持 0.0（逐帧 fail-closed）；`sci_cfg.reference_flux_adu = group_ref_flux`（所有帧共用）。 |
| :3305 | 主循环 `src_frame = find_src_frame(base)`。 |
| :3326-3333 | 主循环行构造/截断改走 `build_snr_rows`（**与两遍法同一实现** ⇒ F0 与逐帧样本定义严格一致；`n_snr_available`/`truncated` 语义不变）。 |

### C3 — p1_snr.json provenance
`lib/infrastructure/scheduler/src/module_adapters.cpp`

| 位置 | 改动 |
|---|---|
| :3367-3377 | `frame["snr_reference"]` 增写 `"scope":"group"` 与 `"reference_flux_source"`（`"config"|"group_median"|"unavailable"`）。 |
| :3406-3413 | `p1_snr.json` 顶层增写 `snr_reference_scope="group"`、`reference_flux_source`、`reference_flux_adu`（块级 F0）。 |
| :3419-3422 | 节点 manifest 增写同样三项块级 provenance。 |

### C4 — 闸门保持 fail-closed，仅增强诊断串
`lib/infrastructure/scheduler/src/module_adapters.cpp:5468-5484`
- 检查、相对容差 `1e-9`、fail-closed 语义**完全不变**。
- 错误串补 `expected=<ref_flux> actual=<fref> frame_id=<id>`（`%.17g`），仍触发
  `unclosed_invalid_reference_flux`。

### C5 — weight_chain.h 注释澄清（签名/实现不变）
`lib/algorithms/integration/v6/include/astrocs/v6/weight_chain.h`
- :34-42 前置条件补「`reference_flux` 必须**等于定义 frame_snr 时所用的参考通量**（配对性）」，
  并注明「本条为约定澄清，签名与实现不变（缺陷在 Phase1 写侧）」。
- :109-113 `weight_from_snr` 注释补配对性说明；函数签名与实现**未动**。

> 未改：`lib/algorithms/integration/v6/src/weight_chain.cpp`、`lib/infrastructure/aio/**`（sink/writer）、
> `docs/**`、任何科学公式/默认容差。

---

## 2 公共 F0 的取值策略

对**整个 `input_lights` 数据块**只选一个 F0，所有帧传同一值：

1. **首选（config，全局固定 ⇒ 闸门恒过）**：`snr.reference_flux_adu` 显式给出且有限 `>0`
   ⇒ `F0 = 该值`，`reference_flux_source="config"`。
2. **次选（group_median，两遍法）**：键**缺失** ⇒
   - pass-1：遍历 `input_lights`；对每个存在上游 `p1_sources.json` 同帧记录的帧，用**与主循环同一**的
     `build_snr_rows`（`flux>0 ∧ fwhm_px>0`，再施加 `snr.max_sources` 上限）取**检出通量中位数**；
   - 再对这些逐帧中位数取**块级中位数** = `F0`，`reference_flux_source="group_median"`；
   - 只读 `p1_sources.json`（不读图像），排序统计量 ⇒ 与线程数/帧序无关（确定性）。
3. **显式但非法**：非有限/≤0 ⇒ `ErrorDomain::DATA` fail-closed（用户显式值不得被静默忽略）。
4. **都不可得**：`F0` 保持 NaN/0.0 ⇒ 逐帧进入 (C1) fail-closed。

**诚实登记**：次选法下「组内」= 一次 Phase1 run 的 `input_lights`；仅当该 run 的帧集合即 Phase2 组时闸门才过
（裁定 §7.1 C2(b)）。要「闸门恒过」必须在配置层固定 `reference_flux_adu`（裁定 §9.3 建议 (a)）。

**数值等价性说明**：单帧 run（如 parity 测试）下，新 F0 = 该帧检出通量中位数 = 旧逐帧回退值 ⇒ 数值逐位不变；
多帧 run 才发生修正（旧=逐帧、新=块级公共）。

---

## 3 fail-closed 路径（保持 + 新增）

| # | 位置 | 触发 | 行为 |
|---|---|---|---|
| 1 | `snr_frame_science.cpp:174-180`（**新增**） | `cfg.reference_flux_adu` 缺失/非有限/≤0 | `valid=false`，reason 含 `reference_flux_adu required (group-common F_ref)`；`reference_*` 保持 NaN |
| 2 | `module_adapters.cpp:3167-3174`（**新增**） | 显式 `snr.reference_flux_adu` 非有限/≤0 | 节点 `ErrorDomain::DATA` fail（不静默回退两遍法） |
| 3 | `module_adapters.cpp:3270-3278` | 未显式给且块级中位数不可得 | `sci_cfg.reference_flux_adu` 保持 0.0 ⇒ 逐帧 (1) fail-closed；帧 `snr_catalogue_status="degenerate"` |
| 4 | `module_adapters.cpp:5468-5484`（**保持**） | 组内 `ASTROCS_REFERENCE_FLUX` 相对差 > 1e-9 | `unclosed_invalid_reference_flux`（容差/检查不变，仅串补 expected/actual/frame_id） |
| 5 | `astro_sphere_sink.cpp:350-407`（**未改**） | `snr_reference.{snr_f,flux_adu}` 非有限/≤0 | **不写** `ASTROCS_FRAME_SNR/ASTROCS_REFERENCE_FLUX`（禁伪造） |
| 6 | `weight_chain.h:20-27` / `weight_chain.cpp`（**未改**） | 帧级 SNR 或 F_ref 缺失/非有限/非正 | 权重链拒绝，不静默退化为等权 |

⇒ 端到端：**写不出公共 F0 就不会有帧级 SNR 键，Phase2 权重链必然 fail-closed**，不存在伪绿路径。

---

## 4 测试影响（需转交前台重建后跑）

### 4.1 `tests/unit/p1snr/p1snr_linux_test.cpp`（**改了**）
- 旧测试**隐含固化逐帧回退**：F1/F2/F3、oracle、contract、determinism 全部不设 `reference_flux_adu`，
  依赖回退才 `valid`。现改为显式**组内公共** `kGroupRefFlux`（:143 常量；:175/:234/:261/:386）。
  - 被断言量与 ref flux 无关（gain≤0 时 `σ_F` 与通量无关；`snr_f/median_snr/local_snr` 不含 F_ref；
    oracle 锚 `kOracleFlux5` 仍成立）⇒ 既有数值断言不变。
  - F1 增 :202-203：`reference_flux_adu == kGroupRefFlux`。
- **新增组 `common_ref`（:405-476）**，判据未放宽：
  - G1/G2：两"帧"（不同检出星群）用同一 F0 ⇒ `reference_flux_adu` **逐位相同**；
  - G3：`reference_snr_f` 在公共 F0 处评价（`F0/σ_F(F0)`，独立 long-double 参考实现，rtol 1e-12）；
  - G4：缺 F0 ⇒ `!valid` 且 reason 含 `reference_flux_adu required` + `group-common F_ref`，`reference_flux_adu` 保持 NaN；
  - G5：F0 = 0 / NaN / 负 ⇒ `!valid`。
- 注册：`tests/unit/p1snr/CMakeLists.txt:29,32` 新增 `p1snr_linux_common_ref`。

### 4.2 `tests/unit/p1snr/p1snr_frame_parity_test.cpp`（**改了**）
- `run_noise` 未设 `reference_flux_adu` ⇒ 单帧 run 下 F0 = 旧逐帧回退值 ⇒ A（`max_stars` parity）、
  B（真实性）、C（`max_sources` 非恒真）**数值逐位不变**（可继续作 pristine↔patched 逐字节对照）。
- 新增 :244-250：`snr_reference.scope=="group"`、`reference_flux_source=="group_median"`。
- 新增 :279-325（D 段）：
  - D1 显式 `reference_flux_adu=777.5` ⇒ `source=="config"` 且存头 `flux_adu==777.5`、顶层 provenance 同值；
  - D2 显式 `reference_flux_adu=-1` ⇒ 节点 **DATA fail-closed**。

### 4.3 无需改动
- `tests/unit/p1001_real_nodes_test.cpp`（noise-snr 段只查 NoiseModel `valid/variance/ivar`，且 worker-parity
  只比较 `p1_snr.json` 跨 worker 逐位一致 —— 两遍法确定性满足）。
- `tests/unit/p2001_real_nodes_test.cpp`（`test_p1_frame_snr_keys` 直接伪造 `p1_snr.json` 测 **sink**，
  sink 未改；闸门负例未变）。
- 未改任何 ctest 名称或判据，只新增 1 个测试名。

---

## 5 未做 / 需转交前台

1. **构建 + ctest**：本任务遵令**未跑** ninja/cmake/ctest；需前台统一重建并跑
   `p1snr_linux_{units,oracle,contract,negative,determinism,common_ref}`、`p1snr_frame_parity`、
   `p1001_*`、`p2001_*`。
2. **文档变更 claim**（裁定 §7.2 CLAIM-DOC-WEIGHT-SCI-001..004）：本任务**未改 `docs/**`**，按 §3 单独走。
   建议同步 `lib/algorithms/noise_snr/wrapper_phase1/README.md` 的 F_ref 说明（模块 README，非 docs/，本次未动）。
3. **配置层固定 F0**：建议 RELEASE-02 正式配置显式给出 `snr.reference_flux_adu`（治理决定，裁定 §9.3）。
4. **未闭合科学项**（不在本任务范围）：UPM 乘性 `g_k` 未接线；SNR 未含光度响应 `a_k`（裁定 §9.1/9.2）。

---

## 6 验证证据（本地，未入库）

| 证据 | 结果 |
|---|---|
| `run/RELEASE-02/fix-w/logs/module_adapters.syntax.log` | `g++ -fsyntax-only`（精确 ninja INCLUDES/FLAGS）**exit 0，0 行输出** |
| `run/RELEASE-02/fix-w/logs/snr_frame_science.syntax.log` | `-Wall -Wextra -Wpedantic -Wconversion` **exit 0，0 行输出** |
| `run/RELEASE-02/fix-w/logs/p1snr_linux_test.syntax.log` | exit 0，0 行输出 |
| `run/RELEASE-02/fix-w/logs/p1snr_frame_parity_test.syntax.log` | exit 0，0 行输出 |
| `run/RELEASE-02/fix-w/fix-w.patch` | 613 行 diff（4 生产 + 2 测试/构建文件） |

```bash
export TMPDIR=/dev/shm/astrocs_fixw
# 语法检查（示例：module_adapters）
CMD=$(sed -n '2097p' build/build.ninja | sed 's/^  INCLUDES = //')
eval "g++ -fsyntax-only -std=gnu++17 -fopenmp -O3 -DNDEBUG -DAIO_ENABLE_HEALPIX=1 -DASTROCS_PROBES=1 $CMD lib/infrastructure/scheduler/src/module_adapters.cpp"
```

---

## 7 改动文件清单

| 文件 | 类型 |
|---|---|
| `lib/algorithms/noise_snr/wrapper_phase1/snr_frame_science.cpp` | 生产（C1） |
| `lib/algorithms/noise_snr/wrapper_phase1/snr_frame_science.h` | 生产（C1b 注释） |
| `lib/infrastructure/scheduler/src/module_adapters.cpp` | 生产（C2/C3/C4） |
| `lib/algorithms/integration/v6/include/astrocs/v6/weight_chain.h` | 生产（C5 注释） |
| `tests/unit/p1snr/p1snr_linux_test.cpp` | 测试（组内公共 + 负例） |
| `tests/unit/p1snr/CMakeLists.txt` | 测试注册 |
| `tests/unit/p1snr/p1snr_frame_parity_test.cpp` | 测试（provenance + fail-closed） |
