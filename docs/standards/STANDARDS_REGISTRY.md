# 国际标准冻结注册表（STANDARDS_REGISTRY）

> doc_id: STD-REG-001
> doc_status: ACTIVE_NORMATIVE
> authoring_task: STD-REG-001（ASTROCS-CONSTITUTION-ALIGNMENT-V1 rev18，dispatch 79adc53703da7a33）
> 上游权威: `ASTROCS_PROJECT_CONSTITUTION.md` §19（基础科学与格式参考）、§7.3（冻结四投影）、
> §18（负责人裁决，含 Phase3 四投影 TAN+SIN+CAR+AIT）
> 机器检查: `docs/standards/checks/check_standards_registry.py`（exit 0 = PASS；见 §5 与 §5 负向注入）
> 登记: `docs/DOCUMENT_INDEX.yaml` doc_index.active（status = ACTIVE_NORMATIVE）

---

## 1. 目的、权威与纪律

1. **唯一性**：本文件是仓库**唯一**的「领域 → 国际标准 → 版本 → 条款 → 符合性清单 →
   偏差指针」冻结注册表。任何科学/格式实现与外部标准的关系，以本文件登记为准；
   域级 SCI/ALG/DATA 文档承载公式与源码锚，本文件不复制公式、不改写任何 SCI/ALG 语义。
2. **禁止按实现反推标准**（05 号 findings 登记册 §STD-F6 处置条款）：实现与标准不一致时，
   **一律登记偏差（§3）并按 finding 处理**，不得反向修改标准条款、不得把实现现状写成标准要求。
3. **版本冻结**：§2 域表的「冻结版本」字面量由本任务冻结，机器检查器逐字比对
   （`C3_version_frozen_*`）——版本漂移（含以"最新版"含糊表述替代版本号）判 FAIL。
4. **符合状态取值域**（唯一合法四值，机器检查）：
   | 取值 | 语义 |
   |---|---|
   | `CONFORMANT` | 已按标准条款实现且有在位可执行证据（测试/oracle/工具） |
   | `PARTIAL` | 主体实现，存在已登记偏差或条款面未全覆盖（偏差指针必须非空） |
   | `NON_CONFORMANT` | 与标准条款冲突且未处置（必须登记偏差指针与处置归属） |
   | `PROJECT_DEFINED` | 标准未规定或本实现显式偏离标准之处，按 Project-defined 冻结（必须写明归属文档） |
5. **偏差指针纪律**：清单表「偏差」列与域字段 `DEVIATION` 必须指向
   §3 偏差索引中已定义、或 05 号 findings 登记册中已定义的 ID；悬空指针判 FAIL
   （`C6_deviation_id_closure`）。
6. **变更流程**：新增/变更域、版本或条款映射必须走宪章 §1.2 变更流程并同步本文件 §2/§3
   与机器检查器冻结表；仅新增偏差指针（不改语义）由域内原子任务随实现提交更新。

---

## 2. 域表（冻结）

| 域 key | 标准 | 冻结版本 | 标准条款面 |
|---|---|---|---|
| spherical-projection | FITS WCS Paper I/II + SIP（Shupe et al. 2005） | Paper I = A&A 395, 1061 (2002)；Paper II = A&A 395, 1077 (2002)；SIP = ASPC 347, 491 (2005) | Paper I §2.1.1（CRPIX 1-based）/§3（CD/CTYPE）；Paper II §2.1（旋转与 LONPOLE）/§5 Table 1（TAN/SIN/CAR/AIT）；SIP §A（A/B/AP/BP 约定） |
| hips | IVOA HiPS Recommendation 1.0（properties 修订 1.4） | HiPS 1.0 (PR-HiPS-1.0-20161122) + properties hips_version="1.4" | HiPS 1.0 §3（层级索引与目录结构）/§4.1（tile）/§4.2.1（properties）/§4.4.1（all-sky map）/§6.3.1（客户端绘制）；properties 1.4 键集 |
| healpix | Górski et al. 2005 HEALPix（NESTED） | ApJ 622, 759 (2005)，bibcode 2005ApJ...622..759G | §5.1（nside=2^order 等面积单元）/§5.2（NESTED 编号与父子关系）/§5.3（ang2pix/pix2ang） |
| drizzle | Fruchter & Hook 2002 Drizzle | PASP 114, 144 (2002)，bibcode 2002PASP..114..144F | §2（drop 与 pixfrac）/§3（线性重建与权重 w_jp=a_jp/A_drop）/§4（欠采样图像重建） |
| catalog | Gaia DR3 data model（本地 XPSD 星表） | Gaia DR3（Gaia Collaboration et al. 2023, A&A 674, A1）+ XPSD 本地编码合同 | DR3 source 列面（ra/dec 参考历元 J2016.0、phot_g_mean_mag/phot_bp_mean_mag/phot_rp_mean_mag）；本地 XPSD 记录布局（ALG-GAIA-001 §2） |
| fits | FITS Standard 4.0 | FITS 4.0（IAU FWG，2016-07-22 批准版） | §3.1（基本文件结构/80 字节卡）/§4.2（SIMPLE/BITPIX/NAXIS 基本头）/§4.4（扩展 HDU）/§5（表扩展）/§6（DATASUM/CHECKSUM 校验和） |

