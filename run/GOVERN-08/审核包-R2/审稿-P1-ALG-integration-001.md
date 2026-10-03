# 审稿-P1 · ALG-integration-001（第 1 遍 · 对抗审稿）

- 仓库：`/workspace/Astro CS Database`，HEAD = `850a9edefd47434b9ab71bc907c3de1e0814b323`
- 片清单来源：`run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml:333-362`（层 `lib/algorithms/integration`）
- 日期口径：2026-10-02
- 纪律遵守：未编译、未跑 ctest/pytest、未跑任何仓内二进制、未做任何 git 写、未改任何仓内文件、未读 `/tmp/acsd_g08/`。全部结论来自逐行阅读原文 + 只读 grep/sed 取证。

---

## 1. 读完了吗

**口径**：覆盖率 = （亲自完整读完的成员文件行数）/（权威清单声明的该片实际行数）。以 `wc -l` 实测为准，与清单 `实际行数: 8158` 逐份吻合（本人独立 `wc -l` 求和 = 8158）。

| 项 | 数 |
|---|---|
| 成员份数（权威清单） | 22 |
| 实际读了多少份 | **22** |
| 成员总行数 | 8158 |
| 实际读了多少行 | **8158** |
| 覆盖率 | **100%**（22/22 份，8158/8158 行） |

**未读完的：无。** 22 个成员文件全部逐行读完，无跳读、无抽样。

逐份行数（`wc -l` 实测）：

| 文件 | 行数 | 已读 |
|---|---:|---|
| `phase2_integrate/src/phase2_integrate.cpp` | 1576 | 全 |
| `phase1_product/src/phase1_product.cpp` | 1327 | 全 |
| `phase2_integrate/oracle/weight_chain_selfcheck.cpp` | 1000 | 全 |
| `phase2_integrate/src/weight_chain.cpp` | 984 | 全 |
| `phase2_integrate/oracle/weight_chain_oracle.py` | 569 | 全 |
| `phase2_integrate/include/acsd/weight_chain.h` | 466 | 全 |
| `phase2_integrate/oracle/recon_exp04_parity.py` | 398 | 全 |
| `phase2_integrate/include/acsd/phase2_integrate.h` | 247 | 全 |
| `phase1_product/include/acsd/phase1_product.h` | 244 | 全 |
| `phase2_integrate/src/variance_propagation.cpp` | 222 | 全 |
| `phase2_integrate/oracle/recon_negative_injections.py` | 183 | 全 |
| `phase2_integrate/oracle/recon_dump.cpp` | 134 | 全 |
| `phase2_integrate/CMakeLists.txt` | 125 | 全 |
| `phase2_integrate/oracle/recon_contract_gate.py` | 106 | 全 |
| `integration/module.yaml` | 91 | 全 |
| `phase2_integrate/include/acsd/variance_propagation.h` | 86 | 全 |
| `integration/README.md` | 84 | 全 |
| `integration/memory.md` | 81 | 全 |
| `phase1_product/README.md` | 73 | 全 |
| `phase2_integrate/README.md` | 63 | 全 |
| `phase1_product/CMakeLists.txt` | 58 | 全 |
| `phase2_integrate/oracle/CMakeLists.txt` | 41 | 全 |
| **合计** | **8158** | **8158** |

---

## 2. 本片判定

### **阻断**

本片同时存在**多处「用同一个定义式既当被检量又当期望量」的自洽式断言**、**结构上不可变红的恒真门**，以及**会就地改写生产源码且无 `try/finally` 的仓内脚本**。按负责人本轮口径，这些不是「阈值偏松」，而是判据本身不存在。

**另有两条阻断项由第 5 子代理报告、经理复核后采纳**（详见 §8A）：**B8** `phase1_product.cpp:1005` 返回的产品哈希恒为空串（`atomic_publish_directory` 从不赋值，Phase2 已在 `:820-824` 自行绕开，Phase1 未绕）；**B9** 一级工程正本 `docs/engineering/VALIDATION_EVIDENCE_STANDARD.md` **仍把退役对象 `psfsw_robust` 登记为 `production`** —— 本片全部退役纪律的权威出处尚未撤销（跨片，提请文档片与负责人裁决）。

### 最重 3 条

**B1 —— `dimensional_identity`：教科书式自洽式断言，且被正式立为判据**
`weight_chain.cpp:219`
```
r.dimensional_identity = w * in.ref_flux_k * in.ref_flux_k - layer_snr * layer_snr * g * g;
```
而 `w` 恰由同一函数算出：`:197` `weight_from_snr(layer_snr, in.ref_flux_k, &w, …)`（`:104` `w=(snr/F_ref)²`）+ `:200` `w *= g*g`。
⇒ `w·F_ref² − SNR²·g² = ((SNR/F_ref)²·g²)·F_ref² − SNR²·g² ≡ 0`（**精确恒等**，非近似）。
这条量还被三处当作判据直接断言：`oracle/weight_chain_selfcheck.cpp:302-303`、`:819-820`、`eng/tests/integration/p2_integrate/p2_pixel_weight_test.cpp:123`；并且 `weight_chain.h:426-435` 明文把它写成对外契约「**可被调用方与门直接断言**」「后式应恒为 0（相对残差 ≤ 1e-15）」。即项目主动邀请下游把它当质量判据，而它对任何实现缺陷零鉴别力。

**B2 —— `snr_rel`：恒真门，且把恒 0 写进生产产品记录**
`phase2_integrate.cpp:952-956`
```
double sum_w = 0.0;
for (const auto& f : fs.frames) sum_w += f.view.w_info;      // :954
snr_rel = (w > 0.0) ? std::fabs(w - sum_w) / w : 0.0;          // :955
if (!joint_used && snr_rel > kSnrIdentRtol) { …判红… }         // :956
```
`w` 在 `:871` 绑定 `w_sum`，`w_sum` 由 `:868` 用**同一容器 `fs.frames`、同一 range-for 序、同一初值 0.0、同一 FP64 加法**算出。IEEE-754 下两次求和**逐位相同** ⇒ `snr_rel ≡ +0.0`。唯一改写 `w` 的点是 `:914 w=wj`，位于 joint 分支内且必置 `joint_used=true`，被 `:956` 的 `!joint_used` 短路掉 ⇒ **门的唯一求值路径恰好就是恒等路径，无出路口**。
更重：`clause_registry.json:1868` 该条款 subject 是「独立帧 SNR_combined²=ΣSNR_k² 相对容差」——实现**全程没碰 SNR、没做过平方**，条款自己写的 negative_mutation（忘平方）对本门零鉴别力。
污染面：`:998` → `:745`，`phase2_extensions.snr_identity_rel_err` 在每个 Phase2 产品里**永远是精确 0.0**，任何下游/审计读到都会得出「恒等式已验证通过」。

**B3 —— `FZ-DEGRADE-SCALAR` 三重退化，且 evidence 虚假背书**
`phase2_integrate.cpp:1481-1488` + `:1494` + `:1569`
```
th.thresholds_declared = 1;          // :1481
th.residual_trend_max = 0.10;        // :1482  注释称「已冻结值」
th.power_loss_max     = 0.05;        // :1483  注释称「已冻结值」
…
si.spatial_residual_p95 = 0.0;       // :1486  ← 三个门判据量全部写死
si.spatial_trend        = 0.0;       // :1487
si.power_loss           = 0.0;       // :1488
```
1. **输入写死**：我已亲自读 `sampler.h:235-243` 与 `sampler.cpp:1479-1489`——真正参与阈值比较的**只有**这三个量；`:1489-1491` 从真实 `sum` 取的 p05/p50/p95/max_dev/coverage/model_error 在门里只做合法性与单调性校验 ⇒ 两个阈值臂（0.10 / 0.05）恒绿。
2. **阈值冒签**：`sampler.h:235-237` 原文是「数值属 **SO-07 PENDING_OWNER_SIGNOFF**…**不得自行定值、不得放宽**」，而此处注释写「已冻结值」且置 `thresholds_declared=1`，把待签署值声明为已签署。
3. **返回码不裁决**：`:1494` 的 rc 只塞进 `r.scalar_verdict`，从不判红；局部 `verdict`（`:1493`，真正的科学结论）被丢弃；`:1570` 仍无条件 `r.ok = true`。
4. **虚假背书**：`:1569` 无条件 push `"FZ-DEGRADE-SCALAR: spatial gate + power-loss gate + p05/p50/p95"` —— **在从未可能失败的事上签发合格证**。这直接违反本文件 `:1520-1523` 自订的「每个返回码都必须被裁决——恒真门没有证据资格」。

