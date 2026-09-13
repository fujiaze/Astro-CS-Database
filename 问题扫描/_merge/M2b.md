# M2b 合并报告 — L03（19）× L15（22）= 41 条

> 合并代理：M2b（域：IVOA HiPS 产品面 / HEALPix NESTED 映射 / AIO 唯一写出路径 / 国际标准六域横向合规）
> 定稿 29 条（P0:10 / P1:15 / P2:4），落位 问题扫描/findings/<类别>/p<0|1|2>/M2b_L03_L15.md（**12 个文件**：A_p0、B_p0、B_p1、C_p0、C_p1、C_p2、E_p1、F_p0、F_p1、G_p1、H_p1、H_p2；D_COMMENT 域经复核无独立立条项，理由见第三节 C 第 7 项）。
> 取证口径（按前台警示全量返工）：所有「命中 / 0 命中 / 计数」一律用 ripgrep **整行**匹配并写明作用域；**不以 read 的显示文本作结构化文件比对判据**（read 对 >2000 字符单行会截断，而注册表清单行的「偏差」列正是末列）；一切转述数字一律自算并附口径。
> 本次返工订正了本包自己定稿中的 4 处失真引用（见第三节 A 表），未静默丢弃任何一条。

---

## 一、逐条处置表（四态）

| 输入 ID | 叶子级判级/类别 | 四态 | 定稿 | 一句话依据（本次实测） |
|---|---|---|---|---|
| L03-001 | P0/A_SCI_DEF | 仍成立 | M2b-A-01 | begin 闸门仍是 nside<512 + ilog2 向下取整（:481-494）；适配层新守卫只查整除与 tile_nside 幂次（:2453），nside=1536 仍放行 |
| L03-002 | P1/B_STD | 仍成立 | M2b-A-02（与 L03-017 并档） | 层级卡仍 NSIDE=2^(k+9)，ALG (4c) 同行自相矛盾（:142 与 :146 并存） |
| L03-003 | P0/B_STD | 仍成立 | M2b-B-02 | write_moc_fits（:293-330）只写 MOCORDER/PIXCOUNT；该函数内 ORDERING=NUNIQ 与 COORDSYS 0 命中；注册表 :97 仍 CONFORMANT |
| L03-004 | P0/C_DOC | **部分修复** | M2b-C-01 | 头文件/sink/hips_p2 三处「原子发布内建」虚假承诺已消失；writer 仍直写（src/hips 内 rename/fsync/staging/COMPLETE/aio_publish 全 0 命中），module_adapters 三处注释仍在（:24/:2419-2420/:3913） |
| L03-005 | P1/B_STD | **部分修复** | M2b-B-07（与 L15-012 并档） | 非幂次/越界已抛 invalid_argument 且有测试；order 上界仍无（npix :333 仍 12ULL·nside² 无校验）、order-31 合法断言仍在（:224）、§5.1/§5.3 条款号仍错挂 |
| L03-006 | P1/F_TEST | 仍成立（升 P0） | M2b-F-01（与 L15-013 并档） | healpix_fullsky_oracle.py 与 oracle.jsonl 在非 run 区 glob 0 命中；test_healpix_oracle 未进 ctest；容差 1.2×像素 + 极点 continue |
| L03-007 | P1/B_STD | 仍成立（改类） | M2b-G-04 | NOTICE :19/:22 与 healpix_core.cpp :338/:418 的 Healpix_3.83 移植声明互斥，常量表已入库 |
| L03-008 | P1/C_DOC | 仍成立 | M2b-C-02 | README :24/:27（无目标/未开始）vs module.yaml :4（已交付）vs add_library SHARED + packaging required_unit 三向互斥 |
| L03-009 | P2/C_DOC | **部分修复** | M2b-C-03 | 构建闭包已补入 aio_hips_reader.cpp（:36 RESCUE-V3 FD-01 更正段）；:13 的「零依赖剔除…grep 0 命中」旧注释残留 |
| L03-010 | P1/G_GOV | 仍成立 | M2b-G-01 | lib/infrastructure/** glob 0 文件；lib/hips 与 lib/hips_p2 两个顶层模块面；docs/architecture 无任何架构偏差登记 |
| L03-011 | P1/E_TRACE | 仍成立 | M2b-E-01 | 逐条实测偏移：begin :386-422→现 :466、SNR :977-1000→现 :1246-1261（+215）、DATA :399-401→现 :435、abort :1133-1137→现 :1557（+424）、PUBLIC_API「九导出 :104,121…」→实测 12 个 AIO_HIPS_EXPORT 函数在 :114-275 |
| L03-012 | P1/B_STD | 仍成立（拆双事实） | M2b-B-05 + M2b-F-03 | 「http:// 」实测 3 行 4 处（:1249/:1250/:1251×2，逐行计数）；在册断言仍只有 xml.find("<VOTABLE") 子串 |
| L03-013 | P2/C_DOC | 仍成立 | M2b-C-05 | types.h :35 的 002..005 语义与 ALG §0/章头整体错位一格 |
| L03-014 | P2/H_NUM | 仍成立（升 P1） | M2b-H-01 | properties 侧仍 std::to_string（:986）、manifest 侧 %.8f（:1501-1502）、ALG :299-300 冻结 <1e-9；测试仍取 3/12 且双侧同格式 |
| L03-015 | P2/A_SCI | 仍成立（改类 H_NUMERIC） | M2b-H-02 | pix2ang_nest 越界仍置零后 return（:248-255）；两处 shift>=32→31 夹紧（:275/:280）；注册表仍「无域内偏差」 |
| L03-016 | P1/B_STD | 仍成立（改类 C_DOC） | M2b-C-04 | 四层口径并存（reader 必须含 1.4 / AIO 写 1.4 / 会话层不要求 / 夹具写 1.0）；VERSION 仍钉 PR-HiPS-1.0-20161122；其「1.4 是否合法」子问已用标准原文闭环（剔除） |
| L03-017 | P1/C_DOC | 仍成立 | M2b-A-02（并档） | IO_002 §3.1 仍 TW 1..16384 而 §3.2 仍 NSIDE=2^(K+9)；会话层 :121 仍 TW≠512 拒绝；p2 适配层仍 target_order 0..20 |
| L03-018 | P1/F_TEST | **部分修复** | M2b-F-02 | nrej/nused 已有 present 守卫（:1479-1480）、provenance 双向往返已入 verify V6（:1811-1842）；「‖ true」（:89）、「同一文件读两次」（:94-95）、variance/ivar 无 present 守卫仍在 |
| L03-019 | P2/C_DOC | 仍成立 | M2b-C-06 | manifest 计数仍 (flags ? leaf_ipix_list.size() : 0)（:1511-1514），products 清单用 present（:1483-1484）；ALG (5d) 字段清单不含该两键 |
| L15-001 | P0/B_STD | 仍成立 | M2b-B-01 | tile_rel_path :137-138 仍是商/余数互换；全仓同式 13 处（生产 3 / 夹具与 Oracle 6 / 浏览器与工具 4），标准 Dir=(N/10000)*10000 |
| L15-002 | P0/B_STD | 仍成立（维持 P0） | M2b-B-03 | hips_pixel_scale 仍按角秒公式写入 degrees 键（:943/:984，%.6f）⇒ 交付元数据 3600 倍错值 |
| L15-003 | P0/B_STD | 仍成立 | M2b-B-04 | CLAUSES 四条锚全错挂；必需键集仍自定 5 键（标准 §4.4.1 明列 9 键）；STD-F4 推荐的 obs_bandpass 在 REC 全文 0 命中 |
| L15-004 | P0/B_STD | **移交** | —（M2a） | Gaia 历元/UR-1 属 D.catalog 与 Gaia 查询域，非本域；证据（锚实漂 +93）已随移交表交付 |
| L15-005 | P1/B_STD | 仍成立 | M2b-B-06 | hips_status 仍两组词（:958）、hips_hierarchy 仍 "true"；标准 §4.4.1 为三组必填 |
| L15-006 | P0/B_STD | 仍成立（本域持登记面，代码面会签 M2a） | M2b-B-09 | 恒预留并写 CHECKSUM='0000000000000000'（:1151-1156，注释「HDU checksum updated」）；仅 write_checksum 才 patch（:1449）；verify 豁免设在计算值（:1651）；D.fits :223 仍 CONFORMANT |
| L15-007 | P1/B_STD | 部分成立（条款锚面定档，代码面移交） | M2b-B-10 | FITS 4.0 官方目录实测：§6=Random-groups structure、§4.4.1=Mandatory keywords、§4.4.2=Other reserved、§7=Standard extensions、附录 J=CHECKSUM ⇒ 五条款号确错挂 |
| L15-008 | P1/B_STD | 仍成立（计数改判） | M2b-B-08 | 整行匹配实测 7 行 CONFORMANT 挂开放 ID（叶子级报 6 行；差额=第 62 行挂的是 STD-F1「已闭环」指针），共 9 个 ID；口径见第六节 |
| L15-009 | P1/E_TRACE | 仍成立（改类 B_STD） | M2b-B-08 | 挂空 ID 实测 8 个（DISP-HIPS-001/004/007/010/011、DISP-P3PROJ-001、DISP-DRZ-006/008），与叶子级计数一致 |
| L15-010 | P1/E_TRACE | 仍成立 | M2b-B-08 | §3 索引 27 行中 25 TRACKED、STD-F1 状态列越值域、**CLOSED=0** |
| L15-011 | P1/B_STD | **拆分**：条款锚定档 / 语义移交 | M2b-B-10 +（M1a） | ar5iv 实测小节名：2.2 Reference point of the projection（LONPOLE 在此节）、2.3 Spherical coordinate rotation ⇒ §2.1 确为错挂；θ₀/LONPOLE 语义移交 M1a |
| L15-012 | P1/B_STD | **部分修复** | M2b-B-07（并档） | 同 L03-005 |
| L15-013 | P1/F_TEST | 仍成立（升 P0） | M2b-F-01（并档） | 同 L03-006；两侧证据均保留 |
| L15-014 | P1/B_STD | 仍成立 | M2b-B-08 | :239 状态列实测 =「CONFORMANT（导出边界 Phase3 单点 +1 桥接，实测 astropy 交叉 5.7e-14 deg）」，不在三值域内 |
| L15-015 | P2/A_SCI | **移交** | —（M1a） | 本域已复核证据在案：p3_projection.cpp:73-74/:86-87 第二处裸 ±1.0 桥接仍在（p3_wcs.cpp 有 fail-closed 的 fits_pixel_1based）；实现域归 M1a |
| L15-016 | P2/A_SCI | **移交** | —（M1a/M7） | 本域已复核：正向仍 denom<=0（:120）、逆向仍 r>=M_PI/2（:136），注释 :22 仍写「数学等价」；等价性证明归 M1a |
| L15-017 | P2/B_STD | **移交** | —（M1a） | 本域已复核：car/ait 仍不含 crval_dec_deg（:200-205/:212-217），:389 仍写 CRVAL2 卡 |
| L15-018 | P1/E_TRACE | 部分定档 + 部分移交 | M2b-E-01（hips 实例） | 门侧实测：check_doc_line_anchors.py 判据只有 C2 可解析 / C3 界内 / C4 声明式绑定；anchor_contract.json bindings 41 条中 HIPS_WRITER 仅 3 条且不含 writer.cpp ⇒ 漂移结构上不可见；GAIA_QUERY→M2a、PHASE3_PROJ_IMPL→M1a、门改造→M5/M6b |
| L15-019 | P1/G_GOV | 仍成立 | M2b-G-02 | ci/checks.json grep check_standards_registry = 0；checker 实测 27 处断言名无 C8_*；catalog 既无宪章 §19 锚（§19 实测 7 条）也无注册表 §4 对应行（:282-288 实测 7 行） |
| L15-020 | P2/C_DOC | 仍成立 | M2b-C-07 | 自算：fits_stream_v1.h 枚举 = 13 个错误码（含 OK 与哨兵 14 值），注册表 :224 写「17 码」；p3_wcs.cpp 实测 230 行，PHASE3_PROJ_IMPL:12 写「165 行」 |
| L15-021 | P2/D_COMMENT | 仍成立（拆两条、改类） | M2b-G-04 + M2b-H-02 | 出处三向互斥 + 断链 URL 入 G-04；静默收口入 H-02 |
| L15-022 | P1/G_GOV | 仍成立 | M2b-G-03 | CODE_STANDARD:3 权威来源=V19R2、:7 写「正式 toolchain：MSYS2 MinGW64 g++ 16.1.0」，与宪章 §15.2:585「Windows 正式工具链为 Visual Studio 2022 / MSVC v143」冲突；ci/toolchain.lock.json 实为 gcc 14.2.0 |

**四态统计（按 41 条输入逐条计）**：仍成立 **29** · 部分修复 **5**（L03-004、L03-005、L03-009、L03-018、L15-012）· 部分成立/拆分处置 **3**（L15-007、L15-011、L15-018：条款锚面在本域定档，代码/语义面移交）· 整条移交 **4**（L15-004、L15-015、L15-016、L15-017）· 整条已被修复 **0** · 无法判定 **0**。合计 29+5+3+4 = 41 ✓
**对账**：41 条输入 = 本域定稿覆盖 37 条 + 整条移交 4 条；37 条收敛为 **29 条定稿**（并档 5 组：A-02←L03-002+017、B-07←L03-005+L15-012、F-01←L03-006+L15-013、G-04←L03-007+L15-021、E-01←L03-011+L15-018；一分为二 2 处：L03-012→B-05+F-03、L15-021→G-04+H-02；多合一 2 处：L15-008/009/010/014→B-08（登记面四例）、L15-007/011→B-10（跨域条款锚错挂））。**无静默丢弃**。
---

## 二、行号漂移处置

1. 本包全部证据锚改写成「path::符号」，行号只作辅助；**行号与文档不符一律不作为剔除理由**（协议 §5）。
2. 并发提交造成的实际漂移（本次以符号名重新定位成功，逐条量得）：
   - `lib/astro_image_io/src/hips/aio_hips_writer.cpp` 由约 1806 行增至 **1953 行**：`aio_hips_product_begin` 420→**466**、`write_moc_fits` 247→**293**、`finalize_image_product` 886→**约 940**、SNR metadata 段 1092→**1246-1261**、manifest 段 1485→**1466-1514**、`aio_hips_abort` 1354→**1557**；
   - `lib/core/src/module_adapters.cpp`：p1 写链注释 2419-2420、新守卫 2453、begin 调用 2476；p2 注释 3913、begin 调用 3974；
   - `docs/standards/STANDARDS_REGISTRY.md` 行号整体后移约 7 行（D.hips 清单 :93-99、偏差表 :105-117、§3 索引 :239-265、§5 :292-318）；
   - `runtime/io/hips_core.c`：nside 计算 :245、tile 反算 :558-559。
3. 因漂移而失效的引用**已就地剔除并留痕**（第三节 A 表 C1–C4），避免下游再引用失效句。
4. **文档侧行锚漂移本身定稿为 M2b-E-01（P1）**：偏移 +25 ~ +424 行；现行门（C2 可解析 / C3 界内 / C4 声明式绑定）结构上看不见漂移，根因与门改造移交 M5/M6b（总述归 M6b-E-002，本包只交付域内实例与偏移量）。
5. 交叉引用完整性已机检：本包 29 条定稿的内部 `M2b-*` 引用**无一悬空**（defined/referenced 全集比对）。

---

## 三、剔除与降级显式清单（不静默丢弃）

### A. 本包定稿中被本次复核推翻或改写的引用（4 处，均已改写并在条目内留痕）

| # | 条目 | 原引证 | 复核结果 | 处置 |
|---|---|---|---|---|
| C1 | M2b-C-01 | 「lib/astro_image_io 全目录 rename/fsync/staging **0 命中**」 | **错**（跨作用域误计）：全库 rename 命中 50 行（aio_pipeline.cpp:876「原子保存: 先写临时文件, flush 后 rename」、:917 std::rename）、atomic 15 行（hiss_stream_writer.cpp:178 atomic_replace、:111 .partial）、staging 1 行（ltmain.sh） | 作用域收窄为 **lib/astro_image_io/src/hips/**（此处 0 命中成立）；结论改写为「**原子能力已在同库 HISS/缓存路径在用，唯独 HiPS 写出未接线**」——事实更准且更硬 |
| C2 | M2b-C-01 | DISP-HIPS-004「原文」引用含「能力缺口（非合同违规）…§8 登记事实=发布层当前无生产消费者」 | 该句在注册表与 ALG 中 ripgrep **0 命中**（「能力」「缺口」「非合同违规」「无生产消费者」在 STANDARDS_REGISTRY/HIPS_WRITER/IO_003 均 0），系把 L03/L10 的转述当作原文 | 换成实测整行原文：注册表 :109（151 字符）+ ALG §10 DISP-HIPS-004 行（612 字符，含「writer 层不冒认已原子」「对照：HISS 容器有 .partial/.tmppool+atomic_replace 但 writer 未采用」） |
| C3 | M2b-A-01 | aio_hips.h「nside: leaf-level NSIDE (power of 2, >=512; non-power-of-two is rejected)」 | 该英文句当前树 **0 命中**（已被并发改写为 :101「// nside - 叶级 NSIDE (2 的幂, >= 512)」） | 换用现文本；「只承诺、不校验」结论不变，并在条目内保留订正记录 |
| C4 | M2b-B-01 | 读侧实现点含 `modules/services/io/src/hips_input_v1.c`、`lib/phase3_session/hips_reader_p3.c` | 两文件在非 run 区**已不存在**（glob **/hips_reader* 仅命中 run/ 影子树；modules/services/io 下只剩 hips_input_v1.h + tests/hips_core_selftest.c） | 删去该两点，改用整行计数重列的 **13 处**真实落点（生产 3 / 夹具与 Oracle 6 / 浏览器与工具 4） |

