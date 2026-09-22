# 国际标准冻结注册表（STANDARDS_REGISTRY）

> 上游权威: `ASTROCS_DESIGN.md` §5.3（冻结八投影）/附录 B（基础科学与格式参考）+ `ENGINEERING_SPEC.md` §8（本注册表为标准登记，不在 §0 权威链上）
> 条款锚的现行落点：投影集合 = `ASTROCS_DESIGN.md` §5.3（八投影）；资源门 = 同文 §8 + `eng/contracts/resource_gate_v1.json`
> 机器检查: `docs/standards/checks/check_standards_registry.py`（exit 0 = PASS / 1 = FAIL / 2 = ANCHOR_STALE；见 §5 与 §5 负向注入）
> 登记: `docs/DOCUMENT_INDEX.yaml` doc_index.active（status = ACTIVE_NORMATIVE）

---

## 1. 目的、权威与纪律

1. **唯一性**：本文件是仓库**唯一**的「领域 → 国际标准 → 版本 → 条款 → 符合性清单 →
   偏差指针」冻结注册表。任何科学/格式实现与外部标准的关系，以本文件登记为准；
   域级 SCI/ALG/DATA 文档承载公式与源码锚，本文件不复制公式、不改写任何 SCI/ALG 语义。
2. **禁止按实现反推标准**（跨域治理偏差 `STD-F6`「国际标准冻结注册表缺失」处置条款；
   其定义见本注册表 §3.2）：
   实现与标准不一致时，**一律登记偏差（§3）并按 finding 处理**，不得反向修改标准条款、
   不得把实现现状写成标准要求。
3. **版本冻结**：§2 域表的「冻结版本」字面量由本注册表冻结，机器检查器逐字比对
   （`C3_version_frozen_*`）——版本漂移（含以"最新版"含糊表述替代版本号）判 FAIL。
4. **符合状态取值域**（唯一合法四值，机器检查）：
   | 取值 | 语义 |
   |---|---|
   | `CONFORMANT` | 已按标准条款实现且有在位可执行证据（测试/oracle/工具） |
   | `PARTIAL` | 主体实现，存在已登记偏差或条款面未全覆盖（偏差指针必须非空） |
   | `NON_CONFORMANT` | 与标准条款冲突且未处置（必须登记偏差指针与处置归属） |
   | `PROJECT_DEFINED` | 标准未规定或本实现显式偏离标准之处，按 Project-defined 冻结（必须写明归属文档） |
5. **偏差指针纪律**：清单表「偏差」列与域字段 `DEVIATION` 必须指向已定义 ID。
   **偏差 ID 的闭包域 = 本注册表域偏差表 ∪ 本注册表 §3.2 跨域治理偏差表**；
   无外部 findings 登记册时，机器检查器显式登记
   `C6_external_findings_source`，不得静默返回空集；悬空指针判 FAIL
   （`C6_deviation_id_closure`）。
6. **变更流程**：新增/变更域、版本或条款映射必须走 `ENGINEERING_SPEC.md` §3 + `SCIENCE_CORRECTNESS.md` 变更流程并同步本文件 §2/§3
   与机器检查器冻结表；仅新增偏差指针（不改语义）由域内原子任务随实现提交更新。

---

## 2. 域表（冻结）

| 域 key | 标准 | 冻结版本 | 标准条款面 |
|---|---|---|---|
| spherical-projection | FITS WCS Paper I/II + SIP（Shupe et al. 2005） | Paper I = A&A 395, 1061 (2002)；Paper II = A&A 395, 1077 (2002)；SIP = ASPC 347, 491 (2005) | Paper I §2.1.1（CRPIX 1-based）/§3（CD/CTYPE）；Paper II §2.1（旋转与 LONPOLE）/§5 Table 1（TAN/SIN/CAR/AIT）；SIP §A（A/B/AP/BP 约定） |
| hips | IVOA HiPS Recommendation 1.0（properties 修订 1.4） | HiPS 1.0 (PR-HiPS-1.0-20161122) + properties hips_version="1.4" | HiPS 1.0 §3（层级索引与目录结构）/§4.1（tile）/§4.2.1（properties）/§4.4.1（all-sky map）/§6.3.1（客户端绘制）；properties 1.4 键集 |
| healpix | Górski et al. 2005 HEALPix（NESTED） | ApJ 622, 759 (2005)，bibcode 2005ApJ...622..759G | §5.1（nside=2^order 等面积单元）/§5.2（NESTED 编号与父子关系）/§5.3（ang2pix/pix2ang） |
| drizzle | Fruchter & Hook 2002 Drizzle | PASP 114, 144 (2002)，bibcode 2002PASP..114..144F | §2（drop 与 pixfrac）/§3（线性重建与权重 w_jp=a_jp/A_pixel）/§4（欠采样图像重建） |
| catalog | Gaia DR3 data model（本地 XPSD 星表） | Gaia DR3（Gaia Collaboration et al. 2023, A&A 674, A1）+ XPSD 本地编码合同 | DR3 source 列面（ra/dec 参考历元 J2016.0、phot_g_mean_mag/phot_bp_mean_mag/phot_rp_mean_mag）；本地 XPSD 记录布局（ALG-GAIA-001 §2） |
| fits | FITS Standard 4.0 | FITS 4.0（IAU FWG，2016-07-22 批准版） | §3.1（基本文件结构/80 字节卡）/§4.2（SIMPLE/BITPIX/NAXIS 基本头）/§4.4（扩展 HDU）/§5（表扩展）/§6（DATASUM/CHECKSUM 校验和） |

---

## D.spherical-projection — 球面投影与 WCS（FITS WCS Paper I/II + SIP）

- DOMAIN: spherical-projection
- STANDARD: FITS WCS Paper I/II + SIP（Shupe et al. 2005）
- VERSION: Paper I = A&A 395, 1061 (2002)；Paper II = A&A 395, 1077 (2002)；SIP = ASPC 347, 491 (2005)
- CLAUSES: Paper I §2.1.1（CRPIX 1-based）/§3（CD/CTYPE）；Paper II §2.1（旋转/LONPOLE）/§5 Table 1（TAN/SIN/CAR/AIT）；SIP §A（A/B/AP/BP）
- COMPLIANCE: PARTIAL
- EVIDENCE: docs/science/ASTROMETRY.md；docs/algorithms/PLATESOLVE.md；docs/algorithms/PHASE3_PROJ_IMPL.md；docs/algorithms/PHASE3_FITS_IMPL.md；eng/tests/unit/p1wcs/；eng/tests/unit/p3_projection_test.cpp；lib/algorithms/projection/p3_wcs.cpp；lib/algorithms/platesolve/cpp/ipv/src/ipv_wcs.cpp
- DEVIATION: STD-F1；DISP-WCS-001；DISP-WCS-008；DISP-P3PROJ-001