> 同文件还有第二例同型：`:755-759` 产品记录的 `gates` 块里 `{"FZ-MODE-PRODUCTION", true}`、`{"FZ-FORMULA-COV-PROP", true}` 是字面量，且 `as.pixel_ivar_approx`（`:387` 声明 `false`）经全 TU grep 确认**从未被赋 true** ⇒ `:758` 恒真，`cov["approximation"]`（`:631-636`）整块永不生成，而合同 `FZ-GATE-PIXIVAR-APPROX` 要求「未报告 Var_approx/Var_GLS → REJECT」。

---

## 3. 逐文件清单

| # | 文件 | 读了什么 | 看到什么 | 判定 |
|---|---|---|---|---|
| 1 | `phase2_integrate/src/phase2_integrate.cpp` (1576) | 全 1576 行 | 接线层 + 产品装配 + 磁盘重开 + UPM/REJ/SAMP。**问题高度集中于 `:1256-1572` `run_upm_rej_samp_wiring`**：合成数据当科学判据、丢弃库的判决位、吞门失败 | **需修（阻断项最多）** |
| 2 | `phase1_product/src/phase1_product.cpp` (1327) | 全 1327 行 | 产品装配/落盘/重开。退役对象处理**做得好**（`:89-96` 判据 + `:1131-1139` 显式登记 + `:1307-1323` 消费面无条件 fail-closed）。缺陷：硬编码 provenance 输入哈希、硬编码时间戳、伪 provenance 声明 | 需修 |
| 3 | `oracle/weight_chain_selfcheck.cpp` (1000) | 全 1000 行 | oracle 独立性**大部分成立**（`:77-114` 刻意用稠密高斯消元替代被测 Thomas）。但「M06 判据②」是伪注入 | **需修（阻断 B1/B4 载体）** |
| 4 | `src/weight_chain.cpp` (984) | 全 984 行 | 权重链 fail-closed **质量最高**：15 个 `WeightClosure` 码逐条对应真实失败、分母全 `positive_finite` 拒绝、NaN 有 schema 语义 + 填充 + 全无效 fail-closed、域外拒绝不外推、`legacy_allow_weight_fallback` 置真也只 fail-closed | 须修（仅 `:219` 自洽量） |
| 5 | `oracle/weight_chain_oracle.py` (569) | 全 569 行 | 不 import C++（真独立）。但 `:369-373`/`:377-383` 拿 `oracle_reconstruct` 与 `oracle_spline_clip` 比，而前者内部**正调用**后者 ⇒ 同义反复 | 建议 |
| 6 | `include/acsd/weight_chain.h` (466) | 全 466 行 | 权威锚写得详实。**问题在 `:426-435` 把自洽量写成「可被门直接断言」的对外契约** | 须修 |
| 7 | `oracle/recon_exp04_parity.py` (398) | 全 398 行 | **本片判据纪律最好的一份**：G10 未传 `--oracle-json` 时 `:373-374` **显式判 FAIL 不静默通过**；G9 crop 不匹配时 `:336-337` 显式 SKIP。G1-G3 与实验单元 EXP-04 逐像素对拍（真独立） | 建议（仅 G9 硬编码报告值） |
| 8 | `include/acsd/phase2_integrate.h` (247) | 全 247 行 | 冻结常数齐全。但 `ProductResult` 声明 9 个**从未被赋值**的科学字段（`var_gls`/`x_hat`/`var_approx`/`rho*`/`median_wt`/`pixel_ivar_approx_used`/`approx_gate_passed`/`weight_value_null`） | 建议 |
| 9 | `phase1_product/include/acsd/phase1_product.h` (244) | 全 244 行 | 退役对象文档处理**正确**。`Phase1GroupConsumption`（`:226-235`）仍声明 5 个死字段（消费面只设 `error`） | 建议 |
| 10 | `src/variance_propagation.cpp` (222) | 全 222 行 | **干净**。`residual_maker_variance`（`:59-91`）与 `naive_variance`（`:93-122`）是两条**实质不同**的公式；`mean_model_naive_over_correct`（`:212-218`）是闭式、不引用任何实现 | **通过** |
| 11 | `oracle/recon_negative_injections.py` (183) | 全 184 行 | 注入方法学扎实（sha→打补丁→断言门红→还原→复核 sha 逐位一致）。**但就地改写生产源码且无 `try/finally`** | **阻断（B7）** |
| 12 | `oracle/recon_dump.cpp` (134) | 全 134 行 | 纯 harness，错误码诚实（`PREPARE_ERR` + rc=1，`ERR` 行）。无问题 | 通过 |
| 13 | `phase2_integrate/CMakeLists.txt` (125) | 全 125 行 | 自包含可独立构建。**无 `add_subdirectory(oracle)`** ⇒ oracle 不在构建图 | 须修（配套 B7） |
| 14 | `oracle/recon_contract_gate.py` (106) | 全 106 行 | schema 合同门，5 条正例 + 负例齐备，无同义反复 | 通过 |
| 15 | `integration/module.yaml` (91) | 全 91 行 | **描述的是另一个模块**：`:77-78` `source_symbols` 是 `p2_integrate_pixel`/`p2_validate_candidate_weights`（本目录**零命中**，实际在 `lib/algorithms/coverage/`）；`:19-20` 自称本目录「仅合同文件、无源码」而实际有 8158 行生产源码 | **须修（B8）** |
| 16 | `include/acsd/variance_propagation.h` (86) | 全 86 行 | 文档与实现一致。`:80-82` 的「自证」闭式不引用实现，若被当判据用则无鉴别力 | 建议 |
| 17 | `integration/README.md` (84) | 全 84 行 | 与 module.yaml 同型脱节。`:83` 指向的 `docs/detail/phase2_int.md` **不存在** | 须修 |
| 18 | `integration/memory.md` (81) | 全 81 行 | `:37` 引 `INTEGRATION.md:58` 作为已登记缺陷 DISP-P2INT-002 的依据——**我已亲自核对 `INTEGRATION.md:55-59`，该行实为 `n_accepted = \|{i \| accepted(i)}\|`，内容完全不符** ⇒ 该缺陷登记依据失效 | 须修 |
| 19 | `phase1_product/README.md` (73) | 全 73 行 | `:45-47` 把 `weight.kind="psfsw_robust_weight"` 写成**应写入的 canonical 值**，与 `phase1_product.cpp:635-640`「不再写出」及 `:1131-1139` 判红直接矛盾 | 须修 |
| 20 | `phase2_integrate/README.md` (63) | 全 63 行 | `:11` 头文件路径错（`lib/include/acsd/…` 不存在）；交付物表只列 3 个文件（实际 21 个）；**全文未提 `oracle/` 目录** | 建议 |
| 21 | `phase1_product/CMakeLists.txt` (58) | 全 58 行 | 自包含。无问题 | 通过 |
| 22 | `oracle/CMakeLists.txt` (41) | 全 41 行 | `:5` 自陈「不注册 ctest」。经核对根 `CMakeLists.txt:1460-1461` 确实只 `add_subdirectory` 了 phase1_product 与 phase2_integrate，**oracle 确实不在根构建图**（该自陈属实） | 须修（判据无人执行） |

---

## 4. 发现清单

### 阻断（7）