> 口径说明：C1 属**跨目录计数错误**，C2 属**把他人转述当原文**，C3/C4 属**并发漂移**。前台警示的 read 单行 >2000 字符截断风险，本包以「一律 ripgrep 整行匹配 + 键集合/列序定位」规避；B-08 的「ID 是否挂在清单行」判定为此专门重算（read 截断恰好会砍掉末列的偏差列，用行文本判会得出假挂空），结论未变但口径已在条内写明。

### B. 判级与类别调整（逐条附依据，非妥协性降级）

| 输入 | 原 | 现 | 理由 |
|---|---|---|---|
| L03-006 / L15-013 | P1 F_TEST_GAP | **P0** F_TEST_GAP | 它是 D.healpix「COMPLIANCE: CONFORMANT + 偏差无」的唯一支撑，并被 SCI/ALG/注册表三面引用；证据在仓内不存在 ⇒ 符合性声明无据（宪章 §17.11） |
| L03-014 | P2 H_NUMERIC | **P1** H_NUMERIC | 不只是精度：深 order 归零使 properties 与同目录非空 MOC 直接矛盾，并违反自设冻结容差 <1e-9。按前台要求区分「标准违反 vs 精度损失」⇒ 结论：**非 IVOA 条款违反**（标准只规定 [0,1] 实数、未定小数位），属精度损失 + 自设合同违反 + 覆盖声明失真 |
| L03-007 | B_STD_MISMATCH | **G_GOV_GATE** | 属依赖/出处登记合规簇（许可定性待负责人），不是国际标准条款问题；与 L25-001/002 并簇，许可证面本体归 M8 |
| L03-015 | A_SCI_DEF | **H_NUMERIC** | 事实是静默数值收口（合法值哨兵 + 位移夹紧），宪章 §13.5 更贴切 |
| L03-016 | B_STD_MISMATCH | **C_DOC_CODE_GAP** | 「1.4 是否合法」已由标准原文闭环，剩余是版本基线错引 + 四层口径并存（文档↔代码） |
| L15-009 / L15-010 | E_TRACE_BREAK | **B_STD_MISMATCH** | 是登记面失真，不是追溯链断头；真正的追溯断头（行锚）单立 E-01 |
| L15-021 | D_COMMENT | **G_GOV_GATE + H_NUMERIC 两条** | 出处/许可冲突入治理簇；静默收口入数值面；纯注释卫生（断链 URL）并入 G-04 建议③，不单立以免碎片化 |
| L15-002 | P0（维持） | P0 | 交付元数据 3600 倍错值属产品级错误（非注释级），不下调 |
| L15-005 的 hips_hierarchy 子项 | — | 置信度降为**中** | 标准正文键表未列该键取值域，仅示例 properties 的 `hips_hierarchy = mean` 支撑 ⇒ 事实硬、结论强度中 |

