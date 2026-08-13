# AstroCS Normalization Database

## 是什么

天文 CCD 图像校准与标准化数据库系统：Phase1 把单帧 FITS 经校准/定标/
Drizzle 建成标准 IVOA HiPS；Phase2 把多帧 HiPS 经 UPM 联合光度模型
（background-clean 采样、稳健 Huber、全局平滑延续）叠加为无缝马赛克。

## 输入 / 输出

- Phase1 输入：lights FITS + masterBias/Dark/Flat + GaiaDR3/DR3SP 星表；
  输出：单帧 HiPS（signal/support/snr）。
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

历史 V1–V13 审核过程归档在 `archive_deliverables/` 与工程控制记录，
不进入本文档。