| ID | `文件:行` | 问题 | 推导/复现 |
|---|---|---|---|
| **B1** | `weight_chain.cpp:197-200`+`:219`；断言点 `oracle/weight_chain_selfcheck.cpp:302-303`、`:819-820`、`eng/tests/integration/p2_integrate/p2_pixel_weight_test.cpp:123`；契约 `weight_chain.h:426-435` | **自洽式断言**：`dimensional_identity` 由自身定义式代数恒等于 0，却被三处当判据断言、并在头文件明文邀请下游「直接断言」 | `w=(SNR/F_ref)²·g²` ⇒ `w·F_ref²−SNR²·g²≡0`（精确）。对任何实现缺陷零鉴别力 |
| **B2** | `phase2_integrate.cpp:952-956`（污染 `:998`→`:745`）；条款 `eng/contracts/data/clause_registry.json:1868` | **恒真门**：`snr_rel≡+0.0`，`FZ-AP2PT-SNR-IDENT-RTOL` 结构上不可红；且实现根本没做条款要求的 `SNR_comb²=ΣSNR_k²` | `:868` 与 `:954` 同容器/同序/同初值/同 FP64 加法 ⇒ 逐位相同；无 `-ffast-math`（我已核对本 target CMakeLists 全文） |
| **B3** | `phase2_integrate.cpp:1481-1488`、`:1494`、`:1569`；权威 `sampler.h:235-243`、`sampler.cpp:1479-1489` | `FZ-DEGRADE-SCALAR` 三重退化：三个门判据量写死 0.0 + 阈值冒签 `SO-07 PENDING_OWNER_SIGNOFF` + 返回码不裁决；evidence 虚假背书 | `sampler.cpp:1479-1489` 我已亲自读：阈值比较只用 `spatial_residual_p95/spatial_trend/power_loss`，恰是写死的三个 |
| **B4** | `oracle/weight_chain_selfcheck.cpp:914-926`（标为「M06 判据②（反例，可判红）」） | **伪注入 → 恒真门**：所谓「注入」从未触碰被测实现，两侧都由同一个 `oracle_weight_from_snr` 算出 ⇒ 比值恒为 `frame_snr²=40000`，**可证永远红不了** | `:915 w_inject_I=oracle(200·layer)`，`:910 w_true=oracle(layer)` ⇒ `rel_I≡39999`；`:923 w_inject_II=w_true·200²` 同理。Python 侧 `weight_chain_oracle.py:353-355` 同型复制 |
| **B5** | `oracle/recon_negative_injections.py:34`、`:111-112`、`:122`、`:133-134` | **仓内脚本就地改写生产源码** `lib/algorithms/integration/phase2_integrate/src/weight_chain.cpp`，**全程无 `try/finally`**：`:122 run_parity` 一旦抛异常或进程被中断，源码永久停留在 `/* INJECTED: … */` 状态 | `:116-120` 只对「构建失败」做了还原；`run_parity`/`json.load` 抛错路径无还原 |
| **B6** | `phase2_integrate.cpp:755-759`；`:387`、`:630`、`:758` | 产品记录 `gates` 块含**字面量 `true`**；`as.pixel_ivar_approx` 全 TU 从未赋 true ⇒ `:758` 恒真，`cov["approximation"]`（`:631-636`）永不生成，违反 `FZ-GATE-PIXIVAR-APPROX` | grep `pixel_ivar_approx` 全目录仅 4 处命中：声明 `:387`、读 `:630`、读 `:758`，**无任何赋值** |
| **B7** | `phase2_integrate.cpp:1236-1239`；`:346`；上游 `phase1_product.cpp:687`/`:690`/`:1208-1212`、`lib/algorithms/noise_snr/cpp/src/information_weight.cpp:313` | **恒绿门**：被检量与期望量互为倒数（`var_f=1.0/w` 与 `w`），Phase1 侧同构 ⇒ `:346` 磁盘一致性门同样恒绿 | `(1/w)·w` 相对误差 ≤2⁻⁵²≈2.2e-16 ≪ 1e-9。对「`w_info` 整体算错」（漏 `a_k²`、漏 `C⁻¹`、量纲错）**零鉴别力**，错误串却承诺 `Var(F_hat) != 1/W_info (FZ-FORMULA-WINFO)` |

### 须修（11）

| ID | `文件:行` | 问题 |
|---|---|---|
| S1 | `phase2_integrate.cpp:1499-1508` | coverage/support 分类输入全常量（`cov(P,1)`/`cov_known(P,1)`/`sup_known(P,1)`/`sup(P,K)`）⇒ `n_supported≡P`、`n_uncovered≡0`、`n_unavailable≡0`，**与真实数据零依赖**。真实交集 `fs.frames[k].view.target_ipix` 已在 `:1283-1291` 算出却未用 |
| S2 | `phase2_integrate.cpp:1339`、`:1344` | `p2_upm_ma_solution` **返回值完全忽略**。该帧若不在 model 中，输出保持调用方初值 `0.0`，被 `r.g_k/r.b_k/r.s_p` 当**真实解**推入公开结果 |
| S3 | `phase2_integrate.cpp:1332-1334` | `P2UpmMaInfo.identifiable`（`upm.h:340` 原文「判据的唯一判决位：1=绿，0=红」）**未读取**，同族 `n_unidentified`/`rank_rtol_effective`/`additive_only_components` 一并丢弃 ⇒ 秩亏模型照样 `r.ok=true` |
| S4 | `phase2_integrate.cpp:1301`、`:1310` | `if (!found \|\| idx >= P) continue;` 静默丢观测；`P` 取自**帧 0**，而 `idx` 是**帧 k 自己** `target_ipix` 的下标，两者无交叉校验。`:1310` 的 `obs.size() < controls.size()*2` 只挡总量，挡不住「某帧贡献 0」 |
| S5 | `phase2_integrate.cpp:891`、`:971`、`:843`/`:846` | `psf_profile[i]` **越界读**：`joint.m` 与 `psf_profile.size()` 无任何校验，而 `:329` 只 `push_back` 无长度/NaN/Σ≈1 校验 |
| S6 | `phase2_integrate.cpp:1410-1422` | 分类排异喂**合成残差**：`n=4` 固定、`residual[i]=y0-pred+0.3·i`（i=1,2,3 是人造偏移）、`y0` 又取自 `frames[0]`。`accepted_count/rejected_*/z` 全是合成产物却写入公开结果字段 |
| S7 | `phase2_integrate.cpp:1452-1479` | 「空间模型」只取 `frames[0]`、`ny=2` 两行**逐位相同**、`ra0=dec0=0.0`（Phase1 链无 WCS，`pixel_to_sky` 被丢弃）、`step_deg=1e-3`；`:1467` `eval` 的返回值从未被使用。产出的 p05/p50/p95/max_dev/model_error 仍当真实摘要上报 |
| S8 | `phase2_integrate.cpp:1462` + `sampler.cpp:1379` | **筛掉真信号**：`valid=isfinite(v)?1:0`，下游 `if (model->valid[s]==0) continue;` ⇒ **NaN/Inf 像素被静默剔除**后再统计 p05/p50/p95/maxdev/model_error。坏像素恰是最该报的；剔除比例 `sampling_coverage` 无门消费 |
| S9 | `phase1_product.cpp:566-568` | covariance 传播门用 `aperture_w(nd, 1.0)`（注释自陈「gate 需要真实权重」）**全 1 伪权重**跑 `gate_covariance_propagation`，结果却作为 `FZ-FORMULA-COV-PROP` 绿写进 `:928` |
| S10 | `phase1_product.cpp:250-253` | provenance 输入哈希缺失时**静默伪造**：`"sha256:" + Sha256::hex_of_string(in.frame_id)`——把 frame_id 的哈希冒充输入帧哈希写入溯源链，无任何错误 |
| S11 | `phase1_product.cpp:319`/`:624`；`phase2_integrate.h:139` | **硬编码时间戳** `generated_utc = "2026-09-15T00:00:00Z"`（phase1）与 `"2026-09-16T00:00:00Z"`（phase2 默认）——每个产品都带同一个伪造的生成时刻 |

### 建议（13）

