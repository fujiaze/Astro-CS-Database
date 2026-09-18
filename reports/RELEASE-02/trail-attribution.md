# RELEASE-02 卫星线残留归因（TRAIL 分片）

- 任务：解释 M1 板块残余卫星线为何只被「部分」剔除
- 对象：`run/RELEASE-02/L4-rebuild/mosaic_out_w1/`（`weight_mode=1` 等权，`profile=astrocs_adaptive_pixel`）
- 判定线（p3 渲染，TAN 2.573″/px）：x≈1980→3380、y≈960→435（斜率 −0.375，长≈1400px）
- 本轮只归因，未改任何生产代码/文档；未跑 ninja/cmake/ctest；零 git 写权限

---

## 0. 结论（TL;DR）

**这是真缺陷，不是采样稀释的假缺陷。**

| 问题 | 结论 |
|---|---|
| 几何 n | **n = 8**（M1 全线；远端 M1/M2 重叠处 n=17）。**不是低 n**，SD-18 的 n≤3「本就不排异」档未触发 |
| 资格掩膜 | eligible_count **= geom_n = 8**（全线无损失）；`candidates` 平面与 `eligible` 完全一致 |
| kernel 能否拒绝该样本 | **能，也不是接线问题**：把该处真实 8 样本喂给生产 kernel（`p2_reject_plan_resolve_n(8)`+`p2_reject_stack_ex`），其逐像素 nrej 与生产 `p2_integrated_nrej.bin` **12599/12599 完全一致**，源样本拒绝数 8143 与生产 sample_mask **逐位一致**。问题是 kernel **只在 69.6% 的拖线像素拒绝源样本**——这是算法能力/输入质量问题 |
| 欠采样稀释 | **不成立**。p3 采样器是 HiPS 叶格上的**双线性点采样**（`p3_resample.cpp:315-406`），不是面积积分，不存在 2.66× 稀释；实测 p3 线峰值 +4.75% ≈ 叶级马赛克 +6.0%（稀释因子≈0.8）。**反事实（源样本永远拒绝）**下 p3 峰值仅 **+0.6%**（线心 ~0.1–0.25%）⇒ 若排异完全成功，残留应 <1%，与实测 4–5% 差 ~10× |
| 等权 vs 逆方差 | 排异**根本不使用权重**（`module_adapters.cpp:5353/5376` 传 `weights=nullptr`；`reject_winsorized_impl` 无权重形参）⇒ `weight_mode=2` **不可能改变排异判定** |

**根因（两层叠加）**

1. **主因（近因，主导）**：M1 八帧的天光/背景水平相差最高 **1.56×**（标定帧中位数 160.7–250.0 ADU；帧间频段中位数比 0.821–1.312），而 Phase2 排异前**没有任何逐帧背景/光度归一**（`sky_plane_applied=false`；UPM `calibrate` 在拖线瓦片上**逐位恒等**，差值 0.0）。等权叠加下逐像素样本散差被抬到 ~10.9%（MAD），使 73% 背景高度的拖线在 8 样本栈里只有 ~5σ 的有效显著性，**~30% 的拖线像素 working z < 3 → 不拒绝**。把八帧做逐帧归一后，源样本拒绝率 **0.696 → 0.986**。
2. **次因（算法能力）**：冻结的 n=8 winsorized_sigma 档本身小样本离群能力弱（单一离群需 ≳6–8σ 才可靠拒绝；2 个离群 5σ 仅 21%；3 个离群几乎 0%），且设计好的大尺度拖线掩膜生长 `p2_large_scale_apply` **在生产调度路径中根本没接线**（只在 `stage2.cpp:1744` 与测试里调用；plan 默认 `large_scale.enabled=0`）。

---

## 1. 关键工具：Phase2 bin 真实布局（VISUAL 未能读出的原因）

VISUAL 用 astropy_healpix 直接采样 `p2_integrated_*.bin` 失败（6 种布局相关≈0），因为 bin 的像素序**既不是纯 row-major 也不是纯 NESTED local**，而是「FITS row-major ↔ NESTED local」的复合映射，且 tile 块按 manifest 顺序拼接。

从代码确定（权威）：

- `module_adapters.cpp:6110-6114`：`tiles[i].offset = i*tile_span`，每 tile 一块 `tile_span=262144`；
- `module_adapters.cpp:6279-6299`：bin 内索引 = **标准 HiPS FITS row-major**；写 HiPS 时 `local = fits_index_to_nested_local(i,9,512)`；
- `lib/algorithms/shared/healpix/healpix_core.cpp:288-303`：
  `row=i//512; col=i%512; x=511-row; y=col; local=xy_to_nest(x,y,9)`（x 偶数位、y 奇数位交错）；
