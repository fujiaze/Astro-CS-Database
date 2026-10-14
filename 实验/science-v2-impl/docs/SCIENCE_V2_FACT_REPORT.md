# ACSD 科学 V2：代码实施与真实实验 — 事实报告

> 执行分支 `audit/acsd-science-docs-p5`。
> 基线 `942c978e875bf7673ebc30a89fc0b59738092e26` → 最终 `36bcaba8d`（已推送，未建 PR、未动 main）。
> 本报告只写**实际发生过的事**；未运行/未闭合项集中在 §7，不得当作已完成。

---

## 1. 提交清单（按依赖次序）

| SHA | 内容 |
|---|---|
| `04998d268` | **R0D** 统一诊断编译开关为唯一 `ACSD_DEV_MODE`，复用既有 `acsd/probe.h`，新增被动快照 sink |
| `43caa9109` | **R1** 移除非物理的跨滤镜准入（`coverage.cpp`），保留真实坐标一致性校验 |
| `6ac6055f6` | **R6/D02** 阶段二只发布单 signal 科学图像；**D07** 最终响应不取首帧滤镜 |
| `fe634fae7` | 真实数据索引、E2E 实跑记录、可视化交付 |
| `302f34f58` | **R3** 精度桥梁 `T=V_B/V_F` 的可证伪反例与阴性世界（7 条用例） |
| `36bcaba8d` | E2E 文档 §9 更新（R3 反例已跑，生产端到端未闭合） |

---

## 2. R0D：唯一编译开关 `ACSD_DEV_MODE`（已完成并实测）

### 2.1 CMake 统一

改前：`ACSD_PROBES`（独立 option）+ `ACSD_DEV_MODE` + 三个子项
（`ACSD_DEV_INTERMEDIATE_CACHE` / `ACSD_DEV_CHECKPOINT` / `ACSD_DEV_PROBES`）。
源码里 `ACSD_DEV_MODE` 只有 **1 处**命中，三个子项 **0 处**命中（纯死开关）。

改后（`CMakeLists.txt`）：

```cmake
option(ACSD_DEV_MODE "..." OFF)          # 唯一公开诊断编译开关
set(ACSD_PROBES ${ACSD_DEV_MODE})         # 派生，屏蔽缓存值
set(ACSD_DEV_PROBES ${ACSD_DEV_MODE})
set(ACSD_DEV_INTERMEDIATE_CACHE ${ACSD_DEV_MODE})
set(ACSD_DEV_CHECKPOINT OFF)              # 跳算/续跑不是被动诊断（DEV_MODE §3）
```

- `-DACSD_PROBES=ON` 单独设置**不能**再激活诊断（configure 期 STATUS 明示）。
- 新增 `acsd_dev_diagnostics` 目标：ON 时 STATIC（编入实现），OFF 时
  INTERFACE-only（**实现 TU 不进任何 target**）。
- 定义面收敛为一个 `ACSD_DEV_MODE`；旧三个子开关的 compile definition 全删。

### 2.2 复用而非重造

| 能力 | 实现 | 是否新造 |
|---|---|---|
| 细分计时 | `acsd/probe.h` 的 `ACSD_PROBE_SCOPE/COUNT/GAUGE` | **复用**（未新造计时库） |
| 快照落盘 | `aio_atomic::write_file_atomic` / `make_dirs` | **复用**（不经 aio 的路径为 0） |
| FITS 图像 | `aio_fits.cpp` 的 `fits_write_file` | **复用** |
| 失败分类 | 诊断 `IO_ERROR` 计数，不进科学错误模型 | 新（sink 本体） |

### 2.3 OFF/ON 符号差（机器可复跑）

```bash
nm -C build/linux-sci/acsd | grep -c devdiag     # 0   (OFF)
nm -C build/linux-dev/acsd  | grep -c devdiag    # 37  (ON)
strings build/linux-dev/acsd | grep -c "mosaic.integration.signal_tile"   # 1
strings build/linux-sci/acsd | grep -c "mosaic.integration.signal_tile"   # 0
ls build/linux-sci/libacsd_{probes,dev_diagnostics}.a    # 均不存在
```

### 2.4 ON/OFF 科学等价（逐位）

同一输入、同一配置、仅编译开关不同（2 帧 M42 Hα，串行、独立目录）：

| 项 | 数 |
|---|---|
| 公共文件 | 1518 |
| **逐字节相同** | **1496** |
| 科学 FITS tile 不同 | **0** |
| properties 不同 | 8（仅 `hips_creation_date` 一行） |
| 诊断/运行摘要不同 | 14（`alloc_report`、`alloc_samples.csv`、`graph/*`、run manifest） |