| ID | `文件:行` | 问题 |
|---|---|---|
| C1 | `phase2_integrate.cpp:503-504` vs `:611-614` | IVAR 声明 `invalid_policy="nan_or_support_le_0"`，实写 `0.0`；把「无效」伪装成「零信息量」。（phase1 `:678` 声明 `zero_when_variance_le_0` 与其行为**一致**，仅 phase2 不一致） |
| C2 | `phase2_integrate.cpp:503-504`、`:538`；`phase1_product.cpp:539` | `std::max(a, NaN)` 被当 NaN 过滤器用 3 处，实际语义**相反**（返回丢掉 NaN 后的「看起来正常」的值）。`phase1_product.cpp:539` 处若协方差全 NaN ⇒ `corr_rep` 停在 `"diagonal_variance"`、`diagonal_variance_only=true`，**全 NaN 协方差被上报为对角**，下游按对角消费 |
| C3 | `phase2_integrate.cpp:968` | `invert()` 返回值忽略。**不可达**（同输入，`:884` 已判过），但若 `:884` 路径改动则 `ci` 为空 vector、`:976` 越界 UB |
| C4 | `phase2_integrate.cpp:155` | `force_effective_psf_only_fwhm` 内含死行：`:722` 设 `values_or_model`，`:723` 立即 `erase` 同一键 |
| C5 | `phase2_integrate.cpp:727-729` vs `:760` | `inject_p33_key` 注入写 `doc["phase2_extensions"][key]`，但 `:760 doc["phase2_extensions"] = ext;` 是注入区之后**唯一**的整体赋值 ⇒ 注入被整体覆盖。**诚实说明**：全仓 grep 仅 3 处命中（声明 + 两行死代码），**无人设置它**，故当前不存在假绿；性质是「判据名义存在、实际不存在」+ 后人加测试即得假绿的陷阱。同批 9 个注入字段中仅 3 个被测试设置 |
| C6 | `phase2_integrate.cpp:43`/`:48`/`:167` | 死代码 `kPi`、`mat_get`、`nearest_rank_percentile`（全仓零调用，违反 AGENTS.md §6）。另 `:171` 的 `floor(p*(n-1))` 在 n=2,p=0.5 时返回 **min 而非中位数** |
| C7 | `phase2_integrate.cpp:502-504`（ivar 无 `has_*` 语义）、`:511` | `nearest_rank_percentile` 之外，`Assembly` 无「未实现」标记，`:741-753` 把从未赋值的 `median_wt`/`rho_p05..rho_max`/`var_gls`/`x_hat`/`w_psfsw`（空数组）**当诊断量落盘**，消费者无法区分「测得为 0」与「根本没测」 |
| C8 | `phase2_integrate.cpp:353` vs `:740` | evidence 宣称「OI-01: group median=1 normalization performed in phase2」，而记录 `group_normalization_performed = as.has_psfsw`（`:391` 声明 false、**从未赋 true**）⇒ 同一产品自相矛盾 |
| C9 | `phase2_integrate.h:233` + `phase2_integrate.cpp:1493-1494` | 字段名 `scalar_verdict` 存的是**返回码**不是 verdict 枚举；真正的 `verdict` 被丢弃。命名与内容不符 |
| C10 | `integration/module.yaml:76-78`、`:19-20`、`:4` | `source_symbols` 列的两个符号在本目录**零命中**（实际在 `lib/algorithms/coverage/`）；`:19-20` 自称本目录「仅合同文件、无源码」而实有 8158 行；`:4` 声称「构建挂载: 根 CMakeLists.txt:336-346」——**我已亲自核对根 :336-346 是平台库块与 `add_subdirectory(eng/tests/conformance/noop)`，无 `acsd_phase2` 目标**，真实注册在根 `:1393/1395/1403/1460/1461` |
| C11 | `integration/memory.md:37` | 引 `INTEGRATION.md:58` 为已登记缺陷 DISP-P2INT-002 的依据——我已核对 `INTEGRATION.md:55-59`，该处实为 `n_accepted = \|{i \| accepted(i)}\|`，**内容完全不符** ⇒ 该缺陷登记依据失效 |
| C12 | `phase1_product/README.md:45-47` | 把 `weight.kind="psfsw_robust_weight"` 写成**应写入的 canonical 值**，与 `phase1_product.cpp:635-640`「不再写出」及 `:1131-1139` 判红直接矛盾（代码是对的，README 陈旧） |
| C13 | `phase2_integrate/README.md:11`、`:27`、`:59`；`integration/README.md:83` | 悬空/错误引用：`:11` 头文件路径 `lib/include/acsd/phase2_integrate.h` 不存在（实为 `include/acsd/…`）；`:27` 的 `lib/{dynamic_psf,snr_estimator,photometric_calib}` 三者皆无；`:59` 的 `lib/phase1/**` 不存在；`README.md:83` 与 `memory.md:78` 的 `docs/detail/phase2_int.md` 不存在 |

**另记 1 条自证反例**（我主动验伪自己的假设）：我一度怀疑 `phase2_integrate.cpp:128-137` 的 `kForbiddenWeightSources` 未含退役对象名 `psfsw_robust_weight`，导致 `:552` 写出的退役 token 无守卫。**亲自 grep 核对后该假设不成立**——`:132` 含 `"psfsw_robust_weight"`。但 `:552` 仍把 `{"psfsw_robust_weight","1"}` 写进 Phase2 生产产品的 `units` 块（Phase1 已在 `:635-640` 显式移除该项），且 `:1214-1218` 的守卫只查 `weight.sources` 不查 `units` ⇒ 记为建议级不一致。

---

## 5. 我主动构造的反例

本轮按「重新推导结论、构造反例」执行。以下反例**全部为源码级构造与静态推导，未编译、未运行**。

### R1 —— 证明 `dimensional_identity` 无法变红（推翻「量纲自证是判据」）

**构造**：取 `in.ref_flux_k = 812.5`、`layer_snr = 1.35`、`g = 41.0`。
**推导**：`weight_chain.cpp:104` 给出 `w = (1.35/812.5)²`；`:200` 后 `w = (1.35/812.5)²·41²`。
`:219` 展开：`((1.35/812.5)²·41²)·812.5² − 1.35²·41² = 1.35²·41² − 1.35²·41² = 0`（精确）。
**结论**：无论被测实现是否正确、层值是否算错、`g` 是否取错，只要 `w` 是按 `:197-200` 那条式子算出来的，`:219` 就恒为 0（只余 ≤2 ulp 的舍入）。
**是否推翻**：**已推翻**「`selfcheck.cpp:302` 与 `:819` 的断言是有判别力的质量门」这一现行结论。成立。

### R2 —— 证明 `selfcheck.cpp` 判据②的「注入」碰不到被测实现

**构造**：读 `weight_chain_selfcheck.cpp:905-913`，`weight_from_sparse_layer_pixel(pin)` 被调用**一次**，结果 `pr` 用于 `:911` 的真 oracle 比对。
**核对**：`:915` `w_inject_I = oracle_weight_from_snr(frame_snr * layer, fref)` 与 `:910` `w_true = oracle_weight_from_snr(layer, fref)` —— **两侧都出自同一个 `oracle_weight_from_snr`（`:53-56`）**，与 `pr` 无关。`:923` `w_inject_II = w_true * frame_snr * frame_snr` 同理。
**推导**：`w_inject_I / w_true = (200·layer/1000)² / (layer/1000)² = 200² = 40000` ⇒ `rel_I ≡ 39999 > 1e-6` **恒成立**，无论实现写成什么样。
**是否推翻**：**已推翻**注释 `:890`「M06 判据②（反例，可判红）」所声称的能力。`:918`/`:919`/`:925` 三条断言是恒真门。成立。
**注**：同文件 `:911-912`（`pr.weight` 对独立 `w_true`）**是真判据**，问题只在 `:915-926` 这一路。两者不可混淆。

### R3 —— 证明 `snr_rel` 是恒真门

**构造**：`:863` `const FrameSet fs = open_phase2_frame_set(...)`（`const`，构造后不变）；`:868` 累加 `f.view.w_info` 到 `w_sum`（初值 `0.0`）；`:954` 用**逐字相同**的循环累加到 `sum_w`（初值 `0.0`）。
**推导**：IEEE-754 binary64 `+` 在固定操作数序下是确定性纯函数 ⇒ `sum_w ≡ w_sum`（bit-identical）。`w` 仅在 `:914`（joint 分支）被改写，而该分支必置 `joint_used=true`，被 `:956` 的 `!joint_used` 短路。
**补充核对**：`phase2_integrate/CMakeLists.txt` 全文无 `-ffast-math`/`-fassociative-math`，排除重结合。
**是否推翻**：**已推翻**「`FZ-AP2PT-SNR-IDENT-RTOL` 能红」。成立。附带推翻条款符合性——`clause_registry.json:1868` 要求核 `SNR_comb²=ΣSNR_k²`，实现连 SNR 都没读。