### C. 子项剔除与「不另立」判定（整条不剔）

- L03-016 的「1.4 是否合法取值」疑问：HiPS 1.0 §4.4.1 原文「hips_version – Format: word "1.4" corresponds to this document」⇒ 1.4 合法，剔除疑问、保留版本基线错引事实。
- L03 §6 待复核项「moc_sky_fraction 是否标准键」：§4.4.1 键 8「moc_sky_fraction … Format: real between 0 and 1」⇒ 是标准键，剔除。
- L03-004 的「三处调用点注释」中 2 处（astro_sphere_sink、hips_p2 侧）：并发中已消失 ⇒ 移入第四节，条目改判部分修复、只报残留。
- L03-019 的「计数值可能不等于文件总数」推测：正常路径下 diag tile 数 = 叶级 tile 数，该推测不成立 ⇒ 不写入定稿；只保留「计数只看 flag、不看 present，可与同一 manifest 的 products 清单互相否定」这一实测事实。
- L15-019 中「§4 表仅 7 行、catalog 不在内」的表述：实测 catalog **已在 §2 域表 :45 与 checker FROZEN_DOMAINS :49-50 内**（叶子级把 §2 域表与 §4「与宪章 §19 文献锚的对应」表混为一谈）⇒ 改写为真实缺口「catalog 缺宪章 §19 锚 + 缺 §4 对应行（:282-288 实测 7 行无 catalog）」。
- L03-009 的回归缺口：**判定不另立 F_TEST_GAP**——其修复已由 hips_writer_adapter dlopen 用例间接钉住，且 A-01 建议③/④ 与 F-02 建议④ 已覆盖 writer 侧直测需求；在此显式记录以免被视为静默丢弃。
- 前台或他人转述的数字一律未直接采用：本包所有数字（27 个登记 ID / 32 条清单行 / 19 个挂上 / 8 个挂空 / 13 处 tile 命名 / 9 个标准必填键 / 25 TRACKED / 13 个 FITS 错误码 / 230 行 / 3 行 4 处断链 URI）均自算并附口径。
- 总述级判词一律不写：无据 VERIFIED、行锚系统性、追溯双头、证据链三断点、不可达门、平台盲区分别归 M6b-G-001 / M6b-E-002 / M6b-E-001 / M8 / M7 / M9，本包条目仅 related 指回。