---

## D.spherical-projection — 球面投影与 WCS（FITS WCS Paper I/II + SIP）

- DOMAIN: spherical-projection
- STANDARD: FITS WCS Paper I/II + SIP（Shupe et al. 2005）
- VERSION: Paper I = A&A 395, 1061 (2002)；Paper II = A&A 395, 1077 (2002)；SIP = ASPC 347, 491 (2005)
- CLAUSES: Paper I §2.1.1（CRPIX 1-based）/§3（CD/CTYPE）；Paper II §2.1（旋转/LONPOLE）/§5 Table 1（TAN/SIN/CAR/AIT）；SIP §A（A/B/AP/BP）
- COMPLIANCE: PARTIAL
- EVIDENCE: docs/science/ASTROMETRY.md；docs/algorithms/PLATESOLVE.md；docs/algorithms/PHASE3_PROJ_IMPL.md；docs/algorithms/PHASE3_FITS_IMPL.md；tests/unit/p1wcs/；tests/unit/p3_projection_test.cpp；lib/phase3_session/p3_wcs.cpp；lib/plate_solve/cpp/ipv/src/ipv_wcs.cpp
- DEVIATION: STD-F1；DISP-WCS-001；DISP-WCS-006；DISP-P3PROJ-001

| 条款 | 标准要求 | 符合状态 | 证据指针 | 偏差 |
|---|---|---|---|---|
| Paper I §2.1.1（CRPIX 1-based 参考像素） | 参考像素 CRPIX 为 1-based，像素坐标 `xp = x + 1` | PARTIAL | docs/science/ASTROMETRY.md；docs/algorithms/PLATESOLVE.md；tests/unit/p1wcs/p1wcs_astropy_cross.py | STD-F1（ipv 内部 0-based 与消费方 1-based 相差常量 1px，待负责人裁决三选一） |
| Paper I §3（CD/CTYPE 关键词体系） | 线性变换以 CD 矩阵 + CTYPE 表达；FITS 头卡 ≤80 字节 | CONFORMANT | docs/algorithms/PHASE3_PROJ_IMPL.md；lib/phase3_session/p3_wcs.cpp；tests/unit/p3_projection_test.cpp | 无（T5/T7 断言在位） |
| Paper II §5 Table 1（TAN/SIN/CAR/AIT 四投影） | 四投影按 Table 1 的 R_θ 定义实现，新增投影须注册并附独立往返 Oracle | CONFORMANT | docs/algorithms/PHASE3_PROJ_IMPL.md；tests/unit/p3_projection_test.cpp；tests/backend/test_p3_projection_oracle.py | 无（registry v1 恰四行；T1/T2 往返与独立解析解在位） |
| Paper II §2.1（LONPOLE 与旋转） | 允许通用 LONPOLE/φ_p 附加旋转机制 | PROJECT_DEFINED | docs/algorithms/PHASE3_PROJ_IMPL.md | 无（本实现固定 θ₀=+90°、无 φ_p 附加旋转，显式冻结为 Project-defined；不实现通用 LONPOLE） |
| SIP §A（A/B 前向、AP/BP 逆向与单位线性剔除） | SIP 畸变系数约定与单位线性项处理 | PROJECT_DEFINED | docs/science/ASTROMETRY.md；docs/algorithms/PLATESOLVE.md | DISP-WCS-006（AP/BP 以 7×7 网格最小二乘拟合而非标准迭代反演；SCI 层已显式冻结该口径） |
| Paper I/II parity 与手性（det(CD) 符号、east_left/east_right） | 像素手性由 CD 行列式符号表达，翻转不得改变 abs(det(CD)) | CONFORMANT | docs/science/ASTROMETRY.md；docs/algorithms/PHASE3_PROJ_IMPL.md；tests/unit/p3_projection_test.cpp | 无（T3/T5 含 det<0 与 crpix 奇偶双例 bitwise 断言） |
| 退化语义（CD det→0 禁坍缩冒充解） | 退化线性变换不得产生伪 WCS | PARTIAL | docs/algorithms/PLATESOLVE.md；lib/plate_solve/cpp/ipv/src/ipv_wcs.cpp；tests/unit/p1wcs/p1wcs_tests_negative.cpp | DISP-WCS-001（历史 CD 退化静默坍缩，已由负面用例覆盖） |