### R4 —— 证明 `FZ-DEGRADE-SCALAR` 在本层结构上不可失败

**构造**：读 `sampler.h:240-249`（`P2ScalarSummaryInput`）与 `sampler.cpp:1479-1489`（门实现）。
**核对**：参与阈值比较的只有 `spatial_residual_p95`、`spatial_trend`、`power_loss`；而 `phase2_integrate.cpp:1486-1488` 三者**全部写死 0.0**。`:1489-1491` 从真实 `sum` 取的 6 个字段在门里只做合法性与 `p05≤p50≤p95` 校验。
**构造反例尝试**：设 `spatial_trend = 0.2 > 0.10` 应判红 —— 但 `:1487` 是字面量 `0.0`，调用方**无法**把真值送进去。
**是否推翻**：**已推翻**「该门在集成层生效」。成立。
**最重的一层**：`:1569` 仍无条件写入 `"FZ-DEGRADE-SCALAR: spatial gate + power-loss gate + p05/p50/p95"` evidence ⇒ **对从未可能失败的事签发合格证**。

### R5 —— 证明 `coverage/support` 分类恒为 `(P,0,0)`

**构造**：读 `coverage.h` 的判定规则并逐分支代入 `:1499-1504` 的常量输入。
**推导**：`coverage_known=1 ∧ support_known=1` ⇒ 不进 UNAVAILABLE；`coverage[k]=1≠0` ⇒ 不进 UNCOVERED；`support_frames[k]=K ≥ min_support_frames=2`（`:1258` 已保证 `K≥2`）⇒ 不进 COVERED_UNSUPPORTED ⇒ 恒 `P2_CELL_SUPPORTED`。
**是否推翻**：**已推翻**「该分类反映真实覆盖」。成立。真实交集 `:1283-1291` 已算出却未接。

### R6 —— 证明 `var_f * w_info ≈ 1` 恒绿（B7）

**构造**：`phase2_integrate.cpp:950` `var_f = 1.0/w`，`:992` `as.w_info = w`；`:655`/`:658` 分别落盘；`:1236-1239` 读回判 `|(1/w)·w − 1| ≤ 1e-9`。
**推导**：相对误差 ≤ 2⁻⁵² ≈ 2.2e-16。nlohmann 双精度最短往返序列化无损 ⇒ 落盘环节也救不红它。
**Phase1 侧**：我已亲自读 `information_weight.cpp:313` `out.var_f = 1.0/out.sum_w`，配合 `phase1_product.cpp:687/690/1208-1212`，确认 `:346` 磁盘门同构恒绿。
**是否推翻**：**已推翻**「`FZ-FORMULA-WINFO` 是公式门」。它实际只实现「W=1/Var」一半；合同另一面（CRLB）在生产路径上完全没有实现。

### R7 —— 尝试推翻「`weight_chain.cpp` 的控制点复现自检是自洽断言」→ **失败，已放弃该指控**

**构造**：`:689-690` 在节点坐标处调 `eval()`，`:695` 与 `grid_[idx]` 比。
**推导**：`:391-393` 中 `h = pos - i`，`:399` 在 `h=0` 时返回 `y0`。乍看像「必然返回输入值」的自洽式。
**但**：`grid_`（被检输入数据，来自 Phase1 磁盘）与 `eval()`（被测插值器，含 `my_` 预计算、索引、几何）是**两条独立路径**。把 `my_` 算到错的列、或索引转置，都会让 `:695` 残差爆炸并走 `:699-704` 判红。
**结论**：这是**真检查**，能区分实现缺陷。**明确不列为问题**（防止夸大）。`oracle/weight_chain_selfcheck.cpp` 的独立稠密高斯消元 oracle（`:77-114`）交叉验证也证实该路径正确。

### R8 —— 尝试推翻「`corr_ratio` 门是自洽式断言」→ **失败，已放弃该指控**

**构造**：`:937` `corr_ratio = w_naive / wj`。
**推导**：`w_naive` 走**逐帧块对角求逆**（`:917-934`），`wj` 走**全矩阵求逆**（`:899-911`）。对非块对角 `C_in`，两条算法必然给出不同值。
**结论**：真门。**不列为问题**。

### R9 —— 尝试推翻「sha256 三方比对是自洽门」→ **失败，已放弃该指控**

**构造**：manifest/provenance 的 sha 在 `:531` 对 **staging 路径**计算，`:1158` 在 **rename 之后对 target 路径**重算。
**结论**：路径与时刻都不同，是真的「文件是否在 rename 中完好」检查。**不列为问题**。

### R10 —— `recon_negative_injections.py` 的中断反例

**构造**：`:111-112` 把打过补丁的 `weight_chain.cpp` 写回**生产路径**；`:133-134` 才是还原。
**反例**：在 `:112` 与 `:133` 之间发生 `KeyboardInterrupt` / OOM / 超时 / CI 取消 —— `:116-120` 的还原只覆盖「cmake 构建失败」一种路径，其余全部逃逸。
**结论**：源码永久停留在 `/* INJECTED: clip removed */` 状态且 `ready_`/钳制逻辑已被破坏。**成立**。同时违反本轮「不许改任何仓内文件」与仓内治理纪律。

---

## 6. 盲复算

**口径**：遮住既有判定（`逐份判定-权威版.csv` 与他人 `审稿-*.md` 对本片的条目），仅凭原文独立取证，再比对。**未读**他人对本片的结论作为先验。

| 独立取证得到的结论 | 与既有判定的关系 |
|---|---|
| `dimensional_identity` 自洽式断言 | **偏松**——既有登记未把它列为判据缺陷（它被当作"量纲自证"正面引用，`weight_chain.h:426-435` 甚至主动推广） |
| `snr_rel` 恒真门 + 污染产品记录 | **偏松** |
| `var_f*w_info` 恒绿门（含 Phase1 同构） | **偏松** |
| `FZ-DEGRADE-SCALAR` 输入写死 0 + 阈值冒签 + rc 不裁决 + evidence 虚假背书 | **偏松**——既有登记关注「门实现是否退化」，未发现调用方从不喂真数据且从不裁决 rc |
| `selfcheck.cpp:914-926` 伪注入（可证永不红） | **偏松**——既有把它记作「反例注入，可判红」 |
| `gates` 块字面量 `true`、`pixel_ivar_approx` 从未赋值 | **偏松** |
| `inject_p33_key` 被 `:760` 整体覆盖 | **一致**（与子代理独立结论一致）；我额外给出诚实限定：无人设置它，故当前无假绿 |
| 模块三件套描述的是另一个模块、且自称本目录无源码 | **偏松** |
| `recon_negative_injections.py` 就地改写生产源码无 `try/finally` | **偏松** |
| `std::max(a, NaN)` 当 NaN 过滤器（3 处） | **偏松** |
| 硬编码 provenance 输入哈希 / 硬编码时间戳 | **偏松** |
| 全部 4 个 python oracle 零 `add_test` | **偏松** |
| `weight_chain.cpp` 权重链本体 | **一致偏严归我方**：我与两名子代理均认为其 fail-closed 质量最高，唯一问题是 `:219`；但我**否决**了「`:685-704` 控制点复现自检是伪门」的指控（R7） |
| `variance_propagation.{h,cpp}` | **一致**：干净，无自洽断言、无恒真门 |
| `kCommonMin` | **一致**：= 3，与 `:313` 错误文案一致，怀疑不成立 |
| 生产 6 文件私建线程池 | **一致**：无。唯一 `std::thread` 在 `oracle/weight_chain_selfcheck.cpp:526`，且我已亲自核对根 `CMakeLists.txt:1460-1461` 确认 oracle 确实不在根构建图，该文件 `:521-523` 的自陈属实 |
| `recon_exp04_parity.py` 判据纪律 | **一致偏严归我方**：G10 未传 oracle JSON 时显式 FAIL、G9 crop 不匹配显式 SKIP，是本片最好的判据设计 |

