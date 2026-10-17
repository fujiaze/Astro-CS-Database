# Issue #12 报告：严格等距 Simpson 数值合同 + 真实 Gaia XPSD 回归

- Issue: [#12](https://github.com/fujiaze/Astro-CS-Database/issues/12)（执行任务书评论 `6227953089`）
- 验证/报告 main: `f1082869562bedfb6ab9d81b632bc99e39e48de2`（执行前 `git fetch` 确认；本轮改动未提交前的冻结基线）
- 冻结数学合同: `docs/science/algorithms/normalize/PHOTOMETRIC_FIT.md` §「数值积分唯一有效域、容差与解析检验」（未修改该文档）
- 生产改动: `lib/algorithms/photometry/cpp/src/spectrum_integrator.cpp`（`simpson_integrate` +
  `prepare_filter_cache` + `compute_f_syn_cached_xpsd` 的无效输入传播）与 `spectrum_integrator.h` 契约注释。

## 1. 实施内容（按冻结定义逐条）

| 合同条款 | 实现 |
| --- | --- |
| `N=x.size()==y.size()>=2`、全 finite、严格递增 | `spectral_grid_equidistant_nm` 前置检查；`y.size()!=x.size()`/NaN/Inf/重复/倒序 → 拒绝 |
| `n=N-1`、`h=(x_{N-1}-x_0)/n>0` finite | 计算后判定，非法即拒绝 |
| `M=max(|x_0|,|x_{N-1}|,h)`、`eps=2^-52` | `std::numeric_limits<double>::epsilon()` |
| 坐标分辨率前提 `16·eps·M <= 1e-8·h` | 不满足即具名拒绝（大 offset 微小 h 不可辨） |
| `tau=min(1e-8·h, 16·eps·M)`，逐间隔 `|Δx_i−h|<=tau` | 检查全部 n 个间隔（非仅首尾）；不含 `max(1,|h|)` |
| 非法输入返回 quiet NaN（非 0） | `simpson_integrate` 全部失败路径返回 `quiet_NaN()`；合法网格恒零返回真 `0.0` |
| 分段：`n=1` 梯形；n 偶全 1/3；n 奇≥3 前 n−3 用 1/3、尾 3 用 3/8；`n=3` 前段 0 | 保留并显式 `isfinite(result)` 终检 |
| cache 边界先验 nm 轴再插值 | `prepare_filter_cache` 先验 `spectrum_wl`，非法 ⇒ 空缓存 + `INVALID_SPECTRAL_GRID`（防止 Akima 在非法轴上先二分） |
| API 边界具名失败 | `compute_f_syn_cached_xpsd` 空指针/尺寸失配/非法量化参数/非有限求积 ⇒ quiet NaN + `INVALID_INTEGRAND`/`INVALID_SPECTRAL_GRID` |

未换用非等距 quadrature、未重采样、未改 Gaia 模型/光度拟合/其它 Issue。

## 2. 独立解析 oracle（`eng/tests/unit/simpson_integrator_domain_test.cpp`，62 判据）

oracle = 解析原函数 `∫_a^b x^k dx=(b^{k+1}-a^{k+1})/(k+1)`；不复用旧积分核。

- **post（本轮修复）：62/62 PASS**（`evidence/30-post12-oracle.log`）
- **pre（冻结 main 源码）：14/62 FAIL**（`evidence/31-pre12-oracle.log`），关键：
  - `x=[0,1e-12,3e-12], y=x`：pre **接受**并返回 `3.5e-24`，解析真值 `4.5e-24`，**相对误差 −2/9 = −22.22%**；post **拒绝**（quiet NaN）。
  - `x0=500nm,h=1e-4`：pre 接受（旧阈值 `1e-8·max(1,|h|)`），post 以坐标分辨率前提拒绝。
  - `N<2`/y 长度不等/溢出/非等距 cache 入口/flux 量化非法：pre 返回 `0`（伪装合法零），post 返回 NaN。
- 正例：`N=2/3/4/5/6/343`，`y=1/x/x²/x³`，在法律网格上 `rel_err <= 1e-9`；
  `N=2` 梯形只断言等于梯形定义式（对 k≥2 不冒充解析精确，k=2 rel_err=1.5e-3、k=3 4.5e-3）。
- 多尺度：336..1020(h=2)、500(h=1e-3)、0(h=1e-3)、500(h=0.5)；合法零函数返回精确 0；带符号常数正确。
- cache/API 边界：`T≡0 ⇒ F_syn==0`（合法零保留）；`flux_mul=0`/`flux_min=NaN`/`spectrum_count` 失配/空指针 ⇒ NaN。

## 3. 生产构建

`ninja -C build/linux-sci-p5`（default all）：**exit 0**（`build/01-build.log`）。
A/B 二进制（`code/build_ab_pre12.sh`，临时替换源码后恢复）：
post `d9d0510d…`、pre `ec8d9dca…`（`evidence/00-binaries.txt`）。

## 4. 真实 Gaia XPSD 回归（**非合成**）

真实 `gaia/GaiaDR3SP`（64G XPSD）+ 真实响应曲线 `config/response_curves/Baader_7nm_H_alpha.json`
与 `Ideal_QE_curve.json`；M42 天区锥形搜索。harness 只调用生产核
`prepare_filter_cache → compute_f_syn_cached_xpsd`（`code/gaia_fsyn_regression.cpp`）。

**真实网格元数据（从产品读取，非硬编码）**：`N=343, x0=336, x_last=1020, step=2nm, h=2,
min/max Δx=2, max_i|Δx_i−h|=0, tau=3.624e-12, 16·eps·M=3.624e-12, 1e-8·h=2e-8` ⇒ 合法等距。

| 量 | pre-#12 | post-#12 | 差异 |
| --- | --- | --- | --- |
| 星数 | 1906 | 1906 | — |
| `F_syn` | 逐星 | 逐星 | **0 格不同（bit 相同）** |
| 合成零点 `zp_syn = magG + 2.5·log10(F_syn)` | 逐星 | 逐星 | **0 格不同**；median = −17.6636 mag（原始合成量，非官方绝对 ZP） |
| 独立光子率 `Q_γ = F_syn·1e-9/(hc)` | 逐星 | 逐星 | **0 格不同** |

（`evidence/40-fsyn-post.csv`, `41-fsyn-pre.csv`, `42-fsyn-ab-compare.json`。）
单位独立核对：`Q_γ` 与**本程序内独立梯形**求积（不复用 `simpson_integrate`）相对差
max 9.94e-3、median 3.27e-3，符合真实 XPSD 的求积离散量级，未见单位错误。
既有实验 `实验/normalize-precision-p5/code/verify_round9_fsyn_integral.cpp`（含 SI 光子率与
`round-trip` 判据）对修复后源码仍 **8/8 PASS**（`evidence/45-...log`）。

## 5. 应用标度（光度定标路径）：**NOT_RUN**（如实，非绿）

用真实 M42 T3 M1 Red 六帧 + `photometry.fit.enabled=true`（配置复制自既有
`run/PHOT-FIX/a3_curves/arm1_config.json`）跑真实 `acsd normalize`：

| tag | exit | `gaia_projected_in_frame` | `n_matched` | `p1_phot.json.photscal` | `n_frames_applied` |
| --- | --- | --- | --- | --- | --- |
| pre-#12 | 4 | 0 / 2309 | 0 | 1.0 | 0 / 6 |
| post-#12 | 4 | 0 / 2309 | 0 | 1.0 | 0 / 6 |

pre 与 post **逐项相同**；失败发生在 `star_matcher` 的 Gaia 逆投影阶段（在 F_syn 计算之前），
报文 `PHOT_FIT_NO_SCALE`（`n_matched=0 < 3`，exit 4）。**该失败与 #12 无关**（损坏点在积分核
之外，且 pre 二进制同样复现）。因当前 main 无法对真实帧产出拟合标度，**“应用标度 pre/post
对拍”子项记 NOT_RUN**，并作为未闭合问题上报（§7）。证据：`evidence/50-photscale-notrun.json`、
`50-norm6-{pre,post}.keylines.txt`（大日志已裁成关键行）、`50-p1_phot-{pre,post}.json`、`50-cfg6.json`。

## 6. PASS / FAIL / NOT_RUN

| 项 | 状态 |
| --- | --- |
| 冻结合同静态实现（tau/分辨率前提/逐间隔/NaN 口径/具名失败） | **PASS** |
| 独立解析 oracle 正负例（含 22.22% 反例） | **PASS**（post 62/62；pre 复现 14 FAIL） |
| 生产目标编译 | **PASS**（exit 0） |
| 真实 Gaia XPSD × 真实滤镜/QE 的 F_syn | **PASS**（1906 星，pre≡post） |
| 合成零点（逐星，未做官方绝对 ZP 定标） | **PASS**（pre≡post） |
| 独立光子率单位 | **PASS**（SI 换算 vs 独立梯形 max 1.0%） |
| 既有 F_syn 实验回归 | **PASS**（8/8） |
| 应用标度（`p1_phot.json`）pre/post 对拍 | **NOT_RUN**（§5，与 #12 无关的既有失败） |
| 非等距网格调用方全量排查 | 生产仅 4 处 `prepare_filter_cache` 调用，均以 `wl_start+i*wl_step` 生成合法轴 |

## 7. 未闭合问题（只上报，未修）

当前 main 上，对真实 M42 T3 M1 Red 帧启用 `photometry.fit.enabled=true` 时，
`star_matcher` 的 `gaia_projected_in_frame=0`（2309 颗 Gaia 星无一落入 4096×4096 帧内），
导致 `PHOT_FIT_NO_SCALE`、CLI exit 4；`p1_wcs.json` 已解出（`n_pairs=54, rms_arcsec=0.127`），
pre/post 二进制均复现。该现象与 #12 的积分核无关，需另行定位（候选：光度节点读取的 WCS/SIP
与求解产物或投影约定不一致）。**最短复现**：`50-cfg6.json` + `acsd normalize`。

## 8. 复跑命令

```bash
ninja -C build/linux-sci-p5                                   # 生产构建
python3 eng/tests/unit/run_simpson_integrator_domain_test.py  # 62 判据解析 oracle
bash 实验/issue12-simpson-domain/code/build_ab_pre12.sh       # pre/post A/B 二进制
python3 实验/issue12-simpson-domain/code/run_gaia_fsyn_regression.py \
    --tag post --impl lib/algorithms/photometry/cpp/src/spectrum_integrator.cpp \
    --out /workspace/issue12-scratch
python3 实验/issue12-simpson-domain/code/compare_fsyn_csv.py <pre.csv> <post.csv>
python3 实验/issue12-simpson-domain/code/run_photscale_ab.py --pre ... --post ...   # §5
```

## 9. 交付边界

仅改生产积分核（`spectrum_integrator.{cpp,h}`）+ 新增 `eng/tests/unit/**`、`实验/issue12-simpson-domain/**`；
未改 `docs/**`（Codex 维护）、未动其它算法/Issue。#12 内的科学结论以本合同为唯一依据。