- 叶 ipix@nside 2^18 = `tile_ipix * 2^18 + local`（`child_nest(ipix,9)=ipix<<18`）；`tile_ipix` 是 order-9（nside 512）NESTED ipix。

已实现于 `run/RELEASE-02/trail/layout.py`，并**交叉校验**：

- 与写出的 HiPS FITS 瓦片逐元素比对：`max |bin−FITS|/|FITS| = 1.4e-7`（float32 舍入），NaN 图样完全一致（`validate_layout.log`）；
- p3 WCS（TAN，CRVAL 83.747318/−5.361391，CD 7.1462e-4）→ sky → `lonlat_to_healpix(nside=2^18, nested)` → (tile, fits_index)，与 p3 实测线位置吻合。

> 注：HiPS FITS 瓦片自带的 WCS 是 HiPS 占位 WCS（all_pix2world 恒返回 256.5/256.5），不可用于定位；必须用上面的 NESTED 映射。

---

## 2. 逐假设证据

### H1 几何 n 太低 —— **否定**

从 `p2_rejection.json`（`geometric_n_source=frame_support_gt0`、`low_n_max_n=3`、`low_n_policy=underdetermined_no_rejection`、`plans[]` 逐 n 明细）与 `p2_integrated.json`：

- 拖线频段（p3 线 ±4px，125,302 叶像素，12 个 tile）几何 n 分布：
  `{8: 109109, 9: 241, 10: 83, 11: 21, 12: 105, 13: 83, 14: 442, 15: 349, 16: 281, 17: 14588}`
- **M1 主体（109,109 像素）= n=8**；远端进入 M1/M2 重叠 = n=17。**没有 n<8 的拖线像素**。
- 路由正确：`plans[8]` = `astrocs.winsorized_sigma_siril_1_4_3.v1`（method=2, minimum_n=3, underdetermined_n=3）；`plans[17]` = `linear_fit`（method=4）。kernel 实测 method 分布 `{2: 11117, 4: 1482}`，与之一致。
- 全局 `stats.underdetermined_pixels = 1,275,512 / 137,101,312 = 0.93%`（低 n 边缘），**拖线像素一个都不在其中**（拖线处 `accepted` 平面恒 1，`candidates=8`）。

### H2 资格掩膜压低 eligible_count —— **否定**

- `eligible` 分布与 `geom_n` 分布**逐值相同**（8:109109 … 17:14588），即 `support>0` 的样本其 corrected 值全部 finite；
- `candidates` 平面（=eligible_count）分布与之一致；
- 生产 reject 路径的资格 gather 传 `valid/support/quality=nullptr`（`module_adapters.cpp:5342-5359`），只用 finite 过滤，不引入星点掩膜。

### H3 线宽 vs 排异尺度：kernel 实际能否拒绝？—— **能拒绝（无接线问题），但小样本能力弱**

**工具**：`run/RELEASE-02/trail/kernel_probe`（直接链接既有构建产物
`build/lib/algorithms/coverage/CMakeFiles/phase2.dir/src/rejection.cpp.o`，调用生产
`p2_reject_plan_resolve_n` + `p2_reject_stack_ex`，profile=`astrocs_adaptive_pixel`、
request=AUTO，与 `module_adapters.cpp:5131-5155` 的 plan_cache 同源）。

**(a) 接线检验（生产 = kernel）**
把拖线核心 12,599 个叶像素的真实 8 样本（来自 `p2_corrected_*.bin`，与生产逐位同源）喂给 kernel：

- kernel `nrej` == 生产 `p2_integrated_nrej`：**12599/12599 完全一致**；
- kernel 判定「源样本被拒」== 生产 `sample_mask` 源 slot：**8143 == 8143**。

⇒ **不存在接线/门限/映射问题**，生产跑的就是 kernel 的结论。

**(b) 合成能力曲线（n=8，背景 σ=1，200 次）**

| 注入离群数 | 3σ | 4σ | 5σ | 6σ | 8σ | 12σ |
|---|---|---|---|---|---|---|
| 1 个 | 26% | 44% | 67% | 87% | 99.5% | 100% |
| 2 个 | 9% | 12% | 21% | 33% | 64% | 95% |
| 3 个 | 0.5% | 0% | 0% | 0% | 0% | 0% |

⇒ n=8 winsorized_sigma（Siril 1.4.3 语义）**对小样本离群的可靠阈值约 6–8σ**，不是名义 3σ；多离群时几乎失效。

**(c) 真实栈**

