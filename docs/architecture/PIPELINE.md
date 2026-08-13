# AstroCS Pipeline（Phase1 → Phase2）

## Phase1（单帧 → 单帧 HiPS）

```text
read FITS/metadata → calibrate(A/B/C) → plate solve/WCS →
PSF → photometry → SNR catalogue → drizzle(HEALPix NESTED) →
HiPS write(signal/support/snr, 512-tile, hierarchy) → verify
```

入口：`orchestrator.exe <stage1.json>`。

## Phase2（多帧 → 马赛克 HiPS）

```text
discover/validate manifest → coverage union → control sampling
(background-clean, V13) → UPM fit（全几何节点 + 平滑 continuation,
V13/V14）→ block plan → calibrate → reject(7种) → integrate →
HiPS write（V11 serialization）→ verify
```

入口：`astrocs-stage2.exe <stage2.json>`。

## 关键不变量

- 科学冻结：Phase1/2 基础算法语义以 `SCIENCE_FREEZE.md` 为准。
- 序列化：V11（外部 oracle 冻结）；sampler/UPM：V13/V14。
- Browser 不拥有科学数据解释权。