---

## 四、已修复表（并发提交期间修复，按协议不入 findings）

| 项 | 关联输入 | 实测证据 | 回归测试是否在位 | 残留处置 |
|---|---|---|---|---|
| F1 非 2 的幂 / 越界 HEALPix 入参改抛 std::invalid_argument | L03-005、L15-012 | healpix_core.h:25「nside == 0 或非 2 的幂: 抛 std::invalid_argument (R9-B: 禁止静默向上取整)」；require_valid_nside 调用点 cpp:234/:251/:441 | **在位**：test_healpix_neighbors.cpp:195-210、:222-223；p1hips negative 组 14 例 | order 上界与 npix 回绕 ⇒ M2b-B-07 |
| F2 lib/hips 构建闭包补入 aio_hips_reader.cpp | L03-009 | lib/hips/CMakeLists.txt:36 RESCUE-V3 FD-01 更正段 | 间接在位：tests/unit/CMakeLists.txt 的 hips_writer_adapter（dlopen 符号解析，:544-581） | 旧注释残留 ⇒ M2b-C-03 |
| F3 nrej/nused 诊断通道 manifest 只记真正写过的 | L03-018 | writer :1471-1481「诊断平面只在真正写过 tile 时进入 products 清单」 | 部分在位（p1hips_tests_diag_prov.cpp 12 例） | variance/ivar 仍无 present ⇒ M2b-F-02；计数字段仍只看 flag ⇒ M2b-C-06 |
| F4 provenance properties ↔ manifest 双向往返守卫 | L03-018 相关 | writer :1517 起 prov 段 + verify V6（:1811-1842 双写面值一致性） | 在位（diag/prov 用例） | 仅 prov_set 时生效 ⇒ M2b-F-02 |
| F5 aio_hips.h 的「Atomic publish / staging→promoted→COMPLETE」虚假承诺块删除 | L03-004 | ripgrep「原子|publish|staging|promoted|COMPLETE|atomic|rename」于 lib/astro_image_io/include → 仅 1 命中（nside 行） | 不适用（注释） | 残留 module_adapters 3 处注释 ⇒ M2b-C-01 |
| F6 astro_sphere_sink.cpp / lib/hips_p2 侧「AIO-002 原子发布原语内建」措辞消失 | L03-004 | sink grep「原子|AIO-002|IO-003|发布」= 0；lib/hips_p2/** 只剩 module.yaml/README.md/memory.md（src 不存在） | 不适用 | 载体消失即修复；模块归属问题 ⇒ M2b-G-01 |
| F7 适配层新增部分 nside 校验（整除 + tile_nside 幂次 fail-closed） | L03-001 | module_adapters.cpp:2453-2461 | 未见叶级 nside 幂次直测（p1_nside ctest :602-608 只覆盖 writer 侧 256/511） | 叶级 nside 非幂次仍放行 ⇒ M2b-A-01 仍成立 |

> 结论：**整条已被修复 0 条**（7 项均为子项修复），故 findings 内无「已被修复」条目；F7 无直测但不另立（理由见第三节 C 末项）。
---

## 五、移交清单

| 输入/子项 | 移交给 | 理由 | 本包已备证据（可直接取用） |
|---|---|---|---|
| L15-004（Gaia 参考历元 J2016.0 / UR-1 语义） | M2a | D.catalog 与 Gaia 查询域 | GAIA_QUERY.md:75-76 锚实漂 +93（inv_scale 实测在 gaia_client.c:1375/:1504/:1643）；D.catalog :186/:193 仍写 J2016.0 |
| L15-006 代码语义面（占位卡与 verify 逻辑整改） | M2a（与本包 B-09 会签） | 真源 runtime/io/fits_core.c 属 IO/FITS 实现域 | M2b-B-09 已给行位（:1151-1156 / :1261-1270 / :1449 / :1651）与测试默认值（test_fits_stream_contract.py:162 checksum=0） |
| L15-007 代码语义面 | M2a | 同上 | 条款号对照表在 M2b-B-10 |
| L15-011 语义面（θ₀ 符号、LONPOLE 默认与 CRVAL2<0 不等价） | M1a / M7 | Phase3 投影实现与符合性终裁 | ar5iv 实测小节名（§2.2 Reference point / §2.3 Spherical coordinate rotation）；p3_projection.cpp kMaxAbsDec=85.0 与 1≤r≤2 漏判面 |
| L15-015（1-based 桥接第二处实现） | M1a | Phase3 投影实现 | p3_projection.cpp:73-74/:86-87 裸 ±1.0；p3_wcs.cpp::fits_pixel_1based 为 fail-closed 正解 |
| L15-016（TAN 正/反向半球界不等价） | M1a / M7 | 需解析证明 + Oracle 设计 | 正向 denom<=0（:120）、逆向 r>=M_PI/2（:136）、注释 :22 仍写「数学等价」 |
| L15-017（CAR/AIT CRVAL2 不进映射） | M1a | 投影域 | :23-25 自述「CRVAL2 仅记录于 header 不进入映射」+ :200-205/:212-217 恒等映射 + :389 仍写 CRVAL2 卡 |
| L15-018 门侧（行锚检查器判据改造） | M5 / M6b（总述 M6b-E-002） | 门禁与文档权威链治理 | check_doc_line_anchors.py 判据实况（C2 解析 / C3 界内 / C4 声明式绑定，docstring :13-17 原文）；anchor_contract.json bindings 41 条中 HIPS_WRITER 仅 3 条、不含 writer.cpp；该门在 ci/checks.json:3301 **有**执行面（与注册表检查器不同，勿混） |
| L15-018 GAIA_QUERY / PHASE3_PROJ_IMPL 文档订正 | M2a / M1a | 各自文档域 | 偏移量表在 M2b-E-01 |
| L25-001 / L25-002（依赖许可证与版本自述冲突总簇） | M8（前台指定） | 跨全域依赖合规面 | M2b-G-04 已按本域（HEALPix 模块）登记实例并 related 指回，不重复制判 |
| L03-018 可测试性轴（声明↔产物一致性） | L23 / M2a | 跨切片主题 | 恒真断言行位（:79-80/:89/:94-95）与 variance/ivar 缺 present 的判据在 M2b-F-02 |
| L03-014 单位与量纲横向面 | L26 / M9 | 单位口径主题 | properties 6 位小数、manifest %.8f、ALG <1e-9 三向口径实测在 M2b-H-01 |
| IO_003 合同侧（原子发布契约与 writer 责任边界） | M2a | 合同域 | 本包只持写出实现与注释事实（M2b-C-01），条内已声明「写侧我持、合同侧 M2a 持」 |
| HiPS 写出归属裁决（lib/hips / lib/hips_p2 vs 宪章 §5.2 指定目录） | 前台 → 项目负责人 | 宪章 §1.2 变更权仅在负责人 | M2b-G-01 已列全部事实（lib/infrastructure/** 0 文件；DLL 已进 packaging required_units；无任何架构偏差登记） |
| DISP-HIPS-004 的整改选项二选一（INT 层接线 vs writer 内嵌事务 vs tree hash 归属） | 前台 → 负责人（tree hash 归属）+ M2a（IO_003） | ALG 明写「tree hash 归属裁决」 | M2b-C-01 已给出现成可复用原语清单（aio_publish.cpp / hiss atomic_replace / aio_pipeline 临时+rename） |

---

## 六、复算取证（标准原文与自算数字，全部带口径与 URL）

| 复算项 | 口径 | 结果 |
|---|---|---|
| HiPS tile 目录算式 | IVOA HiPS 1.0 §4.1 正文 + 示例（官方 PDF 经文本层取回） | 「Tile N in order K → Norder K/Dir D/Npix N{.ext} where D=( N/10000)*10000 (integer division)」；示例 cell 10302 @order6 → Norder6/Dir10000/Npix10302；URL: https://www.ivoa.net/documents/HiPS/20170519/REC-HIPS-1.0-20170519.pdf |
| properties 必填键数 | 同一文档 §4.4.1 逐条计数 | 原文「There are 9 mandatory keywords」：creator_did / obs_title / dataproduct_type / hips_version / hips_release_date / hips_status / hips_tile_format / hips_order / hips_frame（另条件键 dataproduct_subtype、hips_cube_depth）；对照实现写 5 键、IO_002 只要求 5 键 |
| hips_pixel_scale 单位 | §4.4.1 键位与 Unit 行 | 「hips_pixel_scale … Unit : degrees」⇒ 实现写角秒 = 3600 倍错 |
| obs_bandpass 是否存在 | HiPS REC 全文关键词计数（obs_bandpass、bandpass） | **0 命中** ⇒ STD-F4 以「标准推荐键」名义引用不存在的标准对象 |
| hips_status 词表 | §4.4.1 hips_status Format 行 | 三组空白分隔受控词（private|public）（master|mirror|partial）（clonable|unclonable|clonableOnce）；实现只写两组 |
| Moc.fits 强制键 | REC-MOC 2.0 §6 Table 3 逐行（HTML 直取） | ORDERING（NUNIQ/RANGE）与 COORDSYS（须为 'C'）在 **MOC1.1 与 MOC2.0 两列都是 mandatory**；PIXTYPE 与 MOCORDER「自 1.1 起不再需要」；URL: https://www.ivoa.net/documents/MOC/20220727/REC-MOC-2.0-20220727.html |
| order ≤ 29 的出处 | Górski 2005 arXiv HTML 全文计数 | 「29」**0 次**、「power of」**0 次**；§5 引言只有 Npix=12·N²side 与 Ωpix=π/(3N²side)，小节为 5.1 Pixel Positions / 5.2 Pixel Indexing / 5.3 Pixel Boundaries；URL: https://arxiv.org/html/astro-ph/0409513 |
| tile NSIDE 与 order 关系 | HiPS 1.0 §4.2.1.1 像素角尺寸式 | 「sqrt( 4*PI / (12 x ( tileWidth x 2^order )² )）」⇒ NSIDE = TW·2^K（TW=512 时 =2^(K+9)）⇒ ALG (4c) 的 NSIDE=2^k 为错 |
| FITS 4.0 章节 | 官方 PDF 目录逐条 | §4.4.1 Mandatory keywords（p.10）/ §4.4.2 Other reserved keywords（p.12，CHECKSUM/DATASUM 在此）/ §5 Data representation / **§6 Random-groups structure（p.16）** / §7 Standard extensions / **附录 J CHECKSUM implementation guidelines**；URL: https://fits.gsfc.nasa.gov/standard40/fits_standard40aa-le.pdf |
| WCS Paper II 小节 | ar5iv 渲染正文小节标题串 | 2.1 Spherical projection / **2.2 Reference point of the projection（LONPOLE 在此节）** / 2.3 Spherical coordinate rotation；URL: https://ar5iv.labs.arxiv.org/html/astro-ph/0207413 |
| tile 反算式落点数 | ripgrep 整行，pattern `ipix / 10000|ipix % 10000|10000u`，作用域全仓并排除 run/、问题扫描/、docs/、third_party/ | **13 处**：生产 3（writer/reader/runtime）+ 测试与 Oracle 6 + 浏览器与工具 4 |
| 注册表登记基数 | ripgrep 整行双向匹配（`^\| (DISP|STD)-` 取定义集；`(CONFORMANT|PARTIAL|NON_CONFORMANT|PROJECT_DEFINED|NOT_APPLICABLE).*<ID>` 判「是否挂在清单行」） | §3 索引 **27 行**（25 TRACKED、1 带尾巴的 OPEN、1 越值域字面量，**CLOSED=0**）；六域清单行 **32 行**；挂上清单行 **19 个 ID**、挂空 **8 个**；CONFORMANT 行挂开放 ID **7 行 / 9 个 ID**；另 §4「与宪章 §19 文献锚的对应」表 **7 行无 catalog**；checker 断言 **27 处覆盖 C1–C7，无 C8**；ci/checks.json grep check_standards_registry = **0** |
| HiPS 写出原子性 | ripgrep 整行，作用域 lib/astro_image_io/src/hips/ | rename / fsync / staging / COMPLETE / aio_publish / .tmp / atomic **全部 0 命中**；对照 lib/astro_image_io 全域：rename 50、atomic 15、.tmp 52、fsync 2、staging 1（且都在 HISS/缓存/ltmain，与 HiPS 无关） |
| 宪章指定归属目录 | glob lib/infrastructure/** | **0 文件** |
| FITS 写接口错误码数 | 逐值枚举 modules/services/io/include/astrocs/io/fits_stream_v1.h:51-64 | **13 个错误码**（ACS_FIO_ERR_PARAM…ACS_FIO_ERR_DISKFULL），含 OK 与 STATUS_COUNT 哨兵共 14 值；注册表 :224 写「17 码」 |
| p3_wcs.cpp 规模 | read 工具 totalLines（行计数口径，非文本比对） | **230 行**；PHASE3_PROJ_IMPL.md:12 写「165 行，实测 2026-09-11」 |
| Oracle 资产 | glob healpix_fullsky_oracle* / **/oracle.jsonl（排除 run/）+ tests/unit/CMakeLists.txt add_test 枚举 | 两者均 **0 命中**；healpix 唯一生成器是 gen_spatial_fuzz.py（默认 --order 7 单阶、标 NON_PRODUCTION_TOOL_ONLY）；tests/unit/CMakeLists.txt 中 healpix 相关 ctest 仅 r9b_healpix_neighbors（:612-614），p1 侧有 p1_hips_writer（:616-620）与 p1_nside（:602-608） |
| SNR metadata.xml 断链 URI | ripgrep 计数口径 = **字面量出现次数**（区别于「行数」口径：3 行命中、4 处出现，:1251 同行 2 处） | 「http:// ivoa.net…」×3 + 「http:// www.w3.org…」×1 ⇒ **4 处** |

---

## 七、给前台的一句话

本域最硬的三块：① **tile 命名与 IVOA 反**（13 处同式，改一处即全线不一致，M2b-B-01）；② **D.healpix 判 CONFORMANT 所依据的百万点独立 Oracle 在仓内不存在**（M2b-F-01，已由 P1 升 P0）；③ **HiPS 写出不是没有原子发布能力，而是写出路径没接线，且三处注释仍在替它冒认**（M2b-C-01，部分修复后仍 P0）。