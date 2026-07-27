# AstroCS 测光修正与银心三片马赛克验证方案 v1

- 文档状态：讨论稿，不是已执行结果
- 审计基线：AstroCS v1.1，`main` at `a3b468d`

## 1. PlateSolve 单次检测路径状态

当前主线已经合并单次检测共享方案：

```text
PlateSolve 内部 sdet_detect_ex（一次）
        ├── PlateSolve 内部继续选星与求解
        └── callback 同步导出完整 detections
                     ↓
                 star_det
                     ↓
                    PSF
```

生产代码使用 `ipv_solve_from_memory_with_callback()`。提交 `f8097df` 已是当前 `main` 的祖先。

710 帧 A/B：旧路径 709/710，新路径 709/710；RMS 和 n_pairs 无退化。它不是“外部 STAR_DETECT 先算、PlateSolve 再消费”的路径，而是“PlateSolve 内部唯一检测并共享导出”。现有证据把它标成 `UPSTREAM_SHARED_DETECTIONS` 容易误解，建议更名为：

```text
INTERNAL_DETECTION_SHARED_EXPORT
```

PSF 的 float32 API 尚未接入，仍有 float32→uint16 全图量化，这与检测去重是两个独立问题。

## 2. 测光实际失败位置

银心 Red 代表帧日志：

```text
Gaia 光谱星：14649
F_syn 有效：14649/14649
PSF 有效：1984/2000
空间 KD-tree 匹配：1
最终 n_matched：1
sigma_residual：0
```

其他天区也出现：

```text
Gaia 光谱星：2489
F_syn 有效：2489/2489
空间匹配：1
```

因此：

- Gaia DR3SP 文件能解析；
- 光谱能读取；
- 滤光片和 QE 曲线能加载；
- 合成流量积分能完成；
- 失败发生在 Gaia 天球坐标投影到 PSF 像素坐标后的空间匹配。

“解析成功”和“积分成功”只证明参考星流量存在，不保证其投影坐标与图像星点处于同一像素坐标系。

## 3. 首要根因：PlateSolve 内部 Y 轴与标准像素 Y 轴转换不完整

PlateSolve 选择阶段定义：

```cpp
U.x = det_x - cx;
U.y = -(det_y - cy);  // 内部 Y 轴向上
```

但 WCS 提取阶段直接写：

```cpp
CD1_1 = trans.x10 / 3600;
CD1_2 = trans.x01 / 3600;
CD2_1 = trans.y10 / 3600;
CD2_2 = trans.y01 / 3600;
```

标准 WCS 像素偏移的 `dy` 与内部 `U.y` 关系为 `U.y=-dy`。因此把内部多项式转换为标准像素坐标时，所有含奇数次 y 幂的项必须变号。

线性项至少应满足：

```text
CD1_1 =  trans.x10 / 3600
CD1_2 = -trans.x01 / 3600
CD2_1 =  trans.y10 / 3600
CD2_2 = -trans.y01 / 3600
```

对任意内部项 `c_ij Ux^i Uy^j`，标准像素多项式系数应为：

```text
c'_ij = (-1)^j c_ij
```

SIP A/B 及 AP/BP 必须基于转换后的标准像素多项式重新计算，不能只在测光模块临时翻转坐标。

为什么已有测试未发现：

1. PlateSolve A/B 比较的是两条检测路径，二者共用同一 WCS 提取；
2. PlateSolve RMS 使用内部 U→切平面残差，不等于标准 FITS WCS 的外部正确性；
3. SNR 与 Astropy 的测试证明“下游实现与所写 Header 一致”，未证明 Header 与真实 Gaia/像素星对一致。

## 4. 修正原则

不要在 photometric matcher 中做私有 Y 翻转补丁。应从 PlateSolve WCS 生产者修复，使以下模块共用同一正确契约：

- PHOTOMETRIC sky→pixel；
- SNR pixel→sky；
- Drizzle pixel→sky；
- HISS 天球定位；
- 浏览器显示与坐标读取。

建议在 PlateSolve 中增加唯一的 `convert_internal_trans_to_fits_wcs()`，集中完成：

1. 内部 U 坐标到标准 pixel offset 的符号变换；
2. CD 提取；
3. SIP A/B 提取；
4. AP/BP 重算；
5. CRPIX 0/1-based 契约；
6. 输出真实星对外部残差。

## 5. 修复后的 WCS 验证 Gate

### 5.1 真实匹配对闭环

PlateSolve 输出至少一份诊断块：

```text
astrometry_matches FLOAT64 [N,6]
[det_x, det_y, gaia_ra, gaia_dec, residual_x, residual_y]
```

使用最终写入 Header 的 WCS，把每个 Gaia RA/Dec 重新投影到像素。要求：

- 中位像素残差与 PlateSolve RMS 一致量级；
- p95 不超过预设阈值；
- 图像四象限均有匹配，禁止只在中心线偶然匹配；
- 与 Astropy `all_world2pix` 一致；
- pixel→sky→pixel 往返一致。

### 5.2 全量 710 帧

继续保留原 PlateSolve 成功率/RMS Gate，同时增加外部 WCS Gate。检测共享路径不回退，除非修复与 callback 发生直接冲突。

## 6. 测光模块修正

WCS 修复后，测光应分阶段输出诊断，不再只给一个 `n_matched`：

