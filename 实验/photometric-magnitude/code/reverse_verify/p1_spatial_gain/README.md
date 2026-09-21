# P1-SPATIAL-GAIN —— Phase1 低阶空间乘法增益（逆向验收实验）

工作项报告：`实验/photometric-magnitude/docs/p1-spatial-gain.md`。参考文献：`实验/shared/references/REVERSE_VERIFY_BIBLIOGRAPHY.md` 的 P1SG-R1..R3。

## 一句话

验证 `I_photo = k_photo · m(x,y) · I_cal` 中**低阶空间乘法增益** `m` 能否用**星点**（而非背景）
拟合出来、能吃掉多少帧间乘性残差、阶数取多少，以及在生产链里的**最小改动面**。

## 目录

```
reverse_verify/
├── synthetic/
│   ├── synth_gain.py       # 合成帧生成 + 恢复/负例/阶数扫描/星数敏感性 (真值已知)
│   └── gainlib.py          # 共享: 低阶多项式基 + 加权 Tukey-IRLS 曲面拟合 + 度量
└── experiments/p1_spatial_gain/
    ├── cpp/p1sg_oracle.cpp # 独立 C++ Oracle (判据内嵌, 退出码 0 = 全 PASS)
    ├── src/
    │   ├── real_gain.py         # 真实数据: 逐星匹配 + 帧间差分联合拟合 + before/after
    │   ├── real_ridge.py        # Tikhonov 先验扫描 + 分半一致性 (可辨识性诊断)
    │   ├── real_pixel_check.py  # 像素级复核: 真把 m 乘到像素上 (r=6 独立口径)
    │   ├── aperture_probe.py    # 孔径探针: 空间结构是否与孔径无关 (乘法增益的判据)
    │   ├── analyze_real.py      # 覆盖 / 逐帧幅度 / 按星数分箱的 before-after 表
    │   └── wcs_lib.py           # 与生产 wcs_transform.cpp 逐行对齐的 numpy 移植 (自带一份)
    ├── data/                    # 结果 JSON (入库, 供论文引用)
    └── logs/                    # 运行日志
```

大产物/临时文件落 `run/reverse_verify/p1-spatial-gain/`（gitignore）。

## 复跑

```bash
cd '/workspace/Astro CS Database'
export TMPDIR=/dev/shm/astrocs_p1sg

# 独立构建 (standalone, 不并入主线 build/)
cmake -S reverse_verify -B run/reverse_verify/build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C run/reverse_verify/build
./run/reverse_verify/build/rv_p1sg_oracle

# 合成 (150 MC, 约 7 min)
cd 实验/shared/synthetic && python3 synth_gain.py --nproc 12

# 真实数据 (49 帧)
cd 实验/photometric-magnitude/code/reverse_verify/p1_spatial_gain/src
python3 real_gain.py && python3 real_ridge.py && python3 real_pixel_check.py \
  && python3 aperture_probe.py && python3 analyze_real.py
```

## 数据依赖

- 真实数据：`run/RELEASE-02/L4-rebuild/norm/<tile>/`（49 帧 `calibrated_*.fts` + `p1_flux.json` + `p1_wcs.json`）。
  根目录由 `real_gain._find_root()` 自动向上查找 `ASTROCS_DESIGN.md`，也可用环境变量 `ASTROCS_ROOT` 覆盖。
- 合成实验：无外部依赖（自带场景生成）。

## ⚠ 本轮最重要的结论（负面，务必先读）

**合成（真值已知）：方法有效** —— order 1 把帧间乘性残差场从 9.36% 压到 2.30%（−75%），负例收到统计噪声底（1.24%），独立 C++ Oracle 的 C1/C2/C4/C6 全 PASS。

**真实数据（49 帧）：未获验证** —— 拟合出的空间结构**随孔径强烈变化**：
同一跨镜帧对的 3×3 峰峰在 r=3/4/6/10 上为 **7.78% / 4.93% / 3.81% / 4.25%**。
纯乘法增益不可能有孔径依赖 ⇒ 该结构主要由**孔径/PSF 系统差**（小孔径对 seeing 敏感 + 星云区小环带天光污染）主导。
用 r=4 拟合的 m 在 **r=6 独立口径**上复核，4/4 代表帧对**变差**。
⇒ **在 `F_instr` 换成 PSF 稳健口径并通过「孔径无关性验收」之前，不建议在生产开启空间增益**（报告 §4.5 / §6.0）。


- 判据阈值写在 `cpp/p1sg_oracle.cpp` 顶部 `THRESH_*`（**先写判据再看结果**）；
- 未达标项在报告 §0 / §3.6 登记，**不放宽**；
- 真实数据的**逐帧绝对 m 幅度不可辨识**（联合差分模型的固有局限），报告 §4.2 有诊断与登记。