| 条款 | 标准要求 | 符合状态 | 证据指针 | 偏差 |
|---|---|---|---|---|
| Paper I §2.1.1（CRPIX 1-based 参考像素） | 参考像素 CRPIX 为 1-based，像素坐标 `xp = x + 1` | CONFORMANT | docs/science/ASTROMETRY.md；docs/algorithms/PLATESOLVE.md；eng/tests/unit/p1wcs/p1wcs_astropy_cross.py；eng/tests/unit/p1wcs/p1wcs_std_f1_bridge_cross.py；lib/algorithms/projection/tests/p3wcs/p3_wcs_test.cpp | STD-F1（已闭环：ipv 求解器拟合自变量为 sdet 半整数像素中心、输出 `x=u+CRPIX` 即 1-based FITS `p`，与 Paper I §2.1.1 逐式一致；产品网格/数组下标→FITS 的单次 +1 换算在 Phase3 导出边界与 p1_wcs.json 写出侧；实测 astropy 交叉 5.7e-14 deg、九宫格 18 格无 1px 偏移、负向注入必败；见 §3 偏差索引与 FIX-SCI-WCS-001） |
| Paper I §3（CD/CTYPE 关键词体系） | 线性变换以 CD 矩阵 + CTYPE 表达；FITS 头卡 ≤80 字节 | CONFORMANT | docs/algorithms/PHASE3_PROJ_IMPL.md；lib/algorithms/projection/p3_wcs.cpp；eng/tests/unit/p3_projection_test.cpp | 无（T5/T7 断言在位） |
| Paper II §5 Table 1（TAN/SIN/CAR/AIT 四投影） | 四投影按 Table 1 的 R_θ 定义实现，新增投影须注册并附独立往返 Oracle | CONFORMANT | docs/algorithms/PHASE3_PROJ_IMPL.md；eng/tests/unit/p3_projection_test.cpp；eng/tests/backend/test_p3_projection_oracle.py | 无（registry v1 恰四行；T1/T2 往返与独立解析解在位） |
| Paper II §2.1（LONPOLE 与旋转） | 允许通用 LONPOLE/φ_p 附加旋转机制 | PROJECT_DEFINED | docs/algorithms/PHASE3_PROJ_IMPL.md | 无（本实现固定 θ₀=+90°、无 φ_p 附加旋转，显式冻结为 Project-defined；不实现通用 LONPOLE） |
| SIP §A（A/B 前向、AP/BP 逆向与单位线性剔除） | SIP 畸变系数约定与单位线性项处理 | PROJECT_DEFINED | docs/science/ASTROMETRY.md；docs/algorithms/PLATESOLVE.md | DISP-WCS-008（AP/BP 采样网格 ≥7×7，实现 41×41/81×81 + 迭代反演；7×7 自证门不成立） |
| Paper I/II parity 与手性（det(CD) 符号、east_left/east_right） | 像素手性由 CD 行列式符号表达，翻转不得改变 abs(det(CD)) | CONFORMANT | docs/science/ASTROMETRY.md；docs/algorithms/PHASE3_PROJ_IMPL.md；eng/tests/unit/p3_projection_test.cpp | 无（T3/T5 含 det<0 与 crpix 奇偶双例 bitwise 断言） |
| 退化语义（CD det→0 禁坍缩冒充解） | 退化线性变换不得产生伪 WCS | PARTIAL | docs/algorithms/PLATESOLVE.md；lib/algorithms/platesolve/cpp/ipv/src/ipv_wcs.cpp；eng/tests/unit/p1wcs/p1wcs_tests_negative.cpp | DISP-WCS-001（CD 退化静默坍缩，负面用例已覆盖） |

### D.spherical-projection 偏差表

| 偏差 ID | 严重度 | 指针 | 处置归属 |
|---|---|---|---|
| STD-F1 | 已闭环（原高/P1） | docs/science/ASTROMETRY.md | STD-F1-ADJ（订正标签 FIX-SCI-WCS-001：ipv 求解器输出 `x=u+CRPIX` 即 1-based FITS `p`、与 Paper I 逐式一致；产品网格/数组下标→FITS 的单次 +1 换算在 Phase3 导出边界与 p1_wcs.json 写出侧；实测 astropy 交叉 5.7e-14 deg / 九宫格 18 格无 1px 偏移 / 负向注入必败） |
| DISP-WCS-001 | 中 | docs/algorithms/PLATESOLVE.md | P1-WCS-IMPL（CD 退化静默坍缩，负面用例已在位） |
| DISP-WCS-008 | 低 | docs/algorithms/PLATESOLVE.md；docs/science/ASTROMETRY.md | P1-WCS-IMPL（网格/阶扩展与迭代反演已落地，SCI 口径已同步） |
| DISP-P3PROJ-001 | 中 | docs/algorithms/PHASE3_PROJ_IMPL.md | P3-PROJ-IMPL / P3-PROJ-INT（PA 未接线：会话恒传 rotation_pa_deg=0.0；astrocs_p3_projection.dll 未建 entrypoint=MISSING） |

> WCS `1e-4 px` 冻结门以 41×41/81×81 AP/BP 布局扩展 + 消费方迭代反演达成
> （本机实测 3.3e-10~2.9e-9 px），保留高畸变 fixture。

---

## D.hips — IVOA HiPS 1.0（properties 修订 1.4）

- DOMAIN: hips
- STANDARD: IVOA HiPS Recommendation 1.0（properties 修订 1.4）
- VERSION: HiPS 1.0 (PR-HiPS-1.0-20161122) + properties hips_version="1.4"
- CLAUSES: HiPS 1.0 §3（层级索引与目录结构）/§4.1（tile）/§4.2.1（properties）/§4.4.1（all-sky map）/§6.3.1（客户端绘制）；properties 1.4 键集
- COMPLIANCE: PARTIAL
- EVIDENCE: docs/algorithms/HIPS_WRITER.md；docs/science/PHASE3_HIPS_TO_FITS.md；docs/interfaces/io/IO_002_HIPS_INPUT_INTERFACE.md；docs/contracts/PUBLIC_API.md；lib/infrastructure/aio/src/hips/aio_hips_writer.cpp；eng/tests/unit/CMakeLists.txt
- DEVIATION: STD-F4；DISP-HIPS-001；DISP-HIPS-002；DISP-HIPS-003；DISP-HIPS-004；DISP-HIPS-005；DISP-HIPS-006；DISP-HIPS-007；DISP-HIPS-008；DISP-HIPS-009；DISP-HIPS-010；DISP-HIPS-011；DISP-HIPS-012