```text
n_gaia_spectrum
n_fsyn_valid
n_gaia_in_image
n_psf_valid
n_spatial_matches_raw
n_flux_valid
n_prefilter_pass
n_irls_inliers
scale_factor
sigma_residual
```

### 6.1 空间匹配

- 使用正确 WCS；
- 先保留 2 px 半径，记录距离直方图；
- 根据真实残差决定是否调整，不通过扩大半径掩盖 WCS 错误；
- 强制一对一匹配，避免多个 PSF 匹配同一 Gaia 星；
- 排除画面外 Gaia 星后再建 KD-tree。

### 6.2 流量清洗

当前使用 `Gaia G` 与仪器星等差做 3 mag 预过滤。G 波段与窄带/特定滤镜不等价。建议：

1. 先按 PSF status、饱和、边缘、`F_syn>0` 过滤；
2. 主要在 `r=log10(F_instr/F_syn)` 上做稳健估计；
3. G-mag 预过滤改为可选诊断，或根据 BP/RP 色彩设置滤镜相关门限；
4. 对宽带和窄带分别登记门限。

### 6.3 失败语义

生产模式下：

- 空间匹配低于最小数量；
- IRLS inlier 低于最小数量；
- `sigma_residual<=0`；

均不得伪装为“PHOTOMETRIC 成功”。应返回明确错误或 `DEGRADED`，只有显式 `allow_unphotometric=true` 才允许继续，并在 HISS provenance 中标记。

## 7. T1–T4 数据与校准帧整理

Agent 必须扫描 TestData 每个子目录内的说明文档、FITS Header 和校准文件，不再从目标名或固定文件名猜测。

生成：

```text
testdata_inventory/
├── TELESCOPE_PROFILES.yaml       # T1-T4 器材、相机、像元、焦距、口径
├── FILTER_REGISTRY.yaml          # Header 名、文档名、响应曲线名、Flat 名别名
├── DATASET_MANIFEST.csv          # 每张 Light 的设备与采集信息
├── CALIBRATION_INVENTORY.csv     # 所有 Bias/Dark/Flat 与解析元数据
├── CALIBRATION_MATCH_MATRIX.csv  # 每组 Light 对应哪些 Master
└── INVENTORY_REPORT.md
```

信息优先级：

```text
子目录说明文档 > FITS/XISF 元数据 > 文件名 > 推断
```

校准匹配依据：

- telescope profile；
- camera；
- width/height；
- binning；
- filter alias；
- exposure；
- temperature；
- calibration type。

当前 Orchestrator 通过固定模板拼接 `masterFlat_BIN-1_..._FILTER-<HeaderFilter>_mono.xisf`。用户已确认主校准帧齐全，因此现有“Lum Flat 缺失”优先判为**解析器/别名/命名匹配失败**，直到 inventory 证明文件确实不存在。

现有 710 帧 PlateSolve manifest 只登记到 T2/T3/T4；Agent 必须从实际 TestData 说明文档确认 T1 所在目录和是否属于当前 710 帧集合。

## 8. 银心三片马赛克验证

TestData 中银心三片共同 Red 数据：

```text
panel1 Red：11 帧
panel2 Red：11 帧
panel3 Red：10 帧
总计：32 帧，180 s
```

### 8.1 Stage 1

对 32 帧全部执行：

```text
真实 Master 校准
→ 单次检测共享 PlateSolve
→ PSF
→ 修正后的测光
→ SNR
→ Drizzle
→ HISS
```

每份 HISS 必须：

- `photometric_status=OK`；
- `n_irls_inliers` 达到门限；
- `sigma_residual>0`；
- `has_snr=true`；
- provenance 含 T4、Red、Master 文件 hash、WCS 外部残差。

### 8.2 Stage 2 分层验证

1. 每个 panel 内部先叠加；
2. panel1+panel2；
3. panel2+panel3；
4. panel1+panel2+panel3；
5. 梯度关闭/开启 A/B；
6. 注入已知球面低频梯度后验证恢复。

必须记录：

- overlap graph 连通性；
- 每条 edge 的样本数；
- 校正前后 overlap 中位差和 MAD；
- 梯度拟合 RMS；
- 校正场范围和分位数；
- 星点通量保持；
- 扩展天体大尺度结构保持；
- 每像素覆盖/拒绝/权重；
- SNR² 是否真实启用；
- 内存与耗时。

建议初始 Gate：

- 两条相邻面板 edge 均有效；
- 接缝稳健偏差至少下降 50%，且任何 edge 不得恶化超过 10%；
- 注入梯度恢复相对误差 ≤10%；
- 未饱和星校正前后相对通量变化 ≤2%；
- 无 NaN/Inf、明显条带或过拟合波纹。

门限可在首轮数据统计后由 ADR 固化，不允许根据失败结果临时放宽。

## 9. 用户可见交付

完成后不仅提交日志，还要交付：

- 32 份 HISS 或可复现清单；
- 银心三片 Red HCSD；
- 优化后的浏览器可执行文件；
- 全景与两处接缝截图；
- 梯度关闭/开启同视角对比；
- 浏览器性能 overlay；
- 数值报告和失败帧清单。

这样用户可以直接在修复后的球面浏览器中检查大尺度马赛克结果。
