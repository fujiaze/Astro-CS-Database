# RELEASE-02 端到端运行 + 视觉验证报告

- **日期**: 2026-09-20
- **仓库 HEAD**: `06216ec8`（工作树 111 项改动，未提交）
- **命令**: `./build/astrocs normalize|mosaic|export`（真实三命令，**非**测试桩、**非** in-process 调用）
- **数据**: L4 真实 M42 数据（`configs-phot/`，含 Gaia DR3/DR3SP 标定）
- **证据目录**: `run/RELEASE-02/e2e/`（日志、配置、产物）+ `run/RELEASE-02/e2e-visual/`（图）

---

## 0. 结论

**三命令全链路跑通，且测光归一化在生产上第一次真正执行。**

| 阶段 | 命令 | rc | wall | maxRSS | 结论 |
|---|---|---|---|---|---|
| P1 | `normalize` t2_m1（2 帧） | **0** | 86.2 s | 3.32 GB | ✅ `photometry_applied=True` |
| P1 | `normalize` t2_m2（4 帧） | **0** | 305.8 s | 8.75 GB | ✅ `photometry_applied=True` |
| P2 | `mosaic` weight_mode=1（6 帧） | **0** | 47.4 s | 2.52 GB | ✅ 203 leaf tiles |
| P2 | `mosaic` weight_mode=2（6 帧） | **2** | 29.4 s | 0.98 GB | ⛔ fail-closed（见 §4） |
| P3 | `export`（2048², TAN, bilinear） | **0** | 3.0 s | 55 MB | ✅ **100% 有效像素** |

---

## 1. ⛔ 本次 E2E 发现并修复的**阻断级缺陷**

### 1.1 「组间 k 散度门」仍在阻断生产（**负责人 §9.49 定案 2 明令删除的那条**）

首跑 `normalize` **rc=0、无任何报错**，但产物 `p1_phot.json`：

```json
{"photometry_applied": false,
 "photscal": 1.0,
 "degraded_reason": "photscale_incomplete",
 "photscale_error": "photscale inconsistent across frames (max/min=1.074356 = 0.031148 dex > 0.020000 dex); refusing to apply a mixed photometric system"}
```

⇒ **测光归一化在生产上完全不执行**。该门是 **fail-closed**（拒绝执行而非产出错值），
故此前 L4 产物**不是错的，是缺一步**；但**负责人「所有帧已归一化到同一测光体系」的前提在此前所有运行中都不成立**。

**这与负责人裁决逐字冲突**：
> 「**极度异常值拒绝，并抛出错误**，其他的合理范围都可以接受。**这玩意应该是帧间独立的，为啥要组间对比**」
> 「**不同光学系统的帧混装不得报错**」

**独立佐证**：`reports/RELEASE-02/pmm-study.md` §Q-B —— **PhotometricMosaic 亦没有任何「拒绝帧」的跨帧一致性门**；
其 `scale` 允许任意量级（源码注释显式支持 12bit vs 16bit、高达 2× 尺度差）；跨帧比例只用于**星点匹配预筛**，
超限后果是「这一对星不匹配」而**不是「这一帧被拒绝」**。PMM-STUDY 的第一条建议即删除此门。

**修复**（按裁决：**门降级为报告字段**，不是简单删掉）：
- 删除 fail-closed 分支；
- **保留** `spread_dex` 计算与落盘：新增 `photscale_spread_dex` / `photscale_spread_warn` /
  `photscale_spread_gate = "none (owner ruling 9.49: frame-independent)"`；
- **帧内**判据 `P1_PHOT_MIN_FIT_STARS=3`、`P1_PHOT_MAX_SIGMA_DEX=1.0` **不变**。
- 变更 claim：`工程控制/RELEASE-02/change-claims/PHOT-GATE-DROP-001.md`。

### 1.2 修复前后实测（同一输入）

| 字段 | 修复前 | **修复后** |
|---|---|---|
| `photometry_applied` | **false** | **True** |
| `photscal` | 1.0（伪） | **5.647065217024146e-17**（真实 `k_photo`） |
| `photscale_spread_dex` | （触发门，未落盘） | **0.031148161841308315** |
| `photscale_spread_warn` | — | **True**（仅提示） |
| `degraded_reason` | `photscale_incomplete` | **None** |

### 1.3 逐帧独立核对（**最强证据**）

`photoapplied_*.fts ÷ calibrated_*.fts` 的逐像素中位比值 vs 落盘的 `k_photo`：

| 帧 | measured ratio | `k_photo` | 相对偏差 |
|---|---|---|---|
| `...20251212_012404-300S-Red` | **5.64706522e-17** | 5.64706522e-17 | **1.478e-11** |
| `...20251224_031410-300S-Red` | **5.25623340e-17** | 5.25623340e-17 | **6.666e-12** |

⇒ **测光标度被逐像素精确施加**（float32 量化极限量级），**不是「声明已应用」**。

---

## 2. 视觉验证（**负责人硬性要求：必须拉伸，纯线性看不了**）