### D.spherical-projection 偏差表

| 偏差 ID | 严重度 | 指针 | 处置归属 |
|---|---|---|---|
| STD-F1 | 高（P1，待裁决） | 工程控制/AstroCS_CONSTITUTION_ALIGNMENT_CONTROL_V1_20260909/05_FINDINGS_REGISTER_20260911.md | STD-F1-ADJ（负责人三选一裁决后落地：修 ipv 内部口径／导出边界 +1 桥接／合同标注双口径） |
| DISP-WCS-001 | 中 | docs/algorithms/PLATESOLVE.md | P1-WCS-IMPL（CD 退化静默坍缩，负面用例已在位） |
| DISP-WCS-006 | 低 | docs/algorithms/PLATESOLVE.md | P1-WCS-IMPL（AP/BP 网格拟合口径与 SCI 冻结一致，维护歧义） |
| DISP-P3PROJ-001 | 中 | docs/algorithms/PHASE3_PROJ_IMPL.md | P3-PROJ-IMPL / P3-PROJ-INT（PA 未接线：会话恒传 rotation_pa_deg=0.0；astrocs_p3_projection.dll 未建 entrypoint=MISSING） |

---

## D.hips — IVOA HiPS 1.0（properties 修订 1.4）

- DOMAIN: hips
- STANDARD: IVOA HiPS Recommendation 1.0（properties 修订 1.4）
- VERSION: HiPS 1.0 (PR-HiPS-1.0-20161122) + properties hips_version="1.4"
- CLAUSES: HiPS 1.0 §3（层级索引与目录结构）/§4.1（tile）/§4.2.1（properties）/§4.4.1（all-sky map）/§6.3.1（客户端绘制）；properties 1.4 键集
- COMPLIANCE: PARTIAL
- EVIDENCE: docs/algorithms/HIPS_WRITER.md；docs/science/PHASE3_HIPS_TO_FITS.md；docs/interfaces/io/IO_002_HIPS_INPUT_INTERFACE.md；docs/contracts/PUBLIC_API.md；lib/astro_image_io/src/hips/aio_hips_writer.cpp；tests/unit/CMakeLists.txt
- DEVIATION: STD-F4；DISP-HIPS-001；DISP-HIPS-002；DISP-HIPS-003；DISP-HIPS-004；DISP-HIPS-005；DISP-HIPS-006；DISP-HIPS-007；DISP-HIPS-008；DISP-HIPS-009；DISP-HIPS-010；DISP-HIPS-011；DISP-HIPS-012

| 条款 | 标准要求 | 符合状态 | 证据指针 | 偏差 |
|---|---|---|---|---|
| §3（层级索引与 NorderK/DirD/NpixN 目录结构） | 层级 tile 目录命名与 NESTED 地址编码固定 | CONFORMANT | docs/interfaces/io/IO_002_HIPS_INPUT_INTERFACE.md；lib/astro_image_io/src/hips/aio_hips_writer.cpp；tests/unit/CMakeLists.txt | 无（D=ipix/10000、N=ipix%10000 与读侧同一合同） |
| §4.1（tile 为 W×W FITS 单元，tile_width=512） | tile 宽为 2 的幂、标准 512；tile 内 NESTED 序 | CONFORMANT | docs/algorithms/HIPS_WRITER.md；tests/unit/CMakeLists.txt | DISP-HIPS-006；DISP-HIPS-008（写路径无互斥包装；兼容入口 8bit support 旧语义并存） |
| §4.2.1（properties 必需键集） | `hips_version/hips_order/hips_tile_width/hips_tile_format/hips_frame` 必需且自洽 | PARTIAL | docs/interfaces/io/IO_002_HIPS_INPUT_INTERFACE.md；lib/astro_image_io/src/hips/aio_hips_writer.cpp | STD-F4（写出侧键集缺 em_min/em_max/obs_bandpass 等推荐键；META-002 禁伪造，待 index.json 真实滤镜元数据接通） |
| §4.2.1（properties 可选/推荐键：hips_status/hips_estsize/hips_initial_fov） | 可选键存在时须自洽、非占位 | PARTIAL | docs/algorithms/HIPS_WRITER.md；lib/astro_image_io/src/hips/aio_hips_writer.cpp | DISP-HIPS-002（hips_estsize="1000000"、hips_initial_fov="60" 硬编码占位）；DISP-HIPS-003（hips_status 恒 "private master"） |
| §4.4.1（all-sky map 与 MOC 关系） | 覆盖由 MOC 表达；低阶像素=子像素聚合 | CONFORMANT | docs/algorithms/HIPS_WRITER.md；lib/astro_image_io/src/hips/aio_hips_writer.cpp | DISP-HIPS-005（moc_order 静默 clamp；低阶 UNIQ 对自家 reader 无效，Moc.fits 为 optional hint） |
| §6.3.1（客户端绘制所需的初始视场/像素尺度元数据） | 提供 `hips_pixel_scale`/`hips_initial_fov` 等客户端键 | CONFORMANT | lib/astro_image_io/src/hips/aio_hips_writer.cpp；docs/algorithms/HIPS_WRITER.md | DISP-HIPS-012（FIRSTPIX/LASTPIX 声明性头卡无消费方） |
| properties 1.4（hips_version="1.4" 与 hierarchy 聚合） | 1.4 修订的 properties 版本字面量与层级聚合语义 | CONFORMANT | lib/astro_image_io/src/hips/aio_hips_writer.cpp；docs/algorithms/HIPS_WRITER.md；docs/contracts/PUBLIC_API.md | DISP-HIPS-009（f32 产品层级累加为 float 求和，多子 tile 舍入漂移） |