| 条款 | 标准要求 | 符合状态 | 证据指针 | 偏差 |
|---|---|---|---|---|
| §3（层级索引与 NorderK/DirD/NpixN 目录结构） | 层级 tile 目录命名与 NESTED 地址编码固定 | CONFORMANT | docs/interfaces/io/IO_002_HIPS_INPUT_INTERFACE.md；lib/infrastructure/aio/src/hips/aio_hips_writer.cpp；eng/tests/unit/CMakeLists.txt | 无（§4.1 标准式 D=(ipix/10000)*10000、Npix=ipix；Dir=商/Npix=余数仅作只读回退） |
| §4.1（tile 为 W×W FITS 单元，tile_width=512） | tile 宽为 2 的幂、标准 512；tile 内 NESTED 序 | CONFORMANT | docs/algorithms/HIPS_WRITER.md；eng/tests/unit/CMakeLists.txt | DISP-HIPS-006；DISP-HIPS-008（写路径无互斥包装；兼容入口 8bit support 语义并存） |
| §4.2.1（properties 必需键集） | `hips_version/hips_order/hips_tile_width/hips_tile_format/hips_frame` 必需且自洽 | PARTIAL | docs/interfaces/io/IO_002_HIPS_INPUT_INTERFACE.md；lib/infrastructure/aio/src/hips/aio_hips_writer.cpp | STD-F4（写出侧键集缺 em_min/em_max/obs_bandpass 等推荐键；META-002 禁伪造，待 index.json 真实滤镜元数据接通） |
| §4.2.1（properties 可选/推荐键：hips_status/hips_estsize/hips_initial_fov） | 可选键存在时须自洽、非占位 | PARTIAL | docs/algorithms/HIPS_WRITER.md；lib/infrastructure/aio/src/hips/aio_hips_writer.cpp | DISP-HIPS-002（hips_estsize="1000000"、hips_initial_fov="60" 硬编码占位）；DISP-HIPS-003（hips_status 恒 "private master"） |
| §4.4.1（all-sky map 与 MOC 关系） | 覆盖由 MOC 表达；低阶像素=子像素聚合 | CONFORMANT | docs/algorithms/HIPS_WRITER.md；lib/infrastructure/aio/src/hips/aio_hips_writer.cpp | DISP-HIPS-005（moc_order 静默 clamp；低阶 UNIQ 对自家 reader 无效，Moc.fits 为 optional hint） |
| §6.3.1（客户端绘制所需的初始视场/像素尺度元数据） | 提供 `hips_pixel_scale`/`hips_initial_fov` 等客户端键 | CONFORMANT | lib/infrastructure/aio/src/hips/aio_hips_writer.cpp；docs/algorithms/HIPS_WRITER.md | DISP-HIPS-012（FIRSTPIX/LASTPIX 声明性头卡无消费方） |
| properties 1.4（hips_version="1.4" 与 hierarchy 聚合） | 1.4 修订的 properties 版本字面量与层级聚合语义 | CONFORMANT | lib/infrastructure/aio/src/hips/aio_hips_writer.cpp；docs/algorithms/HIPS_WRITER.md；docs/contracts/PUBLIC_API.md | DISP-HIPS-009（f32 产品层级累加为 float 求和，多子 tile 舍入漂移） |

### D.hips 偏差表

| 偏差 ID | 严重度 | 指针 | 处置归属 |
|---|---|---|---|
| STD-F4 | 中（P2） | docs/interfaces/io/IO_002_HIPS_INPUT_INTERFACE.md | HiPS 域原子任务（以 testdata/index.json 真实滤镜元数据接通 em_min/em_max/obs_bandpass，不得臆造） |
| DISP-HIPS-001 | 高 | docs/algorithms/HIPS_WRITER.md | P1-HIPS-IMPL / P1-HIPS-INT（abort 不删除已写文件，无 rollback） |
| DISP-HIPS-002 | 中 | docs/algorithms/HIPS_WRITER.md | P1-HIPS-IMPL（hips_estsize/hips_initial_fov 硬编码占位） |
| DISP-HIPS-003 | 低 | docs/algorithms/HIPS_WRITER.md | P1-HIPS-IMPL（hips_status 恒值未参数化） |
| DISP-HIPS-004 | 高 | docs/algorithms/HIPS_WRITER.md；docs/interfaces/io/IO_003_ATOMIC_OUTPUT_PUBLISH.md | P1-HIPS-INT（C++ 写出无原子发布；原子语义由 IO-003 发布层承接） |
| DISP-HIPS-005 | 低/中 | docs/algorithms/HIPS_WRITER.md | P1-HIPS-IMPL（moc_order 静默 clamp 与读侧兼容性） |
| DISP-HIPS-006 | 中 | docs/algorithms/HIPS_WRITER.md | P1-HIPS-IMPL（CFITSIO 写路径无进程级互斥包装） |
| DISP-HIPS-007 | 低 | docs/algorithms/HIPS_WRITER.md | P1-HIPS-IMPL（错误码无集中枚举、正负混用） |
| DISP-HIPS-008 | 低 | docs/algorithms/HIPS_WRITER.md | P1-HIPS-IMPL（兼容入口 8bit support 旧语义并存） |
| DISP-HIPS-009 | 中 | docs/algorithms/HIPS_WRITER.md | P1-HIPS-IMPL（f32 产品层级累加精度边界） |
| DISP-HIPS-010 | 低 | docs/algorithms/HIPS_WRITER.md | P1-HIPS-IMPL（FITS 头字符串 68 字符静默截断；properties 值无转义） |
| DISP-HIPS-011 | 低 | docs/algorithms/HIPS_WRITER.md | P1-HIPS-IMPL（空 acc 照写全 NaN hierarchy tile；缓冲未复用） |
| DISP-HIPS-012 | 低 | docs/algorithms/HIPS_WRITER.md | P1-HIPS-IMPL（FIRSTPIX/LASTPIX 声明性头卡无消费方） |

---

## D.healpix — HEALPix NESTED（Górski et al. 2005）

- DOMAIN: healpix
- STANDARD: Górski et al. 2005 HEALPix（NESTED）
- VERSION: ApJ 622, 759 (2005)，bibcode 2005ApJ...622..759G
- CLAUSES: §5.1（nside=2^order 等面积单元）/§5.2（NESTED 编号与父子关系）/§5.3（ang2pix/pix2ang）
- COMPLIANCE: CONFORMANT
- EVIDENCE: docs/algorithms/HEALPIX_MAPPING.md；docs/algorithms/HIPS_WRITER.md；lib/algorithms/shared/healpix/healpix_core.cpp；lib/algorithms/shared/healpix/tests；eng/tests/unit/CMakeLists.txt
- DEVIATION: 无域内偏差（HEALPix 核心零偏差；层级聚合精度边界归 HiPS 域 DISP-HIPS-009）