### 2.1 「纯线性看不了」的定量证据

单帧 `calibrated_*`（4096²，ADU）：

| 量 | 值 |
|---|---|
| min | −16975.64 |
| median | 195.98 |
| p99 | 439.57 |
| max | 80789.73 |
| **max / median** | **412.23×** |

⇒ 线性显示时全部结构被压在 1/412 的动态范围里，**肉眼只能看到一片均匀灰**。

### 2.2 图 1：`02_linear_vs_stretch.png`

- **左（LINEAR raw）**：一片均匀灰，仅个别亮星可见 —— **与上面的 412× 定量一致**；
- **右（asinh 拉伸，`arcsinh((x−med)/(20·σ))/arcsinh(5)`）**：
  **完整星场**：左上 **Trapezium 星团**、M42 星云结构、数百颗星清晰可辨。

⇒ **拉伸不是「美化」，是使结构可见的必要条件**；本报告所有图均用同一 asinh 拉伸。

### 2.3 图 2：`01_normalize_frames.png`（4 面板）

`calibrated_*`（2 帧）与 `photoapplied_*`（2 帧）并列：**测光施加前后肉眼可见差异**，
与 §1.3 的 `1.5e-11` 定量一致。

### 2.4 图 3：`03_export.png`（最终产品）

`output_phase3.fits`（2048², TAN, 0.4″/px, `surface_brightness`）：

| 量 | 值 |
|---|---|
| 有效像素 | **100.00%** |
| median | 4.357e-4 |
| min | −5.896e-3 |
| max | 0.16749 |

**目视**：干净密集星场，**无可见接缝、无拼接痕、无块状伪影**。

### 2.5 接缝定量检查（`04_seam_profiles.png`）

行/列中位剖面的逐像素跳变：

| 方向 | 中位跳变 | p99.9 / 中位 |
|---|---|---|
| 行 | 5.724e-7 | **7.85** |
| 列 | 4.990e-7 | **5.22** |

⇒ 中位跳变在 **1e-7 量级**（相对产品中位 4.36e-4 约 **0.13%**），
**p99.9/中位 ≈ 5–8**（无「阶跃式」接缝特征，阶跃会给出 ≫10 的比值）。

⚠ **诚实边界**：本检查用的是**剖面中位跳变**，对**位于剖面之外、局部的短接缝不敏感**；
且与 P9 在**另一路径**（`p3_upmfix`）上测到的 seam step（H1bot −6.36% / H1top −3.75% / V-right −3.06%）
**不是同一产品**，两者**不可互相印证也不可互相否定**。**不得据此宣称「全产品无接缝」**。

---

## 3. 复现命令（**逐条可跑**）

```bash
cd "/workspace/Astro CS Database"
export TMPDIR=/dev/shm/astrocs_e2e

# P1（两块，6 帧）
bash run/RELEASE-02/e2e/run_e2e.sh
# P2 + P3
bash run/RELEASE-02/e2e/run_p23.sh
# 视觉验证（生成全部图）
python3 run/RELEASE-02/e2e/visual_verify.py
```

日志：`run/RELEASE-02/e2e/logs/{e2e,e2e_p23,mosaic_w1,mosaic_w2,export2}.log`
配置：`run/RELEASE-02/e2e/configs/*.json`

---

## 4. ⛔ 仍未闭合：`weight_mode=2`（逆方差加权）在生产上不可用

```
astrocs: phase2 failed: node integrate failed: weight_mode=2 requires per-frame ivar products;
6/6 frames missing ivar; frame-SNR weight chain NOT closed (unclosed_missing_frame_snr):
4680031429619600814: frame-level SNR missing
```

- **fail-closed 行为正确**（拒绝产出错误的加权结果，而不是静默降级）；
- 但**逆方差加权链路至今未在真实生产数据上跑通过一次** ⇒
  **「逆方差加权叠加」这一论文主张目前无生产级证据**（与 P8 的 B8/B19 独立发现一致）；
- 根因：`normalize` 未产出 per-frame variance（`uncertainty_available=false`），
  且 frame-SNR 链未闭合 ⇒ **这是 RELEASE-02 的剩余头号阻塞项**。

---

## 5. 诚实边界

1. **本报告的所有图都经过 asinh 拉伸**；线性原图见 `02_linear_vs_stretch.png` 左面板；
2. **接缝检查只覆盖剖面中位**（见 §2.5），**不构成「无接缝」证明**；
3. `weight_mode=2` **未跑通**，本报告的 mosaic/export 全部是 **`weight_mode=1`（等权）**；
4. **本轮 E2E 用的是 6 帧（2 块 tile）**，不是 49 帧全量；49 帧全量另见 `reports/RELEASE-02/`；
5. **本报告发现的缺陷（§1）是在 E2E 过程中才暴露的** —— 此前所有单元测试与机器门**均未捕获**，
   因为门只在**跨帧**时触发，而单测都是单帧/合成小样本。**这是「真实端到端跑通」不可被替代的直接证据**。