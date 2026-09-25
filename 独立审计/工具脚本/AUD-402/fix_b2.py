import io

P = r"独立审计/工具脚本/AUD-402\rows_b2.tsv"
ROW = [
    "upm.k_corr",
    "eng/packaging/config/defaults.json:639",
    "1.4",
    "1（无量纲）",
    "控制点方差相关放大因子：control_variance = k_corr·(π/2)·σ_bg²/N_retained",
    "未标注；SCI 记保守取整 1.4，MC 实测 ≈1.3883",
    "1 ≤ k_corr",
    "需实验标定",
    "docs/science/PHASE2_UPM.md:24,:124（改默认属不可接受变化）；:116 保守取整；:131/:169 "
    "MC k_corr≈1.3883（control_median_mc_test 未注册/MISSING，不可复跑）",
    "实验标定",
    "Phase2 UPM 控制点方差",
    "**支撑实验不可复跑**（defaults.json:649 自证 MISSING）⇒ 1.4 的标定证据链断裂；代码兜底同值三处："
    "lib/algorithms/coverage/src/sky_plane.cpp:233/:254、include/astro/phase2/sampler.h:55 注释；另"
    "lib/algorithms/drizzle/healpix_drizzle/v6_drizzle_science.h:377 同名 k_corr = 0.0"
    "（同名不同族，未进配置键面）",
]
assert len(ROW) == 12
lines = io.open(P, encoding="utf-8").read().split("\n")
n = 0
for i, l in enumerate(lines):
    if l.startswith("upm.k_corr\t"):
        lines[i] = "\t".join(ROW)
        n += 1
io.open(P, "w", encoding="utf-8", newline="\n").write("\n".join(lines))
print("replaced", n, "row(s)")
