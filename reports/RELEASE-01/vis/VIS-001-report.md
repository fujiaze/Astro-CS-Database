# 工程控制 / RELEASE-01 — VIS-001 视觉验证记录

> 依据：tasks/VIS-001.md；ACCEPTANCE_SPEC.md L4 §5.2 视觉验收清单。脚本与产物：`artifacts/RELEASE-01/VIS-001/`（拉伸/切块/裁剪脚本随证据归档）。

## 1. 方法与可复现性

```bash
# 拉伸（两组同一套参数：asinh，百分位 1/99）
python3 tools/quality/plane_stretch.py --input <p3>_vis/output_phase3.fits \
  --out <g>_full.png --thumbnail <g>_thumb.png --thumbnail-size 2048 \
  --stretch asinh --percentiles 1,99 --sidecar <g>_stretch.json
# 切块（4×4，tile 1024，带行列标号 + 缩略图）
python3 tools/quality/plane_chunks.py --input <g>_full.png --outdir chunks_<g> --grid 4 --tile 1024 --label --thumbnail 256
# 可疑区域 1:1 裁剪放大
python3 artifacts/RELEASE-01/VIS-001/l4/profile_and_crop.py
```

- 输入：L4 全量 R 通道导出 `run/RELEASE-01/e2e/l4/p3_{m42,gc}_vis/output_phase3.fits`（4096²，全覆盖重采样尺度）；
- 证据：整幅 PNG（4096²）、缩略图（2048²）、分块 PNG 各 16 张、裁剪放大图 5 张、拉伸参数 sidecar、数值剖面脚本输出。

## 2. 逐项结论（L4 §5.2）

| 检查项 | M42（T2+T3，6 面板 × R） | Galaxy Center（T4，3 面板 × R） |
|---|---|---|
| 无黑洞（零值/死区/未填充孔洞） | **通过**：覆盖区内无零值（zero=0.0000%）；覆盖外为 **NaN**（full 3.53% / vis 31.08%），非死区 | **通过**：覆盖区内无零值（zero=0.0000%）；覆盖外 NaN（full 6.81% / vis 50.60%） |
| 无亮斑（宇宙线/伪影/饱和溢出） | 通过：未发现伪亮点；亮星呈正常衍射星芒 | 通过：未发现伪亮点 |
| **无接缝（帧间亮度阶跃/重影/重复星点）** | **不通过**：水平接缝可见，行中位数跳变为典型行差的 **17.2–22.4×**（行 4710/4753/5091/5093/10784） | **不通过**：多条水平接缝 + 面板间背景台阶，跳变 **9.8–12.4×**；裁剪放大可见面板交界两侧亮度不同 |
| 星点圆锐无拖尾 | 通过 | 通过（噪声较重） |
| 背景均匀 | **不通过**：面板间背景台阶（同接缝） | **不通过**：同上，且南侧整体偏暗 |
| 全局观感 | 星云结构（M42/M43/暗带）清晰可信 | 银道结构、暗星云、星场密度真实可信 |
| 可疑区域放大复核 | 已放大 `crops/m42_seam1.png`、`crops/m42_upper.png`：无重复星点/重影 | 已放大 `crops/gc_seam1.png`（面板交界亮度台阶清晰）、`gc_seam2.png`、`gc_core.png` |

另记：两组均可见少量细直线条纹（卫星/流星轨迹），属原始数据现象，非管线伪影；未做移除（设计未要求）。

## 3. 归因与处置

- 接缝根因：生产 mosaic 的 UPM 为**纯加性帧级标量** `b_k`，无 `b_k(x)=B_ref(x)+δ_k(x)` 稀疏天光面（GAP_AUDIT P0-09 / A2 UPM-01）→ 面板/望远镜间背景台阶无法被平滑掉；这与 L3 小批量同一现象（`l3_smoke/gc_full.png` 已可见水平带）一致。
- 处置：按 VIS-001 步骤 5，登记「天区/块/现象」→ 修复归属为 **P0-09**（实现侧）；**本轮不修复**（属新文档包新增语义，是否纳入 RELEASE-01 范围待负责人裁决，见 GAP_AUDIT §4 范围裁决）。
- 因此 **VIS-001 判定为 FAIL**（§5.2「无接缝/背景均匀」两项不通过），不以"整体观感正常"掩盖局部缺陷。