### D.hips 偏差表

| 偏差 ID | 严重度 | 指针 | 处置归属 |
|---|---|---|---|
| STD-F4 | 中（P2） | 工程控制/AstroCS_CONSTITUTION_ALIGNMENT_CONTROL_V1_20260909/05_FINDINGS_REGISTER_20260911.md | HiPS 域原子任务（以 testdata/index.json 真实滤镜元数据接通 em_min/em_max/obs_bandpass，不得臆造） |
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
- EVIDENCE: docs/algorithms/HEALPIX_MAPPING.md；docs/algorithms/HIPS_WRITER.md；lib/common/healpix/healpix_core.cpp；lib/common/healpix/tests；tests/unit/CMakeLists.txt
- DEVIATION: 无域内偏差（HEALPix 核心零偏差；层级聚合精度边界归 HiPS 域 DISP-HIPS-009）

| 条款 | 标准要求 | 符合状态 | 证据指针 | 偏差 |
|---|---|---|---|---|
| §5.1（nside=2^order，等面积单元 A=4π/(12·nside²)） | 单元面积公式与 nside 幂次关系 | CONFORMANT | lib/common/healpix/healpix_core.cpp；docs/algorithms/HIPS_WRITER.md；tests/unit/CMakeLists.txt | 无 |
| §5.2（NESTED 编号与 4 分叉父子关系） | NESTED 索引与父子位移关系 child = 4·parent + k | CONFORMANT | lib/common/healpix/healpix_core.cpp；lib/common/healpix/tests/test_healpix_neighbors.cpp；tests/unit/CMakeLists.txt | 无（tile_shift=9、mask=(1<<18)-1 不变量在位） |
| §5.3（ang2pix/pix2ang 往返） | 球面角 ↔ NESTED 索引往返在 FP64 下达机器精度 | CONFORMANT | lib/common/healpix/tests/test_healpix_oracle.cpp；lib/common/healpix/tests/snr_hips_spatial_oracle.py；docs/algorithms/HEALPIX_MAPPING.md | 无（往返 ≤1e-12 deg；astropy-healpix 百万点 oracle 对拍） |
| §5.2/§5.3（order 上限与溢出收口） | order ≤ 29 且越界输入显式拒绝 | CONFORMANT | lib/common/healpix/healpix_core.cpp；lib/common/healpix/tests/test_healpix_neighbors.cpp | 无（checked 收口，禁静默溢出） |
| 单源纪律（B4-01 去重） | 全仓唯一 HEALPix 权威实现，重复实现须机器门禁 | CONFORMANT | lib/common/healpix/healpix_core.h；docs/algorithms/HEALPIX_MAPPING.md | 无（deprecated shim + 机器门禁登记在位） |

### D.healpix 偏差表

| 偏差 ID | 严重度 | 指针 | 处置归属 |
|---|---|---|---|
| （无） | — | HEALPix 核心实现与 Górski 2005 NESTED 条款无偏差；下游层级聚合精度边界登记在 HiPS 域 | 域内无待处置项（DISP-HIPS-009 归 HiPS 域） |

---