| 条款 | 标准要求 | 符合状态 | 证据指针 | 偏差 |
|---|---|---|---|---|
| §5.1（nside=2^order，等面积单元 A=4π/(12·nside²)） | 单元面积公式与 nside 幂次关系 | CONFORMANT | lib/algorithms/shared/healpix/healpix_core.cpp；docs/algorithms/HIPS_WRITER.md；eng/tests/unit/CMakeLists.txt | 无 |
| §5.2（NESTED 编号与 4 分叉父子关系） | NESTED 索引与父子位移关系 child = 4·parent + k | CONFORMANT | lib/algorithms/shared/healpix/healpix_core.cpp；lib/algorithms/shared/healpix/tests/test_healpix_neighbors.cpp；eng/tests/unit/CMakeLists.txt | 无（tile_shift=9、mask=(1<<18)-1 不变量在位） |
| §5.3（ang2pix/pix2ang 往返） | 球面角 ↔ NESTED 索引往返在 FP64 下达机器精度 | CONFORMANT | lib/algorithms/shared/healpix/tests/test_healpix_oracle.cpp；lib/algorithms/shared/healpix/tests/snr_hips_spatial_oracle.py；docs/algorithms/HEALPIX_MAPPING.md | 无（往返 ≤1e-12 deg；astropy-healpix 百万点 oracle 对拍） |
| §5.2/§5.3（order 上限与溢出收口） | order ≤ 29 且越界输入显式拒绝 | CONFORMANT | lib/algorithms/shared/healpix/healpix_core.cpp；lib/algorithms/shared/healpix/tests/test_healpix_neighbors.cpp | 无（checked 收口，禁静默溢出） |
| 单源纪律（B4-01 去重） | 全仓唯一 HEALPix 权威实现，重复实现须机器门禁 | CONFORMANT | lib/algorithms/shared/healpix/healpix_core.h；docs/algorithms/HEALPIX_MAPPING.md | 无（兼容 shim + 机器门禁登记在位） |

### D.healpix 偏差表

| 偏差 ID | 严重度 | 指针 | 处置归属 |
|---|---|---|---|
| （无） | — | HEALPix 核心实现与 Górski 2005 NESTED 条款无偏差；下游层级聚合精度边界登记在 HiPS 域 | 域内无待处置项（DISP-HIPS-009 归 HiPS 域） |

---

## D.drizzle — Drizzle 线性重建（Fruchter & Hook 2002）

- DOMAIN: drizzle
- STANDARD: Fruchter & Hook 2002 Drizzle
- VERSION: PASP 114, 144 (2002)，bibcode 2002PASP..114..144F
- CLAUSES: §2（drop 与 pixfrac）/§3（线性重建与权重 w_jp=a_jp/A_pixel）/§4（欠采样图像重建）
- COMPLIANCE: PARTIAL
- EVIDENCE: docs/science/DRIZZLE.md；docs/algorithms/DRIZZLE_GEOMETRY.md；lib/algorithms/drizzle/healpix_drizzle/tests/candidate_oracle_test.cpp；lib/algorithms/drizzle/healpix_drizzle/tests/p1drz
- DEVIATION: DISP-DRZ-001；DISP-DRZ-002；DISP-DRZ-003；**DISP-DRZ-004**；DISP-DRZ-005；DISP-DRZ-006；DISP-DRZ-007；DISP-DRZ-008；DISP-DRZ-009
  （**`DISP-DRZ-004` = TRACKED/OPEN**：现行实现为值 NaN 经 `F_p` **传播、不掩膜**，
  与生产口径不符。**唯一口径 = rule_id `NAN-SAMPLE-MASK-COVERAGE-NAN`**（正本 =
  `docs/interfaces/data/DATA-002_PHASE_PRODUCT_EXCHANGE.md` §2a `invalid_handling` 块：
  样本级掩膜 + 重归一 + 覆盖级 NaN + 强制计数 `n_rejected_nonfinite`；
  `docs/standards/NUMERIC_STANDARD.md` §MUST 引用同一份文字，**不得两套**）。
  处置 = 实现侧改为掩膜（P1-DRZ-IMPL）。）

| 条款 | 标准要求 | 符合状态 | 证据指针 | 偏差 |
|---|---|---|---|---|
| §2（drop 与 pixfrac 收缩因子） | drop 为源像素按 pixfrac 收缩后的足迹，pixfrac∈(0,1] | PARTIAL | docs/science/DRIZZLE.md；docs/algorithms/DRIZZLE_GEOMETRY.md；lib/algorithms/drizzle/healpix_drizzle/tests/candidate_oracle_test.cpp | DISP-DRZ-003（API 层接受 pixfrac=0.0，引擎层拒绝——两层双轨） |
| §3（线性重建 w_jp = a_jp / A_pixel 与面亮度语义） | 一致加权均值，drop 面积在分子分母相消；每像素常量 ADU ⇒ S=C/A_pixel | PARTIAL | docs/science/DRIZZLE.md；docs/algorithms/DRIZZLE_GEOMETRY.md；lib/algorithms/drizzle/healpix_drizzle/tests/p1drz | DISP-DRZ-009（源码归一为 `w=a/A_drop`，pixfrac<1 的绝对面亮度偏 1/pixfrac²）；DISP-DRZ-002（面积实现为 S-H 裁剪+Eriksson 扇形剖分，非 Girard 定理，文档措辞已登记） |
| §3（球面交叠面积与微小 drop 数值路径） | 交叠面积计算须数值稳定 | PROJECT_DEFINED | docs/algorithms/DRIZZLE_GEOMETRY.md | DISP-DRZ-005（角跨度 <1e-3 rad 时切平面分支为真路径，注释论证偏差 <4e-8；禁删） |
| §4（欠采样重建与候选枚举完备性） | 重建须覆盖全部候选源像素，零漏选 | CONFORMANT | lib/algorithms/drizzle/healpix_drizzle/tests/candidate_oracle_test.cpp；docs/algorithms/DRIZZLE_GEOMETRY.md | 无（9003 例全枚举 false_negative=0：4 pixfrac × 5 尺度 × 7 nside × RA 跨 0 × 极区 × face 边界） |
| §3（方差/权重传播确定性） | 重建为线性加权，须确定性可复现 | PARTIAL | docs/science/DRIZZLE.md；docs/algorithms/DRIZZLE_GEOMETRY.md；docs/standards/NUMERIC_STANDARD.md | **DISP-DRZ-004（TRACKED/OPEN）**：现行实现为值 NaN 经 `F_p` **传播、不掩膜**（`docs/science/DRIZZLE.md:116`；`drizzle_engine.cpp:1898-1902`；回归 `p1drz_tests_core.cpp:517-537`），与生产口径 `NAN-SAMPLE-MASK-COVERAGE-NAN`（样本级掩膜 + 覆盖级 NaN + 强制计数）不符 ⇒ 待实现侧整改（P1-DRZ-IMPL）；DISP-DRZ-007（方差锚行号漂移） |
| §2/§3（SIP 畸变场下的 drop 映射） | 源像素角点经 WCS 映射到球面多边形 | PARTIAL | docs/algorithms/DRIZZLE_GEOMETRY.md；lib/algorithms/drizzle/healpix_drizzle/tests | DISP-DRZ-001（SIP 阶数校验 [0,5] 与注释 0..4 不符） |

### D.drizzle 偏差表