> 唯一"疑似分叉"的早期读数（8 帧 ON 报 `aio_hips_finalize` checksum 失败）
> 经追查是**我自己的流程失误**：同一命令并发启动两次、共用 output_dir。
> 串行重跑后 OFF/ON 均 exit=0 且科学产物逐位相同。这条留档说明：判定
> "ON/OFF 分叉"必须用串行 + 独立目录的对照实验。

### 2.5 开发快照真实样例

```
out/ab_snap.p1-work/_dev_diagnostics/aedf8b2582f1/
  debug_manifest.json                       (non_scientific_product: true)
  snapshots/normalize.noise.reference_snr/<frame>/{0,1}.json        (189/211 KB)
  snapshots/normalize.noise.reference_snr/<frame>/{0,1}.json.meta.json
  perf/events.jsonl  ← 由 ACSD_PROBES 产出 (6.5 KB)
```

meta 完整字段：probe_id / stage / producer / semantic / unit / coord_space /
dtype / shape / representation / roi / endianness / frame_id /
upstream_product / non_scientific_product。

**实测并修掉的两个自身缺陷**（详见 E2E_RUN §8.1）：
1. `DevDiagSessionGuard` 原先在 `run_with_resource_gate` **之前**析构 ⇒
   manifest 早于快照写出、`entries` 恒空。已提到函数作用域覆盖会话。
2. `declare_hips_surface_brightness_units` 形参语义混淆（D02 后 Phase2 只剩
   signal，却仍按 `uncertainty_available` 找 variance/ivar 的 properties）。
   形参改名 `variance_products_present`（"是否在磁盘上"），Phase1 true /
   Phase2 false。

---

## 3. R1：跨滤镜准入（代码已改，实跑验证未做）

- `lib/algorithms/coverage/src/coverage.cpp`：删除 `filter mismatch` 拒绝。
- 保留：`obs_filter` **键**存在性、跨帧坐标系一致性、NESTED、tile_width=512、
  hips_version。每帧真实通带身份仍登记进 `P2HipsInputInfo.filter_passband`。
- `coverage.h` 头注同步改写（兼容前提 ≠ 光谱准入）。

**未实跑跨滤镜合成**：本轮 E2E 两帧同带，走"透传"分支（与旧行为逐字相同）。
跨滤镜输入验证见 §7。

### D07：最终响应不冒充（已随 R6 提交）

`p2_op_write` 不再取 `frames[0].filter_passband`：
全组一致 ⇒ 透传真实身份；不一致 ⇒ 写 `mixed:<n>_passbands` 并 stderr 明示；
`p2_final.json` 增写 `obs_filter` + `response_distribution`。
本轮实测 `response_distribution = {"Baader 7nm H-alpha": 2}`（一致分支）。

---

## 4. R6/D02：阶段二只发布单 signal（已完成并逐位验证）

**改前**实测：`products = ["signal","support","variance","ivar"]`（4 个科学图像层）。
**改后**：`flags = AIO_HIPS_PRODUCT_SIGNAL`；variance tile 写调用与回读校验移除。

```text
out/mosaic_d02/  →  signal/            ← 唯一科学图像子产品
```

**signal 归一不变量**：`aio_hips_write_signal_support_tile` 里
`signal = flux_sum / covered_area`，`covered_area` 是 view 的**输入面**
（`cov_out` 继续填充），不是 support 子产品。

**逐位对照**（改动前后各跑一次，纯 signal 树）：

```text
common=188  identical=187  differing=1   （properties 仅 hips_creation_date）
```

⇒ **signal 数值逐位不变**。`uncertainty_available` 仍作为方差面完整性事实
写入 `p2_final.json`/provenance，不再触发额外图像产品。

---

## 5. R0 调用图与数据单位表：静态结论 vs 实跑判定

| 条目 | 静态结论 | 本轮实跑判定 |
|---|---|---|
| `ACSD_FRAME_SNR`/`ACSD_REFERENCE_FLUX` 是否写进 HiPS 头 | 键名未冻结 | **写出且可读**，数值与 `p1_snr.json` 的 `snr_f`/`flux_adu` 一致（14.042478467183946 逐位相同） |
| `ACSD_SPARSE_SNR_LAYER` 是否有生产者 | 未确认 | **无生产者** ⇒ `sparse_reconstruct` 生产不可达 |
| 阶段二实际权重路径 | 杂多 | **`per_sample_ivar`**；`snr_path_effective=not_used`（ivar 可用 ⇒ SNR 链不触发） |
| 阶段一"写盘再读回"是否改变科学来源 | 旧 DEV_MODE 曾记录 | 未发现分叉：唯一 dev-gated write 是 `p1_phot_stars_*.json`（无消费者、失败仅 stderr）；其余工作区文件是节点间**真实依赖载体**，OFF 时同样写同样读 |
| drizzle 面亮度归一 | 未证 | **正确**（§6.1） |