## D.drizzle — Drizzle 线性重建（Fruchter & Hook 2002）

- DOMAIN: drizzle
- STANDARD: Fruchter & Hook 2002 Drizzle
- VERSION: PASP 114, 144 (2002)，bibcode 2002PASP..114..144F
- CLAUSES: §2（drop 与 pixfrac）/§3（线性重建与权重 w_jp=a_jp/A_drop）/§4（欠采样图像重建）
- COMPLIANCE: PARTIAL
- EVIDENCE: docs/science/DRIZZLE.md；docs/algorithms/DRIZZLE_GEOMETRY.md；lib/healpix_db/healpix_drizzle/tests/candidate_oracle_test.cpp；lib/healpix_db/healpix_drizzle/tests/p1drz
- DEVIATION: DISP-DRZ-001；DISP-DRZ-002；DISP-DRZ-003；DISP-DRZ-004；DISP-DRZ-005；DISP-DRZ-006；DISP-DRZ-007；DISP-DRZ-008

| 条款 | 标准要求 | 符合状态 | 证据指针 | 偏差 |
|---|---|---|---|---|
| §2（drop 与 pixfrac 收缩因子） | drop 为源像素按 pixfrac 收缩后的足迹，pixfrac∈(0,1] | PARTIAL | docs/science/DRIZZLE.md；docs/algorithms/DRIZZLE_GEOMETRY.md；lib/healpix_db/healpix_drizzle/tests/candidate_oracle_test.cpp | DISP-DRZ-003（API 层接受 pixfrac=0.0，引擎层拒绝——两层双轨） |
| §3（线性重建 w_jp = a_jp / A_drop 与面亮度语义） | 权重为交叠面积与 drop 面积之比；每像素常量 ADU ⇒ S=C/A_drop | CONFORMANT | docs/science/DRIZZLE.md；docs/algorithms/DRIZZLE_GEOMETRY.md；lib/healpix_db/healpix_drizzle/tests/p1drz | DISP-DRZ-002（面积实现为 S-H 裁剪+Eriksson 扇形剖分，非 Girard 定理，文档措辞已登记） |
| §3（球面交叠面积与微小 drop 数值路径） | 交叠面积计算须数值稳定 | PROJECT_DEFINED | docs/algorithms/DRIZZLE_GEOMETRY.md | DISP-DRZ-005（角跨度 <1e-3 rad 时切平面分支为真路径，注释论证偏差 <4e-8；禁删） |
| §4（欠采样重建与候选枚举完备性） | 重建须覆盖全部候选源像素，零漏选 | CONFORMANT | lib/healpix_db/healpix_drizzle/tests/candidate_oracle_test.cpp；docs/algorithms/DRIZZLE_GEOMETRY.md | 无（9003 例全枚举 false_negative=0：4 pixfrac × 5 尺度 × 7 nside × RA 跨 0 × 极区 × face 边界） |
| §3（方差/权重传播确定性） | 重建为线性加权，须确定性可复现 | CONFORMANT | docs/science/DRIZZLE.md；docs/algorithms/DRIZZLE_GEOMETRY.md | DISP-DRZ-004（值像素 NaN 静默 continue，无计数暴露）；DISP-DRZ-007（方差锚行号漂移） |
| §2/§3（SIP 畸变场下的 drop 映射） | 源像素角点经 WCS 映射到球面多边形 | PARTIAL | docs/algorithms/DRIZZLE_GEOMETRY.md；lib/healpix_db/healpix_drizzle/tests | DISP-DRZ-001（SIP 阶数校验 [0,5] 与注释 0..4 不符） |

### D.drizzle 偏差表

| 偏差 ID | 严重度 | 指针 | 处置归属 |
|---|---|---|---|
| DISP-DRZ-001 | 低 | docs/algorithms/DRIZZLE_GEOMETRY.md | P1-DRZ-IMPL（SIP 阶数注释与校验不一致） |
| DISP-DRZ-002 | 低 | docs/algorithms/DRIZZLE_GEOMETRY.md | P1-DRZ-IMPL（面积算法文档措辞 vs 实现） |
| DISP-DRZ-003 | 中 | docs/algorithms/DRIZZLE_GEOMETRY.md | P1-DRZ-IMPL（pixfrac 双轨边界） |
| DISP-DRZ-004 | 中 | docs/algorithms/DRIZZLE_GEOMETRY.md | P1-DRZ-IMPL（NaN 值像素静默跳过） |
| DISP-DRZ-005 | 中 | docs/algorithms/DRIZZLE_GEOMETRY.md | P1-DRZ-IMPL / P1-DRZ-INT（微小 drop 切平面真路径须守护，禁按旧登记删除） |
| DISP-DRZ-006 | 低 | docs/algorithms/DRIZZLE_GEOMETRY.md | P1-DRZ-IMPL（累加器字段数注释漂移） |
| DISP-DRZ-007 | 低 | docs/algorithms/DRIZZLE_GEOMETRY.md | P1-DRZ-IMPL（方差锚行号漂移） |
| DISP-DRZ-008 | 低 | docs/algorithms/DRIZZLE_GEOMETRY.md | P1-DRZ-IMPL（PolyClip legacy 零调用） |