| 偏差 ID | 严重度 | 指针 | 处置归属 |
|---|---|---|---|
| DISP-DRZ-001 | 低 | docs/algorithms/DRIZZLE_GEOMETRY.md | P1-DRZ-IMPL（SIP 阶数注释与校验不一致） |
| DISP-DRZ-002 | 低 | docs/algorithms/DRIZZLE_GEOMETRY.md | P1-DRZ-IMPL（面积算法文档措辞 vs 实现） |
| DISP-DRZ-003 | 中 | docs/algorithms/DRIZZLE_GEOMETRY.md | P1-DRZ-IMPL（pixfrac 双轨边界） |
| DISP-DRZ-004 | **高（P1）** | docs/science/DRIZZLE.md；docs/algorithms/DRIZZLE_GEOMETRY.md；docs/standards/NUMERIC_STANDARD.md | **TRACKED/OPEN**：现行实现为值 NaN 经 `F_p` **传播、不掩膜**（`docs/science/DRIZZLE.md:116`；`drizzle_engine.cpp:1898-1902`；回归 `lib/algorithms/drizzle/healpix_drizzle/tests/p1drz/p1drz_tests_core.cpp:517-537` `p1drz_negative`）。**唯一口径 = rule_id `NAN-SAMPLE-MASK-COVERAGE-NAN`**（正本 = `docs/interfaces/data/DATA-002_PHASE_PRODUCT_EXCHANGE.md` §2a `invalid_handling`：样本级掩膜 + 重归一 + 覆盖级 NaN + 强制计数 `n_rejected_nonfinite`；`NUMERIC_STANDARD.md` §MUST 引用同一份文字）。处置 = 实现侧由「传播」改「掩膜」+ 补计数暴露（P1-DRZ-IMPL） |
| DISP-DRZ-005 | 中 | docs/algorithms/DRIZZLE_GEOMETRY.md | P1-DRZ-IMPL / P1-DRZ-INT（微小 drop 切平面真路径须守护，禁按旧登记删除） |
| DISP-DRZ-006 | 低 | docs/algorithms/DRIZZLE_GEOMETRY.md | P1-DRZ-IMPL（累加器字段数注释漂移） |
| DISP-DRZ-007 | 低 | docs/algorithms/DRIZZLE_GEOMETRY.md | P1-DRZ-IMPL（方差锚行号漂移） |
| DISP-DRZ-008 | 低 | docs/algorithms/DRIZZLE_GEOMETRY.md | P1-DRZ-IMPL（PolyClip 零调用） |
| DISP-DRZ-009 | **高** | docs/science/DRIZZLE.md；docs/algorithms/DRIZZLE_GEOMETRY.md | P1-DRZ-IMPL（源码归一 `w=a/A_drop` vs SCI 目标态面亮度保持 `w=a/A_pixel`；pixfrac<1 偏 1/pixfrac²，FIX-SCI-DRZ-001） |

---

## D.catalog — Gaia DR3 data model（本地 XPSD 星表）

- DOMAIN: catalog
- STANDARD: Gaia DR3 data model（本地 XPSD 星表）
- VERSION: Gaia DR3（Gaia Collaboration et al. 2023, A&A 674, A1）+ XPSD 本地编码合同
- CLAUSES: DR3 source 列面（ra/dec 参考历元 J2016.0、phot_g_mean_mag/phot_bp_mean_mag/phot_rp_mean_mag）；本地 XPSD 记录布局（ALG-GAIA-001 §2）
- COMPLIANCE: PARTIAL
- EVIDENCE: docs/algorithms/GAIA_QUERY.md；docs/modules/gaia_xpsd_client.md；docs/science/ASTROMETRY.md；lib/infrastructure/gaia_xpsd_client/src/gaia_client.c；eng/tests/unit/CMakeLists.txt
- DEVIATION: DISP-GAIA-001

| 条款 | 标准要求 | 符合状态 | 证据指针 | 偏差 |
|---|---|---|---|---|
| DR3 source 位置列（ra/dec，ICRS，参考历元 J2016.0） | 位置以 ICRS 表达，RA∈[0,360)、Dec∈[-90,90] | CONFORMANT | docs/algorithms/GAIA_QUERY.md；docs/science/ASTROMETRY.md；lib/infrastructure/gaia_xpsd_client/src/gaia_client.c | 无（RA 归一到 [0,360)；frame 契约与 SCI-WCS-001 §3a 一致） |
| DR3 测光列（phot_g_mean_mag / phot_bp_mean_mag / phot_rp_mean_mag） | G/BP/RP 星等语义与量化解码 | PARTIAL | docs/algorithms/GAIA_QUERY.md；lib/infrastructure/gaia_xpsd_client/src/gaia_client.c | DISP-GAIA-001（本地 XPSD 以 uint16×0.001−1.5 量化表达；非官方 archive 数据模型，仓库无版本化 DR3 data model 文档） |
| DR3 source 列面完备性（source_id 等主键列） | 星表主键与列面可追溯 | NON_CONFORMANT | docs/algorithms/GAIA_QUERY.md；docs/modules/gaia_xpsd_client.md | DISP-GAIA-001（本地 XPSD 仅存位置/星等/光谱子集，无 source_id 主键列；跨表身份靠位置匹配） |
| XPSD 本地编码（2 µas/LSB 位置量化、10 µas/LSB dra、0.001 mag 星等） | 本地编码须与标准列语义无损对应并写明换算 | CONFORMANT | docs/algorithms/GAIA_QUERY.md；lib/infrastructure/gaia_xpsd_client/src/gaia_client.c | 无（2 µas/LSB 位置量化按实测锚登记） |
| DR3SP 光谱量化解码（F(λ)=byte·fluxMul+fluxMin） | 光谱量化残差须量化登记 | PARTIAL | docs/algorithms/GAIA_QUERY.md；docs/modules/gaia_xpsd_client.md | DISP-GAIA-001（8-bit 量化残差 median 0.21%/p95 1.8%，属本地编码损失） |
| 查询锥与星等窗语义（角距 ≤ρ、m_lo≤m_G≤m_hi 闭区间） | 球面角距定义与闭区间边界 | CONFORMANT | docs/algorithms/GAIA_QUERY.md；eng/tests/unit/CMakeLists.txt | 无（含极区/跨 RA=0 边界用例） |

### D.catalog 偏差表

| 偏差 ID | 严重度 | 指针 | 处置归属 |
|---|---|---|---|
| DISP-GAIA-001 | 中 | docs/algorithms/GAIA_QUERY.md；docs/modules/gaia_xpsd_client.md | catalog 域原子任务（登记 Gaia DR3 data model 版本化列面映射：官方列 → XPSD 本地字段，并显式声明 source_id 主键缺失与量化损失） |

---

## D.fits — FITS Standard 4.0

- DOMAIN: fits
- STANDARD: FITS Standard 4.0
- VERSION: FITS 4.0（IAU FWG，2016-07-22 批准版）
- CLAUSES: §3.1（基本文件结构/80 字节卡）/§4.2（SIMPLE/BITPIX/NAXIS 基本头）/§4.4（扩展 HDU）/§5（表扩展）/§6（DATASUM/CHECKSUM）
- COMPLIANCE: PARTIAL
- EVIDENCE: docs/interfaces/io/IO_001_FITS_STREAM_INTERFACE.md；docs/algorithms/PHASE3_FITS_IMPL.md；docs/contracts/DATA_SEMANTICS.md；lib/infrastructure/aio/io/include/astrocs/io/fits_stream_v1.h；eng/tests/io/test_fits_stream_contract.py
- DEVIATION: DISP-FITS-001