---

## 6. 真实 E2E 与数字测量

### 6.1 尺度自洽性核查（重要，一度误判又推翻）

导出值约 1e12–1e15 ADU/sr，一度被疑为单位错误。逐环节核对：

```text
calibrated 中位数      58.46 ADU/px
帧像素角尺度           0.940689″  ⇒  像素球面立体角 2.0799e-11 sr
B = 58.46/2.0799e-11 = 2.81e12 ADU/sr
HiPS signal 中位数    2.756e12 ADU/sr      （1.02 差 = median vs 覆盖加权均值）
```

⇒ `S_p = Σ_j a_jp B_j / Σ_j a_jp` 面亮度归一**正确**，`BUNIT=ADU/sr` 成立。
母版是 XISF normalized 面，`master_scale=65535` 只是 ADU 换算，**不是**绝对
测光标定 ⇒ 上述数值是仪器域，不可作绝对光度解释（与 D09/D17 一致）。

### 6.2 三阶段运行

| 阶段 | 命令 | 退出码 | 用时 |
|---|---|---|---|
| normalize | `acsd normalize -y --json p1_m42_t2_m1_ha.json`（8 帧 M42 Hα） | 0 | 11m40s |
| mosaic | `acsd mosaic -y --json mosaic.json`（2 帧） | 0 | ~3m |
| export | `acsd export -y --json export.json`（CRVAL 取自真实帧头） | 0 | ~1m |

### 6.3 数字测量（`run/A/preview/measurements.json`）

| 量 | 值 |
|---|---|
| 像素 / NaN / 负值 | 1,048,576 / **0** / 72 |
| median / p1 / p99 | 2.6565e12 / 1.975e12 / 3.613e12 ADU/sr |
| 最亮源 | 2.8235e15 ADU/sr @ (x=387, y=247) |
| robust σ | 3.595e11 ADU/sr |
| 5σ 等效 | 1.798e12 ADU/sr |

> 5σ 是**上界口径**：`snr_caliber = upper_bound_no_gain`（增益未知 ⇒ σ_F 只剩
> 天空项，SNR 系统性偏高）。不得当绝对深度。

### 6.4 可视化（`实验/science-v2-impl/run/A/preview/`）

- `pano_asinh.png` — 全景拉伸（**未** clamp 负值、未改 NaN 语义）
- `bright_star_cutout.png` — 最亮源 96px 半径 cutout
- `gradient_map.png` — 梯度幅值（线状结构 = 候选接缝；**是图，不是接缝指标**）
- `lsb_asinh_a{1,10,100}.png` — 低表面亮度三档 asinh 对照

---

## 7. R3 反例：本轮新增的科学证据

`eng/tests/synthetic/test_r3_surface_precision.py`（7 条：6 正例 + 1 负例）。

数学（全部解析可解，不 import 被测实现）：

```text
I_F,k = (1/σ²_B,k)·h_kᵀK_k⁻¹h_k ,  V_F,k = 1/I_F,k ,  S_ref = F_ref/√V_F,k
V_B,p,k = σ²_B,k·(K_k)_pp ,      T_p,k ≡ V_B,p,k/V_F,k
w_B,p,k = I_F,k / T_p,k = (S_ref/F_ref,k)² / T_p,k
Var(B̂|w) = Σw²V/(Σw)² ≥ 1/Σ(1/V_B) ≡ CRB    （取等 ⟺ w ∝ 1/V_B）
```

实测读数（可复算）：

| 判据 | 读数 |
|---|---|
| r3a 阴性世界（T 相同） | w_F 与 w_B 的合成相等（容 1e-14 二进制残差） |
| r3b 阳性反例（S_ref 同、T 不同） | 合成差中位数 > 1e-12，权重比不同 |
| **r3c w_F 方差** | **Var/CRB − 1 = +0.789**（79% 超出） |
| r3c 等权（T≡1）方差 | 同上 +0.789 |
| **r3d w_B 方差** | **Var/CRB − 1 = +0.0188**（达到 CRB） |
| r3e T 的量纲 | T(K=I)=**0.5**，T(K=ρ=0.7)=**0.200087**（不是无量纲 1） |
| r3f 负例（注入 T≡1） | 超出 **+3.14** ⇒ 判据有判别力 |
| r3g 三种逆方差 | I_F=0.25，w_B=0.5，w_ctrl=65.006（两两不等） |