---

## D.catalog — Gaia DR3 data model（本地 XPSD 星表）

- DOMAIN: catalog
- STANDARD: Gaia DR3 data model（本地 XPSD 星表）
- VERSION: Gaia DR3（Gaia Collaboration et al. 2023, A&A 674, A1）+ XPSD 本地编码合同
- CLAUSES: DR3 source 列面（ra/dec 参考历元 J2016.0、phot_g_mean_mag/phot_bp_mean_mag/phot_rp_mean_mag）；本地 XPSD 记录布局（ALG-GAIA-001 §2）
- COMPLIANCE: PARTIAL
- EVIDENCE: docs/algorithms/GAIA_QUERY.md；docs/modules/gaia_xpsd_client.md；docs/science/ASTROMETRY.md；lib/gaia_xpsd_client/src/gaia_client.c；tests/unit/CMakeLists.txt
- DEVIATION: DISP-GAIA-001

| 条款 | 标准要求 | 符合状态 | 证据指针 | 偏差 |
|---|---|---|---|---|
| DR3 source 位置列（ra/dec，ICRS，参考历元 J2016.0） | 位置以 ICRS 表达，RA∈[0,360)、Dec∈[-90,90] | CONFORMANT | docs/algorithms/GAIA_QUERY.md；docs/science/ASTROMETRY.md；lib/gaia_xpsd_client/src/gaia_client.c | 无（RA 归一到 [0,360)；frame 契约与 SCI-WCS-001 §3a 一致） |
| DR3 测光列（phot_g_mean_mag / phot_bp_mean_mag / phot_rp_mean_mag） | G/BP/RP 星等语义与量化解码 | PARTIAL | docs/algorithms/GAIA_QUERY.md；lib/gaia_xpsd_client/src/gaia_client.c | DISP-GAIA-001（本地 XPSD 以 uint16×0.001−1.5 量化表达；非官方 archive 数据模型，仓库无版本化 DR3 data model 文档） |
| DR3 source 列面完备性（source_id 等主键列） | 星表主键与列面可追溯 | NON_CONFORMANT | docs/algorithms/GAIA_QUERY.md；docs/modules/gaia_xpsd_client.md | DISP-GAIA-001（本地 XPSD 仅存位置/星等/光谱子集，无 source_id 主键列；跨表身份靠位置匹配） |
| XPSD 本地编码（2 µas/LSB 位置量化、10 µas/LSB dra、0.001 mag 星等） | 本地编码须与标准列语义无损对应并写明换算 | CONFORMANT | docs/algorithms/GAIA_QUERY.md；lib/gaia_xpsd_client/src/gaia_client.c | 无（历史 7.2 µas/LSB 换算错误已按实测锚修正登记） |
| DR3SP 光谱量化解码（F(λ)=byte·fluxMul+fluxMin） | 光谱量化残差须量化登记 | PARTIAL | docs/algorithms/GAIA_QUERY.md；docs/modules/gaia_xpsd_client.md | DISP-GAIA-001（8-bit 量化残差 median 0.21%/p95 1.8%，属本地编码损失） |
| 查询锥与星等窗语义（角距 ≤ρ、m_lo≤m_G≤m_hi 闭区间） | 球面角距定义与闭区间边界 | CONFORMANT | docs/algorithms/GAIA_QUERY.md；tests/unit/CMakeLists.txt | 无（含极区/跨 RA=0 边界用例） |

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
- EVIDENCE: docs/interfaces/io/IO_001_FITS_STREAM_INTERFACE.md；docs/algorithms/PHASE3_FITS_IMPL.md；docs/contracts/DATA_SEMANTICS.md；modules/services/io/include/astrocs/io/fits_stream_v1.h；tests/io/test_fits_stream_contract.py
- DEVIATION: DISP-FITS-001