- 核心（对非源 7 帧 MAD 的 ref-z>5，n=10,197）：源样本拒绝率 **0.696**；
- 有效 working-domain z（Python 精确复刻 `reject_winsorized_impl`，与 kernel **0/3000 失配**）：中位 **5.56**（p10=2.09，p90=10.74）；**working z<3 的 3,096 个像素（30.4%）一律不拒绝**；
- 按「栈内 robust 高样本数 nhigh」分层拒绝率：nhigh=1 **95.7%**、nhigh=2 **65.2%**、nhigh=3 **19.3%**、nhigh=4 **0%**。

⇒ 失败发生在栈内出现第 2、3 个抬升样本时，winsorized 尺度被抬高、源样本 working z 掉到阈值下。

### H4 欠采样稀释（关键：是否假缺陷）—— **否定；这是真残留**

**首先纠正前提**：p3 渲染不是面积平均，而是 **HiPS 叶格上的双线性点采样**
（`p3_resample.cpp:315-406`：取 4 象限最近叶心，权重和为 1）。所以「3px 线在 2.573″/px 上被面积稀释 2.66×」不成立——点采样只做 ≤2 叶像素的双线性平滑。

**实测（同一 p3 对齐坐标，中位剖面）**

| 量 | 峰值相对本地背景 |
|---|---|
| p3 实际输出线 | **+4.75%**（VISUAL 报 4.05%，口径不同） |
| 叶级生产马赛克 | **+6.00%** |
| 叶级「完全不排异」（8 帧平均） | **+9.24%** |
| 叶级「源样本永远拒绝」（其余 7 帧平均） | **≈0**（线心 +0.0~0.2%） |
| 合成 p3：生产 | +5.67% |
| 合成 p3：完全不排异 | +9.24% |
| **合成 p3：源永远拒绝** | **+0.62%**（线心 0.1–0.25%） |

p3 实际（4.75%）与叶级生产（6.0%）同量级（比≈0.8），说明 p3 管线几乎不额外稀释；
而「排异完全成功」的反事实只有 **0.2–0.6%**，比实测低 ~10×。

⇒ **若排异成功，p3 上应只剩 <1% 的残影；实测 4–5% 是真实未被剔除的源帧流量，不是采样稀释假缺陷。**
（单帧 73% 背景的拖线被 8 帧平均、再在 p3 上点采样，解释了「为何只有几个百分点」。）

### H5 等权 vs 逆方差 —— **对排异无影响（否定其作为修复手段）**

- 排异 kernel **不接受权重**：`module_adapters.cpp:5353`（`gout.weights=nullptr`）、`:5376`（`stack.weights=nullptr`）；`reject_winsorized_impl`（`rejection.cpp:1441`）签名里没有 weights。权重只在 `p2_integrate_pixel` 的最终合成里生效。
- 因此 `weight_mode=2` **不会改变任何拒绝判定**；它至多改变合成权重。且源样本是**正离群（拖线），不是噪声尖峰**，逆方差权重并不针对该机制。

---

## 3. 根因链（证据闭合）

1. **八帧天光/背景不一致**：标定帧中位数 160.7 / 174.8 / 176.2 / 181.2 / 190.5 / 196.0 / 204.2 / 250.0 ADU（**1.56×**）；在拖线频段逐帧中位数比 0.821–1.312，帧间散差 **13.5%**。
2. **排异前无逐帧归一**：`p2_corrected.json` `sky_plane_applied=false`；UPM `p2_upm_calibrate_block` 在 tile 1371889 上对 8 帧 corrected−Phase1 = **0.000e+00（逐位恒等）**；`p2_upm_model.json` `component_count=1`。任何 mosaic 配置（`mosaic_49.json` / `mosaic_49_w1.json`）都**没有 `sky_plane` 键**。
3. **散差被抬高 → 有效 z 变低**：拖线核心处「非源 7 样本」散差中位 MAD **10.9%**、std **15.2%**；拖线高度 **+73% 背景** ⇒ 仅 ~5σ。做逐帧归一后散差降到 MAD **4.5%**。
4. **kernel 阈值未达 → 不拒绝**：working z 中位 5.56，30% 像素 <3 ⇒ 不拒绝（winsorized 名义阈值 3σ）。
5. **决定性反事实**：逐帧归一（加性或乘性）后再跑同一 kernel，源样本拒绝率 **0.696 → 0.985/0.986**。
6. **残留落地**：~30% 拖线像素保留源帧 1/8 权重 → 叶级马赛克线 +6.0% → p3 +4.75%。
7. **反例自洽**：`...20251211_013548` 的 268% 背景拖线被完全剔除——因为其幅度远超 ~6σ 阈值；本线只有 73%，落在阈值边缘。