| 条款 | 标准要求 | 符合状态 | 证据指针 | 偏差 |
|---|---|---|---|---|
| §3.1（基本文件结构：80 字节卡、END、2880 字节块） | header 卡固定 80 字节、以 END 结束、按 2880 字节补齐 | CONFORMANT | docs/interfaces/io/IO_001_FITS_STREAM_INTERFACE.md；eng/tests/io/test_fits_stream_contract.py | 无（含非法 header/END 缺失负面用例） |
| §4.2（SIMPLE/BITPIX/NAXIS 基本头与基本图像 HDU） | 基本 HDU 头卡合法且维度一致 | CONFORMANT | docs/interfaces/io/IO_001_FITS_STREAM_INTERFACE.md；docs/algorithms/PHASE3_FITS_IMPL.md；eng/tests/io/test_fits_stream_contract.py | 无（NAXIS≥0、≤3；dtype/shape 失配显式拒绝） |
| §4.4/§5（扩展 HDU 与表扩展） | 扩展 HDU/BINTABLE 结构与 EXTNAME/BUNIT 语义 | PARTIAL | docs/algorithms/PHASE3_FITS_IMPL.md；eng/tests/io/test_fits_stream_contract.py | DISP-FITS-001（扩展 HDU 面按产品子集实现：仅登记 EXTNAME/BUNIT/DATASUM 面，未覆盖通用表扩展全集） |
| §6（DATASUM/CHECKSUM 校验和） | 数据与头校验和须可复算、校验失败显式报错 | CONFORMANT | docs/interfaces/io/IO_001_FITS_STREAM_INTERFACE.md；docs/contracts/DATA_SEMANTICS.md；eng/tests/io/test_fits_stream_contract.py | 无（内容哈希流式重算复核在位） |
| §3.1/§4.2（错误语义：截断/坏头/不支持位深） | 违规输入显式错误码，禁静默降级 | CONFORMANT | docs/interfaces/io/IO_001_FITS_STREAM_INTERFACE.md；lib/infrastructure/aio/io/include/astrocs/io/fits_stream_v1.h；eng/tests/io/test_fits_stream_contract.py | 无（ACS_FIO_ERR_* 17 码，含 TRUNCATED/BAD_HEADER/UNSUPPORTED） |
| §4.2（BITPIX 与像素中心/值域语义） | 位深与数据类型显式，单位与 BUNIT 一致 | PARTIAL | docs/contracts/DATA_SEMANTICS.md；docs/algorithms/PHASE3_FITS_IMPL.md | DISP-FITS-001（科学产品的 BITPIX/BUNIT 面按 Phase 子集登记，全通用位深面归 IO 域后续任务） |

### D.fits 偏差表

| 偏差 ID | 严重度 | 指针 | 处置归属 |
|---|---|---|---|
| DISP-FITS-001 | 低 | docs/interfaces/io/IO_001_FITS_STREAM_INTERFACE.md；docs/contracts/DATA_SEMANTICS.md | IO 域原子任务（扩展 HDU/表扩展与全通用位深面按 FITS 4.0 全集收敛；当前为显式产品子集，非静默偏差） |

---

## 3. 偏差索引（域 × 条款 → 偏差 ID → 处置归属）

| 偏差 ID | 域 | 条款 | 注册表清单行 | 状态 | 处置归属 |
|---|---|---|---|---|---|
| STD-F1 | spherical-projection | Paper I §2.1.1（CRPIX 1-based 参考像素） | 第 1 行 | CONFORMANT（求解器输出即 1-based FITS `p`；产品网格/数组下标→FITS 单次 +1 在导出边界，实测 astropy 交叉 5.7e-14 deg） | STD-F1-ADJ |
| DISP-WCS-001 | spherical-projection | 退化语义（CD det→0 禁坍缩冒充解） | 第 7 行 | TRACKED | P1-WCS-IMPL |
| DISP-WCS-008 | spherical-projection | SIP §A（A/B 前向、AP/BP 逆向与单位线性剔除） | 第 5 行 | TRACKED | P1-WCS-IMPL |
| DISP-P3PROJ-001 | spherical-projection | Paper II §5 Table 1（TAN/SIN/CAR/AIT 四投影） | 第 3 行 | TRACKED | P3-PROJ-IMPL / P3-PROJ-INT |
| STD-F4 | hips | §4.2.1（properties 必需键集） | 第 3 行 | OPEN（下一轮域任务） | HiPS 域原子任务 |
| DISP-HIPS-001 | hips | properties 1.4（hips_version="1.4" 与 hierarchy 聚合） | 第 7 行 | TRACKED | P1-HIPS-IMPL / P1-HIPS-INT |
| DISP-HIPS-002 | hips | §4.2.1（properties 可选/推荐键：hips_status/hips_estsize/hips_initial_fov） | 第 4 行 | TRACKED | P1-HIPS-IMPL |
| DISP-HIPS-003 | hips | §4.2.1（properties 可选/推荐键：hips_status/hips_estsize/hips_initial_fov） | 第 4 行 | TRACKED | P1-HIPS-IMPL |
| DISP-HIPS-004 | hips | §4.4.1（all-sky map 与 MOC 关系） | 第 5 行 | TRACKED | P1-HIPS-INT |
| DISP-HIPS-005 | hips | §4.4.1（all-sky map 与 MOC 关系） | 第 5 行 | TRACKED | P1-HIPS-IMPL |
| DISP-HIPS-006 | hips | §4.1（tile 为 W×W FITS 单元，tile_width=512） | 第 2 行 | TRACKED | P1-HIPS-IMPL |
| DISP-HIPS-007 | hips | §4.2.1（properties 必需键集） | 第 3 行 | TRACKED | P1-HIPS-IMPL |
| DISP-HIPS-008 | hips | §4.1（tile 为 W×W FITS 单元，tile_width=512） | 第 2 行 | TRACKED | P1-HIPS-IMPL |
| DISP-HIPS-009 | hips | properties 1.4（hips_version="1.4" 与 hierarchy 聚合） | 第 7 行 | TRACKED | P1-HIPS-IMPL |
| DISP-HIPS-010 | hips | §4.2.1（properties 必需键集） | 第 3 行 | TRACKED | P1-HIPS-IMPL |
| DISP-HIPS-011 | hips | §4.4.1（all-sky map 与 MOC 关系） | 第 5 行 | TRACKED | P1-HIPS-IMPL |
| DISP-HIPS-012 | hips | §6.3.1（客户端绘制所需的初始视场/像素尺度元数据） | 第 6 行 | TRACKED | P1-HIPS-IMPL |
| DISP-DRZ-001 | drizzle | §2/§3（SIP 畸变场下的 drop 映射） | 第 6 行 | TRACKED | P1-DRZ-IMPL |
| DISP-DRZ-002 | drizzle | §3（线性重建 w_jp = a_jp / A_pixel 与面亮度语义） | 第 2 行 | TRACKED | P1-DRZ-IMPL |
| DISP-DRZ-009 | drizzle | §3（线性重建 w_jp = a_jp / A_pixel 与面亮度语义） | 第 2 行 | TRACKED | P1-DRZ-IMPL |
| DISP-DRZ-003 | drizzle | §2（drop 与 pixfrac 收缩因子） | 第 1 行 | TRACKED | P1-DRZ-IMPL |
| DISP-DRZ-004 | drizzle | §3（方差/权重传播确定性） | 第 5 行 | **TRACKED** | P1-DRZ-IMPL（现行实现为值 NaN 经 `F_p` 传播、不掩膜；口径 = 掩膜，rule_id `NAN-SAMPLE-MASK-COVERAGE-NAN`（正本 = DATA-002 §2a `invalid_handling`），实现侧待改） |
| DISP-DRZ-005 | drizzle | §3（球面交叠面积与微小 drop 数值路径） | 第 3 行 | TRACKED | P1-DRZ-IMPL / P1-DRZ-INT |
| DISP-DRZ-006 | drizzle | §2（drop 与 pixfrac 收缩因子） | 第 1 行 | TRACKED | P1-DRZ-IMPL |
| DISP-DRZ-007 | drizzle | §3（方差/权重传播确定性） | 第 5 行 | TRACKED | P1-DRZ-IMPL |
| DISP-DRZ-008 | drizzle | §4（欠采样重建与候选枚举完备性） | 第 4 行 | TRACKED | P1-DRZ-IMPL |
| DISP-GAIA-001 | catalog | DR3 测光列（phot_g_mean_mag / phot_bp_mean_mag / phot_rp_mean_mag） | 第 2 行 | TRACKED | catalog 域原子任务 |
| DISP-FITS-001 | fits | §4.4/§5（扩展 HDU 与表扩展） | 第 3 行 | TRACKED | IO 域原子任务 |
| STD-F6 | (跨域治理) | （注册表级：本注册表自身，§1.2/§3.1） | — | CLOSED | STD-REG-001（本注册表建立即闭环） |