| 条款 | 标准要求 | 符合状态 | 证据指针 | 偏差 |
|---|---|---|---|---|
| §3.1（基本文件结构：80 字节卡、END、2880 字节块） | header 卡固定 80 字节、以 END 结束、按 2880 字节补齐 | CONFORMANT | docs/interfaces/io/IO_001_FITS_STREAM_INTERFACE.md；tests/io/test_fits_stream_contract.py | 无（含非法 header/END 缺失负面用例） |
| §4.2（SIMPLE/BITPIX/NAXIS 基本头与基本图像 HDU） | 基本 HDU 头卡合法且维度一致 | CONFORMANT | docs/interfaces/io/IO_001_FITS_STREAM_INTERFACE.md；docs/algorithms/PHASE3_FITS_IMPL.md；tests/io/test_fits_stream_contract.py | 无（NAXIS≥0、≤3；dtype/shape 失配显式拒绝） |
| §4.4/§5（扩展 HDU 与表扩展） | 扩展 HDU/BINTABLE 结构与 EXTNAME/BUNIT 语义 | PARTIAL | docs/algorithms/PHASE3_FITS_IMPL.md；tests/io/test_fits_stream_contract.py | DISP-FITS-001（扩展 HDU 面按产品子集实现：仅登记 EXTNAME/BUNIT/DATASUM 面，未覆盖通用表扩展全集） |
| §6（DATASUM/CHECKSUM 校验和） | 数据与头校验和须可复算、校验失败显式报错 | CONFORMANT | docs/interfaces/io/IO_001_FITS_STREAM_INTERFACE.md；docs/contracts/DATA_SEMANTICS.md；tests/io/test_fits_stream_contract.py | 无（内容哈希流式重算复核在位） |
| §3.1/§4.2（错误语义：截断/坏头/不支持位深） | 违规输入显式错误码，禁静默降级 | CONFORMANT | docs/interfaces/io/IO_001_FITS_STREAM_INTERFACE.md；modules/services/io/include/astrocs/io/fits_stream_v1.h；tests/io/test_fits_stream_contract.py | 无（ACS_FIO_ERR_* 17 码，含 TRUNCATED/BAD_HEADER/UNSUPPORTED） |
| §4.2（BITPIX 与像素中心/值域语义） | 位深与数据类型显式，单位与 BUNIT 一致 | PARTIAL | docs/contracts/DATA_SEMANTICS.md；docs/algorithms/PHASE3_FITS_IMPL.md | DISP-FITS-001（科学产品的 BITPIX/BUNIT 面按 Phase 子集登记，全通用位深面归 IO 域后续任务） |

### D.fits 偏差表

| 偏差 ID | 严重度 | 指针 | 处置归属 |
|---|---|---|---|
| DISP-FITS-001 | 低 | docs/interfaces/io/IO_001_FITS_STREAM_INTERFACE.md；docs/contracts/DATA_SEMANTICS.md | IO 域原子任务（扩展 HDU/表扩展与全通用位深面按 FITS 4.0 全集收敛；当前为显式产品子集，非静默偏差） |

---

## 3. 偏差索引（域 × 条款 → 偏差 ID → 处置归属）

| 偏差 ID | 域 | 条款 | 注册表清单行 | 状态 | 处置归属 |
|---|---|---|---|---|---|
| STD-F1 | spherical-projection | Paper I §2.1.1（CRPIX 1-based 参考像素） | 第 1 行 | OPEN（待负责人裁决） | STD-F1-ADJ |
| DISP-WCS-001 | spherical-projection | 退化语义（CD det→0 禁坍缩冒充解） | 第 7 行 | TRACKED | P1-WCS-IMPL |
| DISP-WCS-006 | spherical-projection | SIP §A（A/B 前向、AP/BP 逆向与单位线性剔除） | 第 5 行 | TRACKED | P1-WCS-IMPL |
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
| DISP-DRZ-002 | drizzle | §3（线性重建 w_jp = a_jp / A_drop 与面亮度语义） | 第 2 行 | TRACKED | P1-DRZ-IMPL |
| DISP-DRZ-003 | drizzle | §2（drop 与 pixfrac 收缩因子） | 第 1 行 | TRACKED | P1-DRZ-IMPL |
| DISP-DRZ-004 | drizzle | §3（方差/权重传播确定性） | 第 5 行 | TRACKED | P1-DRZ-IMPL |
| DISP-DRZ-005 | drizzle | §3（球面交叠面积与微小 drop 数值路径） | 第 3 行 | TRACKED | P1-DRZ-IMPL / P1-DRZ-INT |
| DISP-DRZ-006 | drizzle | §2（drop 与 pixfrac 收缩因子） | 第 1 行 | TRACKED | P1-DRZ-IMPL |
| DISP-DRZ-007 | drizzle | §3（方差/权重传播确定性） | 第 5 行 | TRACKED | P1-DRZ-IMPL |
| DISP-DRZ-008 | drizzle | §4（欠采样重建与候选枚举完备性） | 第 4 行 | TRACKED | P1-DRZ-IMPL |
| DISP-GAIA-001 | catalog | DR3 测光列（phot_g_mean_mag / phot_bp_mean_mag / phot_rp_mean_mag） | 第 2 行 | TRACKED | catalog 域原子任务 |
| DISP-FITS-001 | fits | §4.4/§5（扩展 HDU 与表扩展） | 第 3 行 | TRACKED | IO 域原子任务 |