**总体：判定整体偏松。** 既有登记把 `weight_chain` 的 "量纲自证" 与 `selfcheck` 的 "判据②反例注入" 当作正面证据，而这两处正是本片最隐蔽的自洽式/恒真断言。

---

## 7. 子代理派发记录

**派发 4 个**（满足 3-5 要求；其中第 1、4 号提示词重复，构成一次独立交叉投票）。

| ID | 角度 | 读了多少 |
|---|---|---|
| `a26e200e` | 自洽式/恒真门/恒红门/筛掉真信号（weight_chain + phase2 + variance_propagation） | 5 文件 3334/3334 行 = **100%** |
| `cdba353b` | 同上（重复派发，用于独立交叉投票） | 5 文件 3334/3334 行 = **100%** |
| `4a4d8996` | 静默降级 / 错误码语义 / 私建线程池 / 硬编码 / 数值稳定性（phase1_product + phase2 + weight_chain） | 6 文件 4344/4344 行 = **100%** + 支撑契约 ~240 行 |
| `cd06cc09` | 负向注入有效性 / oracle 独立性 / 悬空引用 | 13 文件 4412/4412 行 = **100%** |
| `97aa2c91` | 自愈判据锚 / 退役对象活调用者 / 筛掉真信号（phase1_product + 头文件 + variance_propagation） | 未回报告，未采信任何结论 |

**我逐条复核方式**：不直接采信子代理结论，对每条关键指控**亲自 `read`/`grep`/`sed` 复核原始行**，确认后才写入本报告。

### 已采纳（复核通过）

| 来源 | 指控 | 我的独立复核 |
|---|---|---|
| `cd06cc09` | `inject_p33_key` 被 `:760` 整体赋值覆盖 | ✅ 亲自核对 `:727-729` / `:760` 顺序，确认 |
| `cd06cc09` | `phase2_integrate/CMakeLists.txt` 无 `add_subdirectory(oracle)`，4 个 python oracle 零 `add_test` | ✅ 亲自核对根 `CMakeLists.txt:1460-1461` 与 `:1508-1510` |
| `cd06cc09` | `clause_registry.json#FZ-PROV-KCORR-VALUE` **真实存在**且 `value: 1.4` 与 `kKCorrFrozen=1.4` 一致 | ✅ 采纳为「查证准确」，明确记为**不是**问题 |
| `cd06cc09` | `coverage.cpp:414`、`UNIFIED_MODEL.md:58`、`DATA_SEMANTICS §31.8`、`p2_rej_test.cpp:438-469` 精确命中 | ✅ 采纳为「查证准确」，不列为悬空引用 |
| `a26e200e` / `cdba353b` | H1 `snr_rel` 恒真门 | ✅ 亲自复核 `:867-868`/`:871`/`:914`/`:953-956`，确认（并补充条款 subject 不符这一层） |
| `a26e200e` / `cdba353b` | H2 `var_f*w_info` 恒绿门 | ✅ 亲自复核 `:950`/`:992`/`:1238`，并读 `information_weight.cpp:313` 确认 Phase1 同构 |
| `4a4d8996` | D1 阈值冒签：`sampler.h:235-237` 原文 `SO-07 PENDING_OWNER_SIGNOFF` | ✅ **亲自读该行**，确认；这是 B3 的核心 |
| `4a4d8996` | A8 `info.identifiable` 判决位未读 | ✅ 亲自读 `upm.h:340` 与 `phase2_integrate.cpp:1332-1334`，确认 |
| `4a4d8996` | E3 `psf_profile` 越界读无交叉校验 | ✅ 复核 `:329`（只 push_back）、`:891`/`:971`，确认 |
| `4a4d8996` | E1/E2 `std::max(a, NaN)` 语义反转 | ✅ 复核 `phase1_product.cpp:539`、`phase2_integrate.cpp:538`/`:1270`，确认 |
| `a26e200e` | N5 NaN 像素被静默剔除后再统计 | ✅ 复核 `:1462` 与 `sampler.cpp:1379`，确认（归入 S8） |

### 已否决 / 我改判（4 条）

| 来源的结论 | 我的裁决 | 理由 |
|---|---|---|
| `cdba353b` 的 V1 部分：「`weight_chain.cpp:219` `dimensional_identity` 有独立 oracle 交叉验证（`:295`）在旁，**不构成伪门**」 | **否决** | `:295` 验证的是**另一条断言**（`pr.weight` 对独立 oracle）。`:302-303` 与 `:819-820` 是**独立存在**的两条断言，它们断言的量 `dimensional_identity` 由 `weight_chain.cpp:197-200` 同一式导出，代数恒等于 0。「同一区块里另有一条真断言」不能使这条假断言变真。另两名子代理（`a26e200e`、`4a4d8996`）独立判成立 ⇒ 2:1，我维持原判并记录该分歧 |
| `cdba353b` 的 V1 后半 + `4a4d8996` 的 C1：「`weight_chain.cpp:685-704` 控制点复现自检可能是自洽断言」 | **否决（我主动撤回该指控方向）** | 见 R7。`grid_`（被检数据）与 `eval()`（被测插值器）是两条独立路径，写坏插值器会真红。这是**真检查** |
| `cdba353b` 的 V2 / V3：「`corr_ratio` 门」「sha256 三方比对」是真门 | **采纳为否决我的怀疑** | 见 R8、R9。我主动验伪了这两条，未列为问题 |
| `cd06cc09` 关于 `weight_chain_oracle.py:369-373`/`:377-383` 同义反复 | **采纳** | 我已亲自读 `:135-147`，确认 `oracle_reconstruct` 内部调用 `oracle_spline_clip`；两侧同源 ⇒ 该两条断言不可红（记 C5 同级建议） |
| 我自己一度提出的假设：`:128-137` `kForbiddenWeightSources` 缺 `psfsw_robust_weight` | **自我否决** | grep 确认 `:132` 含该 token。假设不成立，已在 §4 末尾如实记录 |

### 子代理声明的限制（我采纳）

`cd06cc09` 自陈「所有『是否真判红』的结论均为**源码路径分析**，非运行时实测」。**我采纳该限制**：本片全部「恒真门/恒绿门」结论均为**静态代数推导 + 数据流追踪**，未运行任何二进制。要把它们变成运行期证据需前台执行（已列入 §8）。

---

## 8. 自证段（可复跑命令）

全部为**只读**命令，不编译、不运行仓内二进制、不写文件。

