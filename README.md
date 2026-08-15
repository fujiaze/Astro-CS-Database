# AstroCS Normalization Database

## 是什么

天文 CCD 图像校准与标准化数据库系统：Phase1 把单帧 FITS 经校准/定标/
Drizzle 建成标准 IVOA HiPS（signal/support/variance/ivar/snr）；Phase2
把多帧 HiPS 经 UPM 联合光度模型（background-clean 采样、稳健 Huber、
ivar 科学权重）叠加为无缝马赛克。

**当前状态 (V19R2)**: `PRE_RELEASE_ENGINEERING_FOUNDATION=PASS`;
`FINAL_REAL_DATA_VALIDATION=PENDING`。SNR/Noise 按三层模型科学重构
(SNR-001..015), Drizzle 已实现方差传播与操作计数, Phase2 默认 ivar 权重,
全仓逐文件审计/可追溯冻结已闭环（详见 reports/v19r2/）。

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

历史 V1–V19 审核过程归档在 `archive_deliverables/` 与工程控制记录，
不进入本文档。

## 文档体系（V19R2 L0-L5）

- 入口：`docs/README-DOCS.md`、`docs/DEVELOPER_GUIDE.md`、
  `docs/RELEASE_STATUS.md`、`docs/KNOWN_LIMITATIONS.md`、`CHANGELOG.md`
- 科学规范：`docs/science/`（定义/公式/单位/假设/域/误差/ID）
- 算法规范：`docs/algorithms/`（输入/输出/不变量/复杂度/oracle）
- 架构/标准/模块：`docs/architecture/`、`docs/standards/`、`docs/modules/`
- 数据/接口契约：`docs/contracts/`
- 追溯矩阵：`docs/TRACEABILITY.csv`
- 历史：`docs/history/`（V19 扁平文档快照，非 current authority）
- 变更记录：`CHANGELOG.md`
