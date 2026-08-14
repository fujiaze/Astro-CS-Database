# Full Phase1→Phase2 真实多曝光 E2E（V16）

## 链路（真实执行）

```text
raw FITS（NGC1727 T2 H-alpha 1200s × 16）
  → Siril 1.4.4：masterBias/masterDark(1200s)/masterFlat(H-alpha) .xisf → FITS
  → orchestrator stage1 × 16（calibration standard → plate solve（Gaia DR3）
      → PSF/star_measurements → photometric（Gaia DR3SP + Baader 7nm HA）
      → SNR catalogue → drizzle(nside=65536, NESTED) → per-exposure HiPS order 7
  → astrocs-stage2（wbpp_current group-level：nominal=16 → linear_fit；
      normalization=median_center；UPM；rejection；integration；HiPS）
  → external/browser verification
```

## 结果

```text
Phase1 16/16 成功（0 失败）
per-exposure HiPS：order 7，nside 65536，全部含 signal/support/snr
Phase2（truth/clean/trail/trail_none 四组）：每组 rc=0，23-25s
mosaic：14 tiles，1 component，output_pixels 1,517,469
浏览器 hips_backend：2048 查询 mismatch=0（external HiPS PASS）
```

## 命令与超时

```text
siril-cli（xisf→fits）     ：每主文件 ~0.2s
orchestrator stage1 ×16   ：~150s/帧（1200s H-alpha 全帧；含 Gaia 查询）
astrocs-stage2 ×4         ：23.5 / 24.6 / 24.6 / 23.4 s
浏览器 hips_backend       ：PASS
```

详见 `evidence/e2e_manifest.json` 与 `evidence/exact_commands.csv`。

## 数据哈希

```text
raw frames : testdata/NGC1727_T2_flying_dutchman/lights（16 个 H-alpha 1200s）
masters    : testdata/T2 calibration files（bias/dark1200/flat H-alpha）
per-exposure HiPS : run/temp/satgate/e2e/hips_1727/frame00..15.hips
```

## 结论

真实 raw→Phase1→Phase2 多曝光 E2E 全链 PASS（16 exposure）。
