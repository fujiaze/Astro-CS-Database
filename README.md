# AstroCS Normalization Database

## 是什么

天文 CCD 图像校准与标准化数据库系统：Phase1 把单帧 FITS 经校准/定标/
Drizzle 建成标准 IVOA HiPS（signal/support/variance/ivar/snr）；Phase2
把多帧 HiPS 经 UPM 联合光度模型（background-clean 采样、稳健 Huber、
ivar 科学权重）叠加为无缝马赛克。

**当前状态 (V19)**: `PRE_RELEASE_FOUNDATION_READY=PASS`;
`FINAL_REAL_DATA_VALIDATION=PENDING_V20`。SNR/Noise 已按三层模型科学重构
(SNR-001..015), Drizzle 已实现方差传播与操作计数, Phase2 默认 ivar 权重。

## 输入 / 输出

- Phase1 输入：lights FITS + masterBias/Dark/Flat + GaiaDR3/DR3SP 星表；
  输出：单帧 HiPS（signal/support/snr + V19 variance/ivar）。
- Phase2 输入：Phase1 输出（≥2 帧）；输出：马赛克 HiPS。

## 生产 CLI

```powershell
.\toolchain.ps1 check | build
.\toolchain.ps1 run <stage1.json>      # Phase1
lib\phase2\build\astrocs-stage2.exe <stage2.json>   # Phase2
lib\healpix_db\healpix_browser_qt\build\healpix_browser_qt.exe --hips <root>
```

## 数据语义 / 冻结状态 / 测试 / 基准

- 数据语义：`docs/contracts/DATA_SEMANTICS.md`（唯一权威）
- 接口：`docs/contracts/PUBLIC_API.md`
- 配置：`docs/development/CONFIG_SCHEMA.md`
- 科学冻结：`docs/validation/SCIENCE_FREEZE.md`
- 测试：`docs/development/TESTING.md`
- 性能：`docs/performance/BASELINE.md` / `OPTIMIZATION.md`
- 浏览器：`docs/browser/HIPS_BROWSER.md`
- 下一阶段：`docs/backlog/NEXT_STAGE.md`

历史 V1–V18 审核过程归档在 `archive_deliverables/` 与工程控制记录，
不进入本文档。

## V19 开发/科学文档

- 架构/模块：`docs/ARCHITECTURE.md` / `docs/MODULE_INDEX.md`
- 开发者指南：`docs/DEVELOPER_GUIDE.md` / `docs/BUILD_RELEASE.md`
- 数据/配置契约：`docs/DATA_CONTRACTS.md` / `docs/CONFIG_REFERENCE.md`
- 科学算法/噪声模型：`docs/SCIENTIFIC_ALGORITHMS.md` /
  `docs/SNR_NOISE_MODEL.md` / `docs/DRIZZLE.md` / `docs/PHASE2.md`
- 诊断/排障：`docs/DIAGNOSTICS.md` / `docs/ERROR_TAXONOMY.md` /
  `docs/TROUBLESHOOTING.md`
- 测试/性能/限制：`docs/TESTING_AND_ACCEPTANCE.md` /
  `docs/PERFORMANCE.md` / `docs/KNOWN_LIMITATIONS.md`
- 变更记录：`CHANGELOG.md`