⇒ **"点源逆方差当面亮度最优权重"在本构造世界被证伪**：相同 `S_ref` 下，
`w_F` 使合成方差比 CRB 高 79%，而 `w_B = I_F/T` 达到 CRB。

新增 3 条冻结容差（`synth.r3.wb_crb_rel` / `suboptimal_excess_min` /
`blend_diff_min_abs`），每条带值、量级域与科学推导来源。

建成过程中被判据抓出并修正的 3 处**自身**缺陷（honest record）：
1. r3a 用 `exact` 判 T 相等 —— `1/√2` 平方不能精确回 0.5（差 1.1e-16）。
2. r3e 判"w_B 比==T 比" —— 换 K 时 `I_F` 与 `T` **都**变，比值商不成立。
3. r3g 对 4 个样本取中位数却套 `N=200` 的 `Var(median)=(π/2)σ²/N`（比值 37.5）。
三条都是判据发现我的公式误用，**不是正本错误**。

全量 synthetic 层回归：**78 通过 / 0 红**（此前 71，新增 7，无回归）。

---

## 8. 未运行 / 未闭合（**不得声称已完成**）

| 项 | 状态 |
|---|---|
| R3 生产实现 | **未改**。`weight_chain.h` 仍是 `w=(SNR/F_ref)²·g²`。反例检验的是公式**结构**，生产 mosaic 仍走 `per_sample_ivar`，未消费 T |
| R2 稀疏控制点闭环 | **未闭合**。帧级 SNR 已证可用；规整 sparse layer 无生产者 |
| R4 曲边 oracle / 混合分辨率 target_order | **未跑**。`target_order=min(...)` 未改 |
| R5 连通分量规范/gauge 实测 | **未跑** |
| 跨滤镜合成实跑 | **未跑**（D01 代码已改，无跨滤镜输入验证） |
| HST 结构前向模拟 | **未跑** |
| E1–E8 实验矩阵 | **均未跑** |
| 三阶段 OFF/ON 全链对照 | 只做到 normalize 逐位对照；mosaic/export 各只跑 ON 一侧 |
| 旧 `ACSD_PROBE_LOG` / `ACSD_NODE_TRACE` 等 env 旁路 | CMake 侧已统一；`ACSD_PROBE_LOG` 在 ON 构建仍是 perf sink 路径源（属"开发运行选择器"范畴；OFF 构建不编译 probe.cpp，已由符号/字符串扫描证明） |
| `ACSD_*_FAULT` 故障注入迁入测试构建 | **未做**（生产默认不可达，但未迁移） |

---

## 9. 交付物位置

| 类别 | 路径 |
|---|---|
| 单元文档 | `实验/science-v2-impl/docs/R0_baseline.md`、`docs/E2E_RUN.md` |
| 复现脚本 | `实验/science-v2-impl/code/{build_input_index,wcs_census,compare_onoff,make_previews}.py` |
| 数据索引 | `实验/science-v2-impl/run/S00/{input_index,wcs_census}.json` |
| ON/OFF 对照 | `实验/science-v2-impl/run/A/onoff_compare.json` |
| 预览图与测量 | `实验/science-v2-impl/run/A/preview/*.png`、`measurements.json` |
| 最终 FITS | `run/science-v2-impl/e2e/out/export/output_phase3.fits`（1024×1024，`BUNIT=ADU/sr`，工作树内，不入 Git） |
| mosaic HiPS | `run/science-v2-impl/e2e/out/mosaic_d02/signal/`（单 signal） |
| R3 反例 | `eng/tests/synthetic/test_r3_surface_precision.py` |

复跑命令清单见 `docs/E2E_RUN.md` §10。

---

## 10. 需要前台裁决的事项

1. **R3 生产权重改造范围**：反例已证 `w_F` 次优（+79% 方差），但把 T 桥梁接进
   `weight_chain.cpp` / `p2_op_integrate` 会改变**生产科学输出**。本轮刻意未动
   ——按任务包"如有独立反例证明模型错误，提交反例与证据，由前台调整后再实施"。
   请裁决是否在下一轮接入，以及 `T` 的默认退化策略（对角/白噪近似 + 近似状态标注）。
2. **`snr_caliber=upper_bound_no_gain` 的处置**：真实数据缺增益时 SNR 只能是
   上界。要不要在 config 面增加 gain 输入，或在产品 metadata 强制标注上界口径。
3. **M42 T2T3 的 30 帧缺几何**：已按 §13.1 登记为待查、未进入分析。是否需要
   上游重新 plate-solve 或从母带恢复。
4. **`ACSD_*_FAULT` 迁移**：建议在下一轮迁入专用测试构建。
