# S1 Contract Freeze — V19R2

状态：PASS

## 输出

- reports/v19r2/contract_inventory.csv：44 行既有权威 ID

## 既有生产科学语义 → 权威 ID 映射（全覆盖）

| 科学域 | 权威 ID | 来源 |
| --- | --- | --- |
| Noise/SNR/ivar | SCI-NOISE-001..015（SNR-001..015） | SNR_REDESIGN_CONTRACT / docs/SNR_NOISE_MODEL.md |
| Drizzle 方差/候选/重叠 | SCI-DRZ-001, 014, 015, 016 | docs/DRIZZLE.md + V19 代码注释 |
| Phase2 UPM | SCI-UPM-001..010（W2 冻结） | 工程控制/docs/PHASE2_INTERFACE_FREEZE |
| UPM 持久化绑定 | SCI-UPM-PERSIST-001 | 本包 PR_UPM_GATE（已冻结） |
| Rejection | ALG-REJ-001..008（RJ-001..008） | V15 语义冻结 + memory |
| Integration | SCI-INT-001,002,004,008 | 工程控制/docs |
| Calibration | SCI-CAL-001 | 04_CALIBRATION_MASTER_RESOLUTION_SPEC |
| Astrometry | SCI-AST-001 | 05_WCS_COORDINATE_CONVENTION |
| Photometry | SCI-PHOT-001 | 06_PHOTOMETRY_CORRECTION_SPEC |
| PSF | SCI-PSF-001 | dynamic_psf 模块 |
| UPM 算法绑定 | ALG-UPM-FRAME-BIND-001 | 本包 PR_UPM_GATE（已冻结） |
| UPM 数据契约 | DATA-UPM-MODEL-001 | 本包 PR_UPM_GATE（已冻结） |

## 缺失 authority（S2 必须补）

1. API-* 族：AIO/public API 契约（现有 docs/contracts/PUBLIC_API.md 无 ID）
2. DATA-HIPS-* 族：HiPS signal/support/variance/ivar 数据契约
3. ENG-* 族：ownership/thread/error/IO/perf/comment 工程契约
4. ERR-* 族：错误码与 troubleshooting 映射（现有 ERROR_TAXONOMY.md 无 ID）
5. METRIC-* 族：性能/诊断指标 ID

## 判定

生产科学语义均有 authority ID（含新增 3 个 UPM 契约）；缺失项均为工程/数据
规范层（S2 补文档并注册 ID）。禁止修改 production code。