### 3.1 登记纪律

- 偏差 ID 命名：域内文档已冻结的 `DISP-<域>-NNN` 沿用原 ID（不得重编号）；
  跨域治理类偏差使用 05 号 findings 登记册的 `STD-F<n>` ID。
- 新增偏差必须**同时**更新：本表、对应域偏差表、域清单行「偏差」列、
  以及域文档自身的 DISP 清单（域文档为偏差语义的权威落点，本文件只登记指针）。
- 偏差闭环（修复完成）后：删除基线/豁免条目、把清单行状态升为 `CONFORMANT`、
  本表行状态改为 `CLOSED`（不得直接删行，保留追溯）；无外部 findings 登记册时，
  同步面 = §3.2（跨域治理）与域偏差表。
- 跨域治理级 ID（`STD-F<n>`，不属任何单一域）在本表以 `域 = (跨域治理)` 登记，
  其**定义**落在 §3.2；域级 ID 的定义仍落在对应域偏差表（`C7`/§5）。

### 3.2 跨域治理偏差（跨域治理级 ID 的定义域）

跨域治理偏差指不归属任何单一标准域、而是约束"本注册表自身"或"标准↔实现关系"的偏差。
本表是这些 ID 的**唯一事实源**（机器检查 `C5_governance_*`/`C6`/`C7` 按此闭包判定）。

| 偏差 ID | 严重度 | 指针 | 处置归属 |
|---|---|---|---|
| STD-F6 | 已闭环（治理级/P1） | docs/standards/STANDARDS_REGISTRY.md（本注册表 §1.2/§3.1） | STD-REG-001（本注册表建立即闭环；依据 = `STD-F6`「国际标准冻结注册表缺失」处置面） |


---

## 4. 与 `ASTROCS_DESIGN.md` 附录 B 文献锚的对应

| `ASTROCS_DESIGN.md` 附录 B 条目 | 本注册表域 | 落地文档 |
|---|---|---|
| IVOA HiPS 1.0 Recommendation | hips | docs/algorithms/HIPS_WRITER.md；docs/interfaces/io/IO_002_HIPS_INPUT_INTERFACE.md |
| Fernique et al. 2015, Hierarchical progressive surveys | hips | docs/science/PHASE3_HIPS_TO_FITS.md |
| Górski et al. 2005, HEALPix | healpix | docs/algorithms/HEALPIX_MAPPING.md；lib/algorithms/shared/healpix/healpix_core.h |
| Greisen & Calabretta 2002, FITS WCS Paper I | spherical-projection | docs/science/ASTROMETRY.md；docs/algorithms/PHASE3_PROJ_IMPL.md |
| Calabretta & Greisen 2002, FITS WCS Paper II | spherical-projection | docs/algorithms/PHASE3_PROJ_IMPL.md |
| IAU FITS Working Group / FITS Standard | fits | docs/interfaces/io/IO_001_FITS_STREAM_INTERFACE.md |
| Fruchter & Hook 2002, Drizzle | drizzle | docs/science/DRIZZLE.md；docs/algorithms/DRIZZLE_GEOMETRY.md |

---

## 5. 机器检查合同（docs/standards/checks/check_standards_registry.py）

| 检查 | 断言 |
|---|---|
| C1 | 注册表存在；docs/DOCUMENT_INDEX.yaml doc_index.active 中登记为 ACTIVE_NORMATIVE |
| C2 | §2 域表 key 集合 == 冻结六域；`## D.<key>` 域节集合相同 |
| C3 | 每域七字段齐备；VERSION/STANDARD 与 §2 冻结值**逐字**相等；CLAUSES 含 § 条款锚；COMPLIANCE 合法；EVIDENCE 路径在仓库中实际存在 |
| C4 | 每域符合性清单表在位（列头冻结）、≥1 行、条款 ID 唯一、状态合法、证据指针指向存在的文件/目录、偏差列非空 |
| C5 | 每域偏差表在位、偏差 ID 唯一且形如 STD-F<n>/DISP-<域>-<n>、指针非空 |
| C5′ | §3.2 跨域治理偏差表在位、ID 合法唯一、指针非空，且与域偏差表**跨表不重号** |
| C6 | 正文所有 STD-F*/DISP-* 引用在**闭包域**（本注册表域偏差表 ∪ §3.2 跨域治理偏差表 ∪（若存在）外部 findings 登记册）中有定义（悬空指针 FAIL）；外部登记册缺席时显式登记"缺席（可选来源）"，绝不静默返回空集 |
| C7 | §3 偏差索引行与定义域（域偏差表 ∪ §3.2）逐 ID 一致（「（无）」行不计入 ID 集合）；域行指向的域/条款在对应清单中真实存在且域 DEVIATION 字段含该 ID；跨域治理行（域列 = `(跨域治理)`）校验其定义在 §3.2 表内且字段齐全 |
| C8 | §3 偏差索引与偏差登记面双向一致：定义域为空 ⇒ 索引必须有显式「（无）」行（不得留空）；定义域非空 ⇒ 索引不得出现「（无）」行（不得用"无"掩盖真实偏差） |
| C9 | **[W4-A3]** 域清单「偏差」列 → 域 DEVIATION 字段**反向一致**：清单行偏差列里出现的每个 STD-F*/DISP-* 词元必须在本域 DEVIATION 字段中有定义。C4 只判该列非空、C7 只判 §3 索引 → DEVIATION；补上反向后"清单行写着 STD-F1 而 DEVIATION 字段删掉它"不再可能整体绿 |