```bash
cd "/workspace/Astro CS Database"

# ── 覆盖口径 ─────────────────────────────────────────────
sed -n '333,362p' run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml
for f in phase2_integrate/src/phase2_integrate.cpp phase1_product/src/phase1_product.cpp \
  phase2_integrate/oracle/weight_chain_selfcheck.cpp phase2_integrate/src/weight_chain.cpp \
  phase2_integrate/oracle/weight_chain_oracle.py phase2_integrate/include/acsd/weight_chain.h \
  phase2_integrate/oracle/recon_exp04_parity.py phase2_integrate/include/acsd/phase2_integrate.h \
  phase1_product/include/acsd/phase1_product.h phase2_integrate/src/variance_propagation.cpp \
  phase2_integrate/oracle/recon_negative_injections.py phase2_integrate/oracle/recon_dump.cpp \
  phase2_integrate/CMakeLists.txt phase2_integrate/oracle/recon_contract_gate.py module.yaml \
  phase2_integrate/include/acsd/variance_propagation.h README.md memory.md \
  phase1_product/README.md phase2_integrate/README.md phase1_product/CMakeLists.txt \
  phase2_integrate/oracle/CMakeLists.txt; do
  printf "%6d  %s\n" "$(wc -l < "lib/algorithms/integration/$f")" "$f"; done | tee /tmp/shard.txt
awk '{s+=$1} END {print "TOTAL =", s}' /tmp/shard.txt    # 期望 8158

# ── B1 自洽式断言 ────────────────────────────────────────
sed -n '195,220p' lib/algorithms/integration/phase2_integrate/src/weight_chain.cpp
sed -n '426,435p'  lib/algorithms/integration/phase2_integrate/include/acsd/weight_chain.h
grep -n "dimensional_identity" -r lib eng | grep -v Binary

# ── B2 snr_rel 恒真门 ────────────────────────────────────
sed -n '867,871p;913,914p;951,960p;996,999p' lib/algorithms/integration/phase2_integrate/src/phase2_integrate.cpp
grep -n -A4 'FZ-AP2PT-SNR-IDENT-RTOL' eng/contracts/data/clause_registry.json

# ── B3 FZ-DEGRADE-SCALAR 三重退化 ────────────────────────
sed -n '1480,1495p;1566,1571p' lib/algorithms/integration/phase2_integrate/src/phase2_integrate.cpp
sed -n '234,244p' lib/algorithms/coverage/include/astro/phase2/sampler.h   # SO-07 PENDING_OWNER_SIGNOFF
sed -n '1479,1489p' lib/algorithms/coverage/src/sampler.cpp               # 阈值只比那三个量

# ── B4 伪注入 ────────────────────────────────────────────
sed -n '897,930p' lib/algorithms/integration/phase2_integrate/oracle/weight_chain_selfcheck.cpp
sed -n '135,147p;353,357p;369,383p' lib/algorithms/integration/phase2_integrate/oracle/weight_chain_oracle.py

# ── B5 就地改写生产源码 ──────────────────────────────────
sed -n '34p;105,120p;122p;132,138p' lib/algorithms/integration/phase2_integrate/oracle/recon_negative_injections.py

# ── B6 字面量 gates ──────────────────────────────────────
grep -n "pixel_ivar_approx" lib/algorithms/integration/phase2_integrate/src/phase2_integrate.cpp  # 4 命中，0 赋值
sed -n '755,760p' lib/algorithms/integration/phase2_integrate/src/phase2_integrate.cpp

# ── B7 互为倒数门 ────────────────────────────────────────
sed -n '949,950p;992p;1236,1239p' lib/algorithms/integration/phase2_integrate/src/phase2_integrate.cpp
sed -n '346p'  lib/algorithms/integration/phase2_integrate/src/phase2_integrate.cpp
sed -n '311,314p' lib/algorithms/noise_snr/cpp/src/information_weight.cpp

# ── S1/S4/S6/S7/S8 接线层取合成数据 ──────────────────────
sed -n '1298,1312p;1410,1422p;1452,1479p;1497,1509p' lib/algorithms/integration/phase2_integrate/src/phase2_integrate.cpp

# ── S3 identifiable 判决位被丢 ───────────────────────────
sed -n '338,342p' lib/algorithms/coverage/include/astro/phase2/upm.h
sed -n '1332,1334p' lib/algorithms/integration/phase2_integrate/src/phase2_integrate.cpp

# ── S9/S10/S11 phase1 硬编码与伪溯源 ─────────────────────
sed -n '250,253p;302p;319p;566,568p;624p' lib/algorithms/integration/phase1_product/src/phase1_product.cpp

# ── C10/C11/C12/C13 悬空引用与三件套脱节 ─────────────────
grep -n "p2_integrate_pixel" -r lib/algorithms/integration/ | grep -v '^Binary'   # 只在三件套里命中，源码零命中
sed -n '336,346p' CMakeLists.txt        # 子代理声称 acsd_phase2 目标在此 —— 实为平台库块
sed -n '1460,1461p' CMakeLists.txt      # 真实注册处；无 oracle
ls docs/detail/phase2_int.md docs/detail/algorithms_phase2/ 2>&1 | head
sed -n '55,59p' docs/science/INTEGRATION.md   # memory.md:37 引用处实为 n_accepted 定义

# ── 私建线程池（结论：生产 6 文件无） ────────────────────
grep -n "std::thread\|std::async\|omp\|#pragma parallel" -r lib/algorithms/integration/
```

**运行期证据（需前台执行，本轮未做）**：把 B1/B2/B3/B4/B7 的反例做成 `/tmp` 副本上的注入，构建 oracle 二进制后确认「注入缺陷 ⇒ 对应断言仍绿」。这是把本文的静态推导升级为运行期证据的唯一途径，本轮按纪律未执行。

---

## 8A. 增补：第 5 子代理报告的逐条复核（`97aa2c91`）

该代理在我交付初稿后回报。我对其**每条关键指控亲自 `read`/`grep`/`sed` 复核原始行**，复核通过的才写入；复核不通过的照记为否决。

### 复核通过并升级为阻断（2 条）

**B8 —— `phase1_product.cpp:1005` `Phase1WriteResult::output_sha256` 恒为空串**
```
res.output_sha256 = pr.sha256_hex;     // :1005
```
`atomic_publish_directory` 的返回值 `PublishResult` **从不给 `sha256_hex` 赋值**。我已亲自核对：`sha256_hex` 在 `atomic_publish.cpp` 全文只有 **1 处赋值**（`:422`），位于**单文件** `publish_fits_product` 内；`atomic_publish.h:104` `std::string sha256_hex;` 默认空串。
⇒ **Phase1 返回的产品哈希恒为空。**
**同仓对照**：Phase2 明确绕开了这点 —— `phase2_integrate.cpp:820-824` 加注释「output_hash = 磁盘实际 sha256」后用 `sha256_file_hex(target_dir + "/" + kPhase2ScienceFile, …)` 重算覆盖。**Phase1 没有这个补偿**。调用方拿空串当产物锚点 ⇒ 静默接受任何内容。
（诚实限定：我未核实仓内是否已有调用方消费 `Phase1WriteResult::output_sha256`；若无人消费则严重度降为须修。）

**B9 —— 一级工程正本仍把退役对象 `psfsw_robust` 登记为 `production`（跨片，提请文档片裁决）**
`docs/engineering/VALIDATION_EVIDENCE_STANDARD.md`（我已亲自读该表）仍含：
```
| `psfsw_robust` | production | 点源信号权重 | 1 | Wt_k=C_norm*S^alpha*Conc^beta/(N^gamma*B^delta); W_psfsw,k=Wt_k/median_j(Wt_j) | 必需 | 是 | ...
```
并保留 **CMP-03 / CMP-06 / CMP-07 / CMP-08** 四个把它当活臂的预注册比较单元。
AGENTS.md §3 规定**一级正本高于代码** ⇒ 退役在最上层活文档里**仍被正式授权**。本片全部 FZ-MODE-RETIRED 纪律（`phase1_product.cpp:52-77`、`phase2_integrate.cpp:192-216`）建立在这个未撤的授权之上。
**跨片提请**：文件在 `docs/`，归文档片；但它是本片退役链的权威出处，须由负责人统一裁决。

### 复核通过并升级为须修（6 条）

**S12 —— 本片 4 处对 `docs/ACSD_DESIGN.md §3.1` 的**直接引语**在原文中不存在**
`phase1_product.cpp:61-62`、`phase1_product.h:22`、`phase2_integrate.h:20-21`、`phase2_integrate.cpp:197-199` 均以引语形式引：「权重只能来自纯净信号与噪声之比……任何使偏差随帧而变的量（含 PSF 拟合质量代理）都不得进入科学叠加权重」。
**我已亲自 grep `docs/ACSD_DESIGN.md`：四个关键词全部 0 命中** —— `PSF 拟合质量代理` 0、`质量代理` 0、`纯净信号` 0、`不得进入科学叠加权重` 0。
真正支撑该结论的是 **`§2.2:131`**「可作科学叠加权重的对象只有两个（§3.1）：点源信息量 `point_information`… 广义最小二乘组合权重 `surface_gls`…」（我已亲自读该行）与 `§2.2:133`（PixInsight PSFSW 对标），**无一处代码引用**。
同段引用的另一半「§3.1 全链没有『权重模式』这一可选概念」**属实**（`:182` 原文在）。
⇒ **结论正确、出处错误**，违反 AGENTS.md §4「引用任何规范或文献条款前，先读原文确认它真实存在且表述一致」。

