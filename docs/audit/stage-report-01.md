# 阶段报告 01｜A0–B1 与第一轮独立审查（2026-11-10）

> 分支 `rebuild/acsd-v2`；远端已推送至 `1139f7ec7`。本报告只汇总实际完成、实际证据与尚未闭合的问题；不把设计存在当通过。

## 1. 提交与基线

| SHA | 内容 |
|---|---|
| `27bd7925b` → `e2df158bb` | 拉取远端 4 个任务书提交后的基线 |
| `fa4649d57` | B1 共享数值原语修复（interval/linalg + oracle） |
| `688adeac7` | A1 文献卡 10 篇、A2 R1–R5 审计、foundation 引用/前提修正 |
| `ca7de6d4d` | A0 基线审查 + X01 新证据 + 独立测光不确定度脚本 |
| `ef91befb7` | workflows：apply_sky、几何状态合并、产品图闭包校验、export Q C Qᵀ；X01 验收命题修订 |
| `1139f7ec7` | product：不再宣称 reader 无法重开的闭包 |

基线构建：`cmake --build build/science -j16` 在 `fa4649d57` 通过（期间删除一个陈旧的 `build/science/acsd -> /tmp/acsd-build/acsd` 符号链接）。

## 2. 已完成的数学/工程工作

### B1 数值原语（`fa4649d57`）

- 四则/sqrt 的表达式真正在 `FE_DOWNWARD/FE_UPWARD` 内求值；平台不支持时降级为 1 ulp 外扩并具名。
- sin/cos 全相位极值、pow=exp(Y log X) 与零/负底域、atan2 原点与负实轴闭包。
- SVD 的正交缺陷/重建残差/谱误差界向外；秩阈值双侧区间；零奇异列正交补全。
- 独立精确有理 oracle：20000 例 0 失败；正弦/幂/atan2/负底反例通过。
- 未用 MPFR（本机无开发头）；超越函数仍是条件性 long-double 保守界，已在 NUMERICS 正本降级陈述。

### A1 文献（`688adeac7`）

- `docs/references/papers/` 10 张卡：WCS I/II、SIP、Fruchter–Hook、HPX/HEALPix、Gaia XP、Montage、Zackay–Ofek（仅对照）、定向舍入/线代、BLUE/GLS/NNLS、CCD 噪声；未读项如实标注。
- 修正：SPHERICAL_DRIZZLE 把 arXiv 0412607 标成 2007 出版版；LONPOLE 折进 CD 只对天顶投影成立；NUMERICS 未写 IEEE 754 不保证超越函数正确舍入。

### A2 R1–R5（`688adeac7`、`docs/audit/r1-r5-research-audit.md`）

- 历史档案 `adc7f33d`/`c5fee09fe` 的 subtree SHA 均为 `bf85d25e…`，可互换。
- 三个历史 oracle 在不同环境实跑，数值逐位相同；只有 R2 是“方法候选”，R1/R3/R4/R5 为成熟基础 + 工程集成。
- `evidence/manifest.json` 自身 7 条陈旧哈希被记录，不回写。

### A0 基线（`ca7de6d4d`）

- 22 process 全部真实注册；57 operation 白名单。
- X01 在 `fa4649d57`：normalize 8/8、mosaic 1、export 1；blind RMS 0.2109 px。
- 新发现：Normalize 产品 `closure=[]`/`inputs=[]`、缺 PSF/response/noise 闭包；默认逐像素 overlap 调用使单帧 >20 分钟。

### 工作流/产品/Export（`ef91befb7`、`1139f7ec7`）

- mosaic 在 select/weights 之前对每帧调用一次 `sky.fit/apply_sky`，用校正后的 signal/noise 重建 provider；coadd 只携带冻结 fit，不二次减天光。
- normalize 合并每块 `termination/bound_validity/support/recipe/coarse/unresolved`；recipe 冲突使合并 invalid；未合并 primitive 表具名 omitted。
- `block_size != 1` 被拒绝（当前数据模型下它是另一个估计量）。
- 产品发布遍历所有 `$ref`，拒绝引用未在 manifest 的对象；SpatialResponse 的 weights/reference 数组持久化并可重开。
- `result.published` 不再被清空，CLI 正确报告已发布帧（单帧实测 published=1）。
- export 方差改为 `Q C Qᵀ`（`compose(Q, I_leaf)`）。

### X01 验收命题修订（`ef91befb7` + `docs/audit/x01-photometry-statistical-analysis.md`）

- 旧 5% 单样本最大相对误差：**保留为 superseded 失败**（观测 0.1033）。
- 证明：B 组最暗源比值统计量 `σ(M)≈5.85%`，5% 低于其 1σ；48 个比值残差最大 |z|=1.90、均值 0.156、RMS 0.907，全部 ≤3σ。
- 新 gating：48 个比值残差全部 |z|≤4（含源–峰协方差；FWER≈0.003）。
- `comparison.json` 记录为 `pending_at_current_head`：旧数值来自 `fa4649d57` 且用 block=16，必须在最终 HEAD 用默认 block=1 重跑。

## 3. 第一轮独立审查的 P0 清单（尚在修复）

| 域 | P0 | 状态 |
|---|---|---|
| Normalize | measure 通量区间漏列尺度 `‖S⁻¹‖`（反例 F∈[8,12]，给 [8.152,11.848]） | implementer 修复中 |
| Normalize | 暗母版 Jacobian、未知 XP 协方差填单位阵、counts 协方差对角化、PRF 未接线、apply_scale θ 影响 | 修复中 |
| Geometry | 极区残差界漏 `L`（12–31×）、Drizzle N≠ΣwΩ/R=w/N/噪声 wCwᵀ、身份常量 hash、normalize 合并丢状态、block>1 | 前两项/合并由 Lead 已修一部分；implementer 修 chart/drizzle |
| Statistics | 缺 apply_sky 边（已接）、unknown→known、等权 1/K 伪方差、逐叶对角化、sky c/T/A 不同源、PSD 零空间、export 方差（已修） | implementer 修复中 |
| Runtime | 产品 complete 无条件 true（已修）、noise 重开丢失（已降级为 unknown+omitted）、用户 Limits 未接线、broadcast 取消挂起、双线程池、桥接计费 | runtime-fixer 修复中 |

完整证据与反例见 `docs/audit/audit-normalize.md`、`audit-geometry.md`、`audit-statistics.md`、`audit-runtime.md`。

## 4. 现在不能声称的内容

- 不能说 22 process 科学正确；B2/B3/B4 均有未关闭 P0。
- 不能说 X01 当前 HEAD 通过；记录标为待重跑。
- 不能说产品闭包完整；noise 目前显式 unknown，PSF/response 内容未全部重开。
- 不能说 Windows 可运行；本轮仅 Linux x86-64。
- 不能说“区间超越函数已认证”；只有四则/sqrt。

## 5. 下一步

1. 等 geometry/statistics/normalize/runtime 修复报告，逐项复核并构建。
2. 用默认 block=1 在最终 HEAD 重跑 X01 三命令，更新 `comparison.json`（保留旧 5% 失败）。
3. 资源接线：workflow 使用 `Context.resources` 的 Limits，删除硬编码与第二套池。
4. 产品闭包：LinearAction/NoiseModel/CovarianceAction 的 reader，或正式声明 `signal_only` 并传播 omitted。
5. 几何 cohort 化（保持 source row=像素与 `a_jp`/归约序不变），去掉逐像素 invoke 的 6 ms 固定 setup。
6. 重跑受影响 process 案例与 G01/W01；记录 tested_source_sha。