用法（PASS 时 exit 0；FAIL 为 1；锚失效为 2）：

    python3 docs/standards/checks/check_standards_registry.py --root .

锚存活（ENGINEERING_SPEC §8，fail-closed）：`REQUIRED_ANCHORS`（本注册表 +
docs/DOCUMENT_INDEX.yaml）在启动时校验 os.path.exists + `git ls-files --error-unmatch`；
失效 ⇒ stderr 打印 `ANCHOR_STALE: <常量名> <路径>` ⇒ **exit 2**（不 traceback、不静默通过）。

负向注入自证（9 场景，**域内手工复跑判据**：全部必须 FAIL；判定看 verdict 字段）：

> ⚠ **负向注入的 CI 登记状态**：
> 下列 9 场景与「注入空转守卫（`FAULT_INJECT_NOOP`）」中，**2 个场景已登记进
> `eng/ci/checks.json`**（`STD-REG-FI-DANGLING` = `dangling-deviation-id`、
> `STD-REG-FI-VERSION-DRIFT` = `version-drift`）；**其余 7 个场景与空转守卫为
> PLANNED（计划）**，**不是**已生效的强制 CI 义务。
> 本节的「全部必须 FAIL」是**域内手工复跑**判据（手动执行 `--fault-inject`），
> **不得**读作「CI 已强制」。补登记属 `eng/ci/checks.json` 写入面（**不在本文件域**），
> 须先在 `eng/ci/checks.json` 登记（`changed_paths=["docs/standards/**"]`，参照 DOC-INDEX 形态）；
> 补登记完成后方可把本节改回强制口径。

    for s in drop-domain-section drop-checklist-table illegal-status version-drift \
             drop-wcs003f1-pointer dangling-deviation-id \
             drop-governance-deviation add-none-marker-row anchor-stale; do
      python3 docs/standards/checks/check_standards_registry.py --root . --fault-inject "$s"
    done

前 8 场景恒退出 0（注入协议：判定看 verdict）；`anchor-stale` 为例外 —— 锚失效按 §8
必须非零退出，退出 2 并打印 `ANCHOR_STALE`。

**注入空转守卫**：8 个文本场景一律"确定性命中一次"替换；命中 0 次（正文
漂移导致锚点失配）或 >1 次 ⇒ 抛 `FAULT_INJECT_NOOP`（exit 3），拒绝以原文冒充
"已注入"。`drop-wcs003f1-pointer` 由 `C7_deviation_index_rows_resolve` 与
`C9_checklist_deviation_backref` 双重判红。

人工复现生产路径（不经 --fault-inject）：

    ASTROCS_STD_REG_ANCHOR_OVERRIDE='REGISTRY_REL=docs/standards/__missing__.md' \
      python3 docs/standards/checks/check_standards_registry.py --root .; echo rc=$?   # rc=2

> CI 登记状态：本检查器为**治理文档检查器**
> （非 CTest 目标、非 add_test 注册面）。**已登记**：`eng/ci/checks.json` 的 `STD-REG` 项
> （主判据 `check_standards_registry.py --root .`）+ 2 个负向注入场景
> （`STD-REG-FI-DANGLING`、`STD-REG-FI-VERSION-DRIFT`）。
> **未登记（PLANNED）**：其余 7 个注入场景 + 空转守卫；该登记属 `eng/ci/checks.json` 写入面，
> 超出本文件域（参照 DOC-INDEX 检查项形态：changed_paths=["docs/standards/**"]）。
> 补登记前，未登记场景由域内任务按 §1 纪律**手工复跑**，**不得**声称已被 CI 强制。

---

## 6. 追溯

- 上游：`ASTROCS_DESIGN.md` 附录 B / §5.3；本文件不在 `ASTROCS_DESIGN.md` §0 权威链上。
- `STD-F6`（「国际标准冻结注册表缺失」）的处置面由本文件落地；
  其跨域治理偏差**定义**在 §3.2。
- D.fits 的证据指针 = `lib/infrastructure/aio/io/include/astrocs/io/fits_stream_v1.h`。
- 域文档：docs/science/ASTROMETRY.md、docs/science/DRIZZLE.md、docs/science/PHASE3_HIPS_TO_FITS.md、
  docs/algorithms/PLATESOLVE.md、docs/algorithms/PHASE3_PROJ_IMPL.md、docs/algorithms/HIPS_WRITER.md、
  docs/algorithms/HEALPIX_MAPPING.md、docs/algorithms/DRIZZLE_GEOMETRY.md、docs/algorithms/GAIA_QUERY.md、
  docs/interfaces/io/IO_001_FITS_STREAM_INTERFACE.md、docs/interfaces/io/IO_002_HIPS_INPUT_INTERFACE.md。
- 机器检查：docs/standards/checks/check_standards_registry.py；docs/DOCUMENT_INDEX.yaml（DOC-INDEX 检查项）。
- 本文件不修改任何 SCI/ALG 公式、默认容差或冻结门；冲突一律登记偏差（§1.2/§1.5）。

---

## 附：状态字段口径

> 依据：`ASTROCS_DESIGN.md` §0.2（登记表/映射表不得写状态字段）、§12.5（状态必须现场计算）。

- 本表**不写**交付状态阶梯（§12.5 的 `CONTRACT_READY`/`IMPLEMENTED`/…）——那是模块/交付物的状态，由 `eng/tools/quality/check_module_map.py` 现场计算；文档活动分类一律以 `docs/DOCUMENT_INDEX.yaml` + `eng/tools/doccheck/check_doc_index.py` 为准。
- 本表保留的两列**不是**交付状态，且都由机器校验，不构成「表内自证绿」：
  - `符合状态`（`CONFORMANT`/`PARTIAL`/`NON_CONFORMANT`/`PROJECT_DEFINED`）= 与**外部标准条款**的关系轴，取值域与证据指针存在性由 `check_standards_registry.py` C4/C5 现场判（`docs/owner/RELEASE_STATUS.md` §0 已声明两轴独立）；
  - §3 偏差索引的 `状态`（`TRACKED`/`CLOSED`）= 偏差处置登记，定义域与索引一致性由 C6/C7 现场判。
- 若后续要求连这两列也改为「检查器现场计算」，须先改 `docs/standards/checks/check_standards_registry.py` 的 C4/C6 判据并同步 `eng/ci/checks.json` 的 STD-REG 项——属门禁改造，不在本文件域内。