**S13 —— `phase1_product.cpp:1187-1190`：`p05≤p50≤p95` 门可被「缺席」满足**
```
const double p05 = c["spatial_summary"].value("p05", 0.0);   // 无 contains() 守卫
const double p50 = c["spatial_summary"].value("p50", 0.0);
const double p95 = c["spatial_summary"].value("p95", 0.0);
if (!(p05 <= p50 && p50 <= p95)) return fail("component p05<=p50<=p95 violated");
```
`nlohmann::json::value()` 的契约是 **key 缺失返回默认值、且不抛**。若 `spatial_summary` 整体缺失（被篡改的产品），三者全 `0.0` ⇒ `0≤0≤0` **通过**。这道门的名义是「校验真实空间摘要的序关系」，实际可在字段缺失时零成本满足。
（诚实限定：`c` 是 `const json&`，`c["spatial_summary"]` 在 const 重载下的行为随 nlohmann 版本与 `JSON_ASSERT` 配置而异（可能抛 `out_of_range` 或断言）。若抛异常则是崩溃而非假绿；无论哪种，**缺 `contains()` 守卫都是缺陷**。我未编译，无法判定该具体构建下的分支。）

**S14 —— `phase1_product.cpp:1169-1172`：两分支同条件，`valid` 不构成判别**
```
has_wv = w->contains("weight_value") && !(*w)["weight_value"].is_null();
if (valid && has_wv) return fail("valid=true but weight_value present (single frame)");
if (!valid && has_wv)  return fail("valid=false but weight_value present (FZ-GATE-PSFSW-FAILCLOSED)");
```
两分支**都要求 `has_wv` 为真**，唯一区别是错误文案。实际语义是「`weight_value` 存在 ⇒ 无条件拒绝」，而 `valid` 只选消息。写成二选一的判别形状，实为单条件 ⇒ **死判别**。

**S15 —— 只报第一条违规，失败身份依赖违规顺序**
`phase1_product.cpp:352` `"frozen units violated: " + uwhy[0]`（`uwhy` 可含 N 条，只取第 1）；`:1090-1094` `vr.violations.front()` 同型。⇒ 多违规并存时失败原因随内部顺序漂移，不可作为稳定失败身份。

**S16 —— `phase1_product.cpp:908` `overlap_area_rejected` 结构性恒为 0**
我已亲自核对：`spherical_overlap_science.h:62` `constexpr std::uint64_t kMaxInvalidAreaHits = 0;`，`spherical_overlap_science.cpp:114` `if (out->n_area_rejected > kMaxInvalidAreaHits) { … return overlap_area_invalid; }`。
⇒ **任何非零都在算子构建阶段硬失败**，故已发布产品中该字段必为 0。而 `:906-907` 注释称「非零即产品的几何闭合不完整（超阈值时算子构建本身已具名失败，故此处数值是『可容忍但必须留痕』的量）」——**自相矛盾**：可容忍值不可能出现在已发布产品里。重开门也从不做「值 vs 容差」比对 ⇒ 该诊断量是不可证伪锚。

### 复核通过并升级为建议（6 条）

| ID | 位置 | 问题（我已逐条复核） |
|---|---|---|
| C14 | `phase1_product.cpp:487`、`:959`、`:1009` | `fit_err` 声明 → `clear()` → `(void)`，**从未赋值也从未读取**。刻意压制告警的死脚手架 |
| C15 | `phase1_product.cpp:1183-1191` | `valid_area_fraction` 写出于 `:730`，重开门**从不校验** |
| C16 | `phase1_product.cpp:539-545` | `rho` 只在 `d = sqrt(var[p]*var[q]) > 0` 守卫内累加 ⇒ **恰好排除零方差像素**（无数据的最差像素）；而 `:538` 的 `max_abs_cov` 无守卫 ⇒ `corr_scale` 与 `rho_max` 来自**不同总体**，两者的比值无物理意义 |
| C17 | `lib/infrastructure/scheduler/budget.py:606` | `sp.add_argument("--mode", default="psfsw_robust")` —— 生产调度器的**活默认 token 是退役对象**。跨片，提请 `lib/infrastructure` 片 |
| C18 | `runtime_contract.h:174`（跨片） | 引用 `eng/ci/check_no_weight_mode_code.py` 作为「退役对象拒绝面必须存活」的门。**我已亲自核对：`eng/ci/` 目录不存在，`git ls-files eng/ci` 为空** —— 该纪律门已不在仓内 |
| C19 | `phase2_integrate.cpp:552` + `:1117-1122` | Phase2 生产记录**无条件**把退役符号写入 `doc["units"]`（Phase1 已为同一理由专门删掉，见 `phase1_product.cpp:635-640`）；`open_phase2_product` 只校验 signal/variance/ivar 三个单位串，**从不检查** `units.psfsw_robust_weight` ⇒ 退役符号原样通过重开门。**此项把我原先的「建议级不一致」升级**（`:132` 的禁词只守 `weight.sources`，不守 `units`） |

### 该代理的否决项，我逐条复核后**采纳**

它否决了我 3 项怀疑，证据成立，我维持放弃：①`:128-137` 禁词表**确实含** `psfsw_robust_weight`（`:132`）；②`:719` `force_variance_from_weight` 注入会被 `:1169-1172` 的枚举限制正确判红；③`is_retired_weight_mode_token` 只认 `psfsw_robust` **不是漏网**——`psfsw_robust` 是**声明 token**、在 `weight_mode` 词表，`psfsw_robust_weight` 是**对象/单位 token**、在另一词表，两者是设计上的分表而非疏漏。
它另有 6 项「引用准确」清单（`atomic_publish.h:28-31`、`atomic_publish.cpp:519-528`/`:154-160`、`coverage.cpp:414`、`p2_rej_test.cpp:438-469`、`UNIFIED_MODEL.md:58`、`DATA_SEMANTICS §31.1/31.3/31.7/31.8`），我采纳为**不是**悬空引用，不列入 §4。

### 该代理自陈的限制（我采纳）

它声明「未跑构建，恒真/恒空结论均为静态可达性推导」。**我采纳并沿用同一限定**，且已在上文对 S13 额外标注了「`operator[]` const 重载行为随版本/断言配置而异」这一我不完全确定的分支。

---

## 9. 交付说明

- 本片 **22/22 份、8158/8158 行、覆盖率 100%**，无未读完项。
- **阻断 9（B1-B9）、须修 17（S1-S16）、建议 19（C1-C19）**。第 5 子代理回报后经逐条复核追加 2 阻断 + 6 须修 + 6 建议，见 §8A。
- 判定 **阻断**。核心不在阈值松紧，而在**判据本身不存在**：`dimensional_identity`（B1）是精确恒等式却被三处立为门并由头文件主动推广；`snr_rel`（B2）与 `var_f*w_info`（B7）是被检量与期望量同源；`selfcheck` 判据②（B4）的注入压根没碰被测实现；`FZ-DEGRADE-SCALAR`（B3）三重退化后仍由 `:1569` 签发合格证。
- 与负责人本轮口径一致的核心产出：**4 处「用同一个定义式既当被检量又当期望量」的自洽式断言/恒真门**（B1、B2、B4、B7），其中 2 处（B1、B2）会把恒 0 写进对外可见的生产产品记录。
- 另有 4 条属**本轮口径之外的独立产出**：`recon_negative_injections.py` 无 `try/finally` 就地改写生产源码（B5）；`Phase1WriteResult::output_sha256` 恒空串（B8）；模块三件套描述的是另一个模块、自称本目录"无源码"而实有 8158 行（C10）；**一级工程正本仍授权退役对象 + 本片 4 处直接引语在 `ACSD_DESIGN.md` 中 0 命中**（B9 / S12）。
- ⚠️ **两条跨片发现须转交**：`docs/engineering/VALIDATION_EVIDENCE_STANDARD.md` 仍把 `psfsw_robust` 登记为 production（B9，归文档片）；`lib/infrastructure/scheduler/budget.py:606` 活默认 token 为退役对象、`eng/ci/check_no_weight_mode_code.py` 已不在仓内（C17/C18，归基础设施片）。
- 已如实记录 **1 条自我否决**（`:132` 含退役 token）、**6 条验伪后放弃的指控**（R7/R8/R9 + 3 项由第 5 代理否决并经我复核成立）、**1 条子代理间的实质分歧**（`cdba353b` 对 B1 的部分否决，已驳回并留痕，2:1）、**1 处我明确标注不完全确定的分支**（S13 的 `operator[]` const 重载行为）。
- 全部「恒真/恒空」结论为**静态代数推导 + 数据流追踪**，按纪律未编译未运行；升级为运行期证据的路径已写入 §8。