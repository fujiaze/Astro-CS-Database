# I-003 发布包清单 (RELEASE MANIFEST)

- Gate: I
- 任务: I-003
- 日期: 2026-07-30
- 状态: DRAFT (待710帧完成后填充)

## 1. 发布包结构

```
AstroCS_Release_v1.0/
├── README.md                          # 项目说明（根目录 SSOT）
├── LICENSE                            # 许可证
├── VERSION                            # 版本号 1.0.0
├── SHA256SUMS.txt                     # 全包校验和
│
├── bin/                               # 编译产物
│   ├── orchestrator.exe               # 编排器主程序
│   ├── astro_image_io.dll             # 图像IO模块
│   ├── astro_calibration.dll          # 校准模块
│   ├── ipv_solver.dll                 # 板解模块
│   ├── dynamic_psf.dll                # PSF模块
│   ├── photometric_calib.dll         # 测光模块
│   ├── snr_estimator.dll              # SNR模块
│   ├── hp_drizzle.dll                 # Drizzle模块
│   ├── hp_stack.dll                   # 叠加模块
│   ├── gaia_client.dll                # Gaia客户端
│   └── healpix_browser_qt.exe         # 球面浏览器（独立exe）
│
├── configs/                           # 配置文件
│   ├── stage1_config_T2.json
│   ├── stage1_config_T3.json
│   ├── stage1_config_T4.json
│   ├── stage2_config.json
│   └── filters.json + qe_curves.json
│
├── contracts/                         # 冻结契约（FROZEN）
│   ├── CLI_CONTRACT.md                # CLI接口契约
│   ├── ALGORITHM_CONTRACT.md          # 算法契约
│   └── HISS_FORMAT_V2.md             # HISS格式规范
│
├── docs/                              # 文档
│   ├── specs/                         # 8份实现规范 (01-08)
│   ├── gate_reports/                  # Gate A-I 合并报告
│   └── api/                           # API文档
│
├── evidence/                          # 测试证据
│   ├── I-002/stage_a_report.md        # 15帧代表帧验证
│   ├── I-002/stage_b_report.md        # 710帧全量回归（待生成）
│   ├── G-004/                         # 银心30帧叠加证据
│   └── G-005/                         # 接缝/连续性量化
│
└── source/                            # 源码（git仓库快照）
    └── (git archive 或 .tar.gz)
```

## 2. 发布包依赖

| 依赖 | 版本 | 说明 |
|------|------|------|
| MSYS2/MinGW64 | GCC 13+ | C++ 编译环境 |
| Qt6 | 6.5+ | 球面浏览器 GUI |
| Python | 3.9+ | Stage2 算法链 + 工具 |
| scipy/numpy | latest | Stage2 稀疏求解 |
| GaiaDR3SP | DR3 | 星表数据（不打包，单独提供） |

## 3. 运行时 DLL 依赖

C:\msys64\mingw64\bin 需加入 PATH（libstdc++-6, libgcc_s_seh-1, libwinpthread-1 等）

## 4. 版本信息

| 项目 | 值 |
|------|------|
| 版本 | 1.0.0 |
| git_commit | (待710帧完成后确定) |
| orchestrator_sha256 | 022E2D037D6C6AD9D216AEA6AD027E0AB211B0BA09F2741ACB2FE234EC3CCC80 |
| 发布日期 | 2026-07-30 |

## 5. 验收门限

| 检查项 | 标准 | 状态 |
|--------|------|------|
| 契约冻结 | CLI+算法+HISS 三份 FROZEN | PASS (I-001) |
| 710帧回归 | 通过率 ≥ 95% | 待 I-002 完成 |
| 失败分类 | 全部失败帧有根因 | 待 I-002 完成 |
| 发布包完整 | SHA256SUMS 校验通过 | 待生成 |
| 网站素材 | 与状态一致 | 待生成 |