### 3.1 登记纪律

- 偏差 ID 命名：域内文档已冻结的 `DISP-<域>-NNN` 沿用原 ID（不得重编号）；
  跨域治理类偏差使用 05 号 findings 登记册的 `STD-F<n>` ID。
- 新增偏差必须**同时**更新：本表、对应域偏差表、域清单行「偏差」列、
  以及域文档自身的 DISP 清单（域文档为偏差语义的权威落点，本文件只登记指针）。
- 偏差闭环（修复完成）后：删除基线/豁免条目、把清单行状态升为 `CONFORMANT`、
  本表行状态改为 `CLOSED`（不得直接删行，保留追溯），并同步 05 号登记册。

---

## 4. 与宪章 §19 文献锚的对应

| 宪章 §19 条目 | 本注册表域 | 落地文档 |
|---|---|---|
| IVOA HiPS 1.0 Recommendation | hips | docs/algorithms/HIPS_WRITER.md；docs/interfaces/io/IO_002_HIPS_INPUT_INTERFACE.md |
| Fernique et al. 2015, Hierarchical progressive surveys | hips | docs/science/PHASE3_HIPS_TO_FITS.md |
| Górski et al. 2005, HEALPix | healpix | docs/algorithms/HEALPIX_MAPPING.md；lib/common/healpix/healpix_core.h |
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
| C6 | 正文所有 STD-F*/DISP-* 引用在本注册表或 05 号 findings 登记册中有定义（悬空指针 FAIL） |
| C7 | §3 偏差索引行与域偏差表逐 ID 一致；每行指向的域/条款在对应清单中真实存在，且域 DEVIATION 字段含该 ID |

用法（PASS 时 exit 0）：

    python3 docs/standards/checks/check_standards_registry.py --root .

负向注入自证（6 场景，全部必须 FAIL；退出码恒 0，判定看 verdict 字段）：

    for s in drop-domain-section drop-checklist-table illegal-status version-drift              drop-wcs003f1-pointer dangling-deviation-id; do
      python3 docs/standards/checks/check_standards_registry.py --root . --fault-inject "$s"
    done

> CI 登记状态：本检查器为**治理文档检查器**（非 CTest 目标、非 add_test 注册面），
> 当前 CI 检查项（ci/checks.json）尚未显式登记该命令——该登记属 ci/checks.json 写入面，
> 超出本任务白名单（docs/standards/ + docs/DOCUMENT_INDEX.yaml），已作为 finding 登记，
> 由 CI 域原子任务承接（同提交注册显式检查项，参照 DOC-INDEX 检查项形态：
> changed_paths=["docs/standards/**"]）。登记前本检查器由域内任务按 §1 纪律手工复跑。

---

## 6. 追溯

- 上游：ASTROCS_PROJECT_CONSTITUTION.md §19 / §7.3 / §18；05 号 findings 登记册 §STD-F6（本文件即其处置物）、§STD-F1、§STD-F4。
- 域文档：docs/science/ASTROMETRY.md、docs/science/DRIZZLE.md、docs/science/PHASE3_HIPS_TO_FITS.md、
  docs/algorithms/PLATESOLVE.md、docs/algorithms/PHASE3_PROJ_IMPL.md、docs/algorithms/HIPS_WRITER.md、
  docs/algorithms/HEALPIX_MAPPING.md、docs/algorithms/DRIZZLE_GEOMETRY.md、docs/algorithms/GAIA_QUERY.md、
  docs/interfaces/io/IO_001_FITS_STREAM_INTERFACE.md、docs/interfaces/io/IO_002_HIPS_INPUT_INTERFACE.md。
- 机器检查：docs/standards/checks/check_standards_registry.py；docs/DOCUMENT_INDEX.yaml（DOC-INDEX 检查项）。
- 本文件不修改任何 SCI/ALG 公式、默认容差、冻结门或负责人裁决；冲突一律登记偏差（§1.2/§1.5）。