---

## 4. 真缺陷 or 假缺陷？

**真缺陷。** 直接证据：

- 不是低 n（n=8）、不是资格掩膜（eligible=8）、不是接线（kernel==生产 12599/12599）；
- 不是采样稀释：源永远拒绝的反事实下 p3 残留 0.2–0.6%，比实测低一个量级；
- 逐帧背景归一可把源拒绝率从 70% 抬到 98.6%，说明**排异失败由输入栈的光度不均匀主导**；
- 设计好的大尺度拖线掩膜生长（`p2_large_scale_apply`，专门处理「trail 扩张 / compact cosmic 不生长」）在生产路径**未接线**、默认关闭。

**残留的诚实边界**：即便逐帧归一，仍有 ~1.4% 核心像素不拒绝（winsorized n=8 的固有小样本弱点），单靠归一不能到 100%。

---

## 5. 最小修复方向与预期

> 以下均为**方向**，涉及冻结科学默认值的需走 `ENGINEERING_SPEC §3` 变更 claim；本轮未改任何生产代码。

1. **排异前做逐帧天光/背景归一（首选，治本）**
   - 现状：FIX-A 的 `p2_sky_plane_*` 已在 mosaic 生产路径接线（`module_adapters.cpp:4621-4698`），但本次运行**无 control observations → fallback**，且没有任何 mosaic 配置提供 `sky_plane`；
   - 动作：在 `mosaic_49*.json` 提供 sky_plane 配置/control 样本（或确认 UPM 的加性背景项应生效），使 `sky_plane_applied=true`；
   - 预期：逐像素样本散差 10.9% → ~4.5%，源样本拒绝率 **0.696 → ~0.99**，p3 残留 **4–5% → <1%**。
2. **接线并启用大尺度拖线掩膜生长（次选，补漏）**
   - 现状：`p2_large_scale_apply` 仅被 `stage2.cpp:1744` 与测试调用，**生产调度路径无调用**；`plan.large_scale.enabled=0`（`rejection.cpp:1180`）；
   - 动作：在 reject op 内对每帧 512×512 的 low/high mask 调用 `p2_large_scale_apply`，并启用 `large_scale`（min_structure=8, radius=2）；
   - 预期：把已连通的拖线拒绝掩膜向外生长、补上 per-pixel 漏检的缺口，残留向 0.2–0.6% 噪声底收敛；
   - 风险：需处理跨 tile 边界的连通性；属科学默认变更，需 claim + 回归。
3. **不建议**把 `weight_mode=2` 当作本缺陷的修复（排异无权重，见 H5）。

---

## 6. 证据与脚本索引（`run/RELEASE-02/trail/`）

| 文件 | 内容 |
|---|---|
| `layout.py` | 由代码确定的 bin 布局实现（fits↔nested-local、tile 拼接、各平面 dtype） |
| `validate_layout.py` / `logs/validate_layout.log` | bin ↔ HiPS FITS 逐元素校验（1.4e-7，NaN 同构） |
| `extract_band.py` | p3 线 → 叶格 → 提取全 slot corrected/support/sample_mask/马赛克平面 → `trail_band.npz` |
| `analyze_trail.py` / `logs/analyze_trail.log` | 几何 n、eligible、kernel vs 生产逐像素比对 |
| `kernel_probe.cpp` / `kernel_probe` | 链接既有构建产物的生产 kernel 驱动（单栈 + stdin 批） |
| `mechanism.py` / `logs/mechanism.log` | 合成能力曲线 + 真实栈 nhigh 分层 |
| `winsor_replica.py` / `logs/winsor_replica.log` | winsorized 精确复刻（0/3000 失配）+ 有效 working z |
| `profile_analysis.py` / `logs/profile_analysis.log` | p3 剖面、叶级剖面、反事实剖面 |
| `synth_p3.py` / `logs/synth_p3.log` | 反事实合成 p3：生产 / 不排异 / 源永远拒绝 |
| `frame_scale.py` / `logs/frame_scale.log` | 帧间背景失配 + 归一后拒绝率跃升 |
| `final_numbers.py` / `logs/final_numbers.log` | 栈散差、帧偏移、UPM 恒等性 |
| `perframe_profile.py`, `verify_frames.py`, `find_case.py`, `diag_pixels.py`, `probe_trail.py` | 逐帧线幅、源帧锁定、个案栈、早期探针 |
| `trail_band.npz`, `trail_core_analysis.npz`, `trail_probe.json` | 中间数据 |

环境：`TMPDIR=/dev/shm/astrocs_trail`（用后为 0）；未改生产代码；未跑 ninja/cmake/ctest。
