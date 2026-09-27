# 检查-S3-scienceB.md —— 全仓-01 切片 S3「docs/science 乙组」对抗式只读静态检查

- **切片责任域（逐份通读，无遗漏）**：
  `docs/science/NOISE_MODEL.md`(412 行)、`docs/science/PHASE2_UPM.md`(552)、
  `docs/science/PHASE3_HIPS_TO_FITS.md`(236)、`docs/science/PHOTOMETRY.md`(432)、
  `docs/science/PSF.md`(211)、`docs/science/PSF_SIGNAL_WEIGHT.md`(162)。
- **方法**：①-④ 四面逐份检查；`file:line` 锚**逐条实开文件核对**（不采信文档自述）；
  关键数值独立复算（有限 N 序统计量 MC、sec²γ、序数换算、闭式常数）；
  文献题录 web/arXiv/DOI 抽验；与 `实验/absolute-snr`、`实验/photometric-magnitude`
  成稿及 `run/SEAM-DERIV-01` 证据文件交叉核对。
- **去重基线**：`独立审计/实验重做/总编对账/检查-修复验证.md` PASS 表（红 8 项、黄 16 项、
  行漂移 6 处 a–f）——**已 PASS 项不重复报**；科学争议只依 `分歧台账.md` D-*/A-* 终裁，不重开。
  `检查-修复验证.md §二` 已登记的遗留观察（NOISE_MODEL:302 锚 `noise_model.cpp:530-585` 已失准等）
  **不计为新发现**，本文只在对应面提及其“已登记、本轮复核仍未修”。
- **纪律**：本切片零 git 写、零构建/测试/磁盘写入；本报告为唯一写入文件。
  涉及科学公式/默认容差/冻结定义的红级问题**只登证据与建议方向**，不代拟正文。

---

## 一、红级（must fix）

### S3B-红1｜PHASE2_UPM.md:515-516：「f < 1/2 恒有 median = 0 ⇒ 结构性失明」缺 A-P5-05 裁决要求的限定条件
- **文件:行**：`docs/science/PHASE2_UPM.md:515-516`（§17.3 漏检面第 4 条）
- **问题**：条文**无条件**断言「边界上只有比例 `f` 的采样点承载台阶时 `f < 1/2` **恒有** `median = 0` ⇒
  对短于边界一半的局域接缝**结构性失明**」。该断言只在**无噪极限**成立；本文件 §17.1 自己给的零假设
  是 `seam_i = [背景梯度项] + [ε₊ − ε₋]`（噪声项在内），带噪下 `f<1/2` 时中位数由无台阶分量 + 噪声决定，
  读数是**围绕 0 的随机量**、并非恒 0，「结构性失明」不成立。
- **证据（含反方核验）**：
  - 终裁：`独立审计/实验重做/总编对账/分歧台账.md:173` **A-P5-05**：「f<1/2 中位数结构性失明」**仅在无噪极限成立**；
    M42 级噪声下 `f=0.3、Δ/L=5%` 读数已达 `1.03e-2`（**到门**）⇒ **➕ 采信（限定条件补入条文）**。
  - 限定语已在实验单元落地：`实验/additive-sky-seamless` 成稿（seam-gate-floor / REPORT_paper）写明限定；
    `独立审计/实验重做/总编对账/五单元成稿简报.md:212/:243` 两次要求「f<1/2 失明**仅无噪极限**」。
  - 反方核验（穷尽）：`grep -n "结构性失明|无噪极限|f=0.3|1.03e-2" docs/science/` → **仅命中 PHASE2_UPM.md:516 一条**，
    全 docs/science 无任何限定语；`grep 三份前轮检查报告`（检查-科学性/行文逻辑/跨文档冲突）→ **0 命中**，
    非前轮已报项；本文件 :496-497 的「确定性下限（无噪声硬界）」是另一条（Δ/L 门），不构成第 4 条的限定。
  - 与本文件内部一致性的反证：§17.3 :506-508 自己给的是**带噪概率性**检出表（17%/70%/96.7%…），
    第 4 条却切回确定性叙述，两条相邻条目的噪声前提不一致。
- **建议改法**：按 A-P5-05 已生效的终裁，为第 4 条补「仅无噪极限成立」的限定与 M42 级反例（f=0.3 到门）指针；
  属冻结文档条文修订，**走变更流程**，本切片不代拟定稿文本。
- **所属面**：①科学性（判据适用域/非退化判据的前提条件）＋②行文逻辑（与 §17.1 自相矛盾）

### S3B-红2｜PSF_SIGNAL_WEIGHT.md:127：「6 等（10 倍通量）」星等↔通量换算错误（差约 25 倍）
- **文件:行**：`docs/science/PSF_SIGNAL_WEIGHT.md:127`（§7a 第 3 条 F_ref 锚定）
- **问题**：「锚定权重…在 `|m−m_ref|≤4` 内 ≤0.6%、**6 等（10 倍通量）**处 3.6%」——
  6 等对应的通量比是 `10^(0.4×6) = 251.2×`，**10 倍通量只对应 2.5 等**；括注把 3.6% 所属的通量域
  说小了约 25 倍，直接影响「该在哪个源电平域引用 3.6%」的判读。
- **证据（含反方核验）**：
  - 出处逐值核对：`实验/absolute-snr/results/b4_integration.json` —— `"m_ref": 6.0`（:76）；
    被引的两格在 `mag:12.0` 段：`"Phi": 0.003981071705534969`（=10^(−0.4·12)），`"anchored_over_oracle": 1.0362212838018146`
    （→ +3.62% ≈ 3.6%），前一格（mag 语境 10.0）为 `1.0058936695279836`（→ +0.59% ≈ ≤0.6%）。
    即 3.6% 属 **Δm = 6 mag（比 m_ref 暗 251×）**，不是 10×；`m=6.0` 行比值恰为 1.0000（零惩罚）。
  - 既有登记（同向、未修）：`独立审计/证据/AUD-101-DB-16.md:252` 已以 ⚠⚠ 登记该句：
    「3.6% 属 m=12.0 行…比 m_ref=6 **暗 251 倍**、不是 10 倍…正本的星等↔通量映射与本表矛盾」。
  - 反方核验：`grep -n "10 倍通量" 实验/absolute-snr/{README,REPORT_paper,REPORT_experiment}.md` → 0，
    现行成稿**未**沿用该括注（只有 LEGACY 件与本正本残留）；数值本身（≤0.6% / 3.6% / 30.1%）
    与 `b4_integration.json`、`COMPARISON_TABLES T6a/T6b` 逐位相符 ⇒ **错的是括注的通量换算，不是数值**。
- **建议改法**：按变更流程订正括注（保留 Δm=6 的星等口径与 3.6% 数值，改写通量换算：6 等 = 251×、
  10× = 2.5 等），或直接去掉括注只留星等口径；订正后与 AUD-101-DB-16:252 一并闭环。
- **所属面**：①科学性（单位/换算：星等↔通量映射）

---

## 二、黄级（suggest）

### S3B-黄1｜NOISE_MODEL.md：源码行号锚系统性漂移（7 处，行为断言本身正确）
- **文件:行**：`docs/science/NOISE_MODEL.md:31 / :55 / :84 / :303 / :304 / :317 / :280`
- **问题**：锚写的是旧文件结构（旧文件约 938 行时代），源文件重构后行号整体前移/后移，逐条实开均**对不上**；
  同表/同句的行为描述与现代码**语义一致**（⇒ 不判红，判黄＝锚面失效，读者按锚核不回）。
- **证据（逐条，实开 `lib/algorithms/noise_snr/cpp/src/noise_model.cpp` 现 1482 行）**：
  | 文档锚 | 文档主张 | 源码实况 | 判定 |
  |---|---|---|---|
  | :31 | `g_model_floor` 注册表锚 `noise_model.cpp:33,48,55,86,903` | `grep -n g_model_floor` → :33 ✓；注册 :50/:57、读 :70-71、擦除 :88-89；**:48/:55/:86/:903 四锚均无该符号**（:903 为无关闭合括号） | ✗ 5 中 1 |
  | :55 | floor 拒绝 `build :369-371` / `fill :818` | build 侧 = :804-806（:804 注释「非法 floor ⇒ SNR_FLOOR_UNBOUND(-10)」、:806 return）；fill 侧 = :1362（`registry_get_floor` 失败）、:1433（`!isfinite(floor_var)||<=0`）；:369-371 = null_space_basis 注释区、:818 = 空行 | ✗ |
  | :84 | 「与 `noise_model.cpp:1-938` 一致（`fill_impl :776-866`；`snr_noise_model_v1_fill :868-883`）」 | 文件现 1482 行；`fill_impl` 定义 :1323、`snr_noise_model_v1_fill` :1412 | ✗（既有登记同向：`独立审计/证据/AUD-101-DA02-算法推导.md:617` 记「`:776-866 → 1412`」，本轮复核仍未修） |
  | :303 | 拒绝证据锚 `noise_model.cpp:369-371,818,889` | 三锚全非 floor 判据（:889 ∈ sky_budget 可行性判定；正确处见 :804-806/:1362/:1433） | ✗ |
  | :304 | `gain<=0 → snr_noise_gain_variance 返回 0` 锚 `:928-936（判据 :931）` | `snr_noise_gain_variance` 定义 :1472、判据与 return 0 在 :1475；:928-936 = 手工掩膜天空预算区 | ✗（行为 ✓：`:1475 return 0` 与文档主张一致） |
  | :317 | 「平面预测 ≤0 ⇒ 不可用」锚 `noise_model.cpp:830-864` | 该语义在 `fill_impl` 逐像素循环 :1364-1405（`pred>0` 才写正值，否则写 0/0，注释 :1366-1375 逐句对应文档）；:830-864 属掩膜/预算规划段 | ✗（行为 ✓） |
  | :280 | orchestrator psf 9 列行字段语义锚 `orchestrator.cpp:4558-4562` | 该处为 `photo_stats` 字符串落键；实际列赋值在 `lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp:2389`（row[2]=flux）、:2392（row[5]=平均 FWHM）、:2393（row[6]=A） | ✗（主张 ✓：row[2]/[5]/[6] 语义与文档一致） |
- **反方核验**：`grep 检查-修复验证.md` → 黄/红表无上述 6 锚（行漂移 e 只 PASS 了 NOISE_MODEL:**301** → :830-930/:1262，
  与 :317 是不同行的同名区间，不构成对 :317 的 PASS）；`grep 三份前轮检查报告` → 0 命中。
- **建议改法**：按 `docs/algorithms/anchors/ANCHOR_CONTRACT.md` 的锚纪律，一次性把上表 7 处改到现址
  （改锚不改科学内容，属锚维护）；:302 行 `collect_patch_sky :530-585` 漂移已在 `检查-修复验证 §二2` 登记，随批同修。
- **所属面**：④幻觉与锚

### S3B-黄2｜PHASE2_UPM.md：源码行号锚系统性漂移（§4/§5/§8/§16.3 共 15 处，行为断言经实开核为真）
- **文件:行**：`docs/science/PHASE2_UPM.md:62-63、:139、:142、:144、:146、:158、:278、:279、:281、:283、:423、:424、:425、:426、:427`
- **证据（逐条，实开 `lib/algorithms/coverage/src/{upm.cpp,sampler.cpp,sky_plane.cpp}`、
  `lib/algorithms/coverage/include/astro/phase2/{upm.h,sky_plane.h}`、`lib/infrastructure/scheduler/src/module_adapters.cpp`）**：

  | 文档行 | 文档锚/主张 | 源码实况 | 判定 |
  |---|---|---|---|
  | :62-63 | 写盘门 FZ-PROV-KCORR `upm.cpp:2265-2275` | 门在 :2486-2490（`k_corr_applicability_domain` 空 ⇒ rc=7）、序列化 :2930-2933；:2265-2276 为 Jacobi 特征分解 | ✗ |
  | :139 | 步长判据 `upm.cpp:1021-1040` | 判据式在 :1050（:1034-1040 仅容差构造，区间缺判据本体） | △ 部分 |
  | :142 | 目标判据（stalled 连续 5 次）`upm.cpp:1028-1050` | 阈值 `kObjImproveFloor=1e-12`/`kStallPatience=5` 在 :732-733，stall 逻辑 :1054-1063，均在区间外 | ✗ |
  | :144 | `scale_obs` `upm.cpp:706-716` | 实际赋值 :729（注释 :717-718） | ✗ |
  | :146 | 只读访问器 `p2_upm_convergence` `upm.cpp:1577-1589` | 实际定义 :1893-1905 | ✗ |
  | :158 | `model_hash` 序列化 `upm.cpp:1155-1175` | 写 :1208-1209、读 :1555-1559（`j.value("model_hash",…)`） | ✗ |
  | :278 | `n_obs=0 → rc=1` 锚 `upm.cpp:215`/`:246-249` | rc=1 在 `p2_upm_build` :258；:215 = `evaluate_c_field`、:246-249 = `huber_w` | ✗ |
  | :279 | `p2_upm_raw_weight rc=2` 锚 `upm.cpp:1665-1670` | `p2_upm_raw_weight` :1966、return 2 :1991 | ✗ |
  | :281 | 单帧区 harmonic `upm.cpp:1054-1090；upm.h:99-101` | `harmonic_extend` :1080、调用 :1101-1102（区间外）；`upm.h:99-101` = 阻尼 α 注释，harmonic 在 `upm.h:127/:393` | ✗ 双锚 |
  | :282 | 畸形模型 `upm.cpp:1304-1567` | `p2_upm_open` :1506 落在区间内 | ✓ |
  | :283 | 跨 endian payload 锚 `sampler.cpp:257` | :257 = RA 窗口循环；`grep -n endian lib/algorithms/coverage/` → **0**，锚指错对象 | ✗ |
  | :423 | `module_adapters.cpp:7808-7813` 取 tolerance=1e-6 ∧ tolerance_relative=1 | 实际 :9571/:9573（两键同段），行为 ✓ | ✗ 锚 |
  | :424 | `upm.cpp:1028-1050 / :1577-1589`、`module_adapters.cpp:7928-7935/:7963/:7997` | 实际 :9690-9695（converged 读入）/:9734（manifest）/:9800；upm 两锚同上 | ✗ 锚 |
  | :425 | `module_adapters.cpp:8807-8864`（节点间距自适应 + provenance） | 实际 :9870-9961（`p2_sky_plane_derive_node_spacing` 与 manifest 键落盘）；`sky_plane.cpp:1082-1135` ✓ 实开相符 | △ 一错一✓ |
  | :426 | `upm.h:293-295`（同一 rank_rtol 符号/值/实现） | `rank_rtol` 字段在 `upm.h:292`（:293-295 = gauge_mode 注释）；`sky_plane.h:217-222` ✓ 完全相符 | ✗（差 1–3 行） |
  | :427 | `module_adapters.cpp:7797-7808` 与 `:7809-7813` 两段互斥注释 | 实际 :9557-9573：前段（CONFORM-FIX-B-001「相对判据待裁决」）与后段（SCI-502 FIX-1「定案」启用 relative）**同函数并存，互斥属实** | ✗ 锚、✓ 行为 |

- **反方核验**：行漂移专表 a–f（`检查-修复验证.md:46-55`）只 PASS 了 PHASE2_UPM:**43-44、:23、:25**，不含上表任何一处；
  `grep -n "7808-7813|2265-2275" 独立审计/` → 仅 `复核-AUD203.md:165`（另批次）已记 §16.3 同一漂移、
  `c10_anchor_forensics.json` 为证据快照 ⇒ 非本轮重复报 PASS 项；`grep 三份前轮检查报告` → 0。
- **建议改法**：把上表 13 个失效锚一次改到现址（:2486/:2930、:1050、:1054-1063、:729、:1893-1905、:1208/:1555、
  :258、:1966/:1991、:1080-1102 与 upm.h:127/:393、（endian 项先补出真正的证据源）、:9571-9573、:9690-9800、
  :9870-9961、upm.h:292、:9557-9573）；行为断言已逐条实开为真，**不改语义**。
- **所属面**：④幻觉与锚

### S3B-黄3｜PHASE3_HIPS_TO_FITS.md §16：C4/C5 的 module_adapters 与根 CMake 锚整体漂移
- **文件:行**：`docs/science/PHASE3_HIPS_TO_FITS.md:227`（C4）、`:228`（C5 中的声明节点锚）、`:233`（C1a 锚）
- **证据（实开 `lib/infrastructure/scheduler/src/module_adapters.cpp`、根 `CMakeLists.txt`）**：
  | 文档锚 | 源码实况 | 判定 |
  |---|---|---|
  | `p3n_guard_input_units（:11440-11535）` | 守卫内核定义 **:13867** | ✗ |
  | `p3_op_properties（:11561）` | `p3_op_properties` 定义 **:13981**，调用 :13988（**先于任何像素读取** ✓） | ✗ 锚、✓ 行为 |
  | `重采样节点（:11729）` | 第二处调用 **:14156**；:11561/:11729 现处 `p2_hips_prop_double` 一带 | ✗ |
  | `p3n_guard_fail（:11541）` | 实际定义 **:13968**（:13989 失败即调 ✓） | ✗ |
  | `declare_hips_surface_brightness_units（:10113-10160）` | 实际 **:8470**（声明）与 **:12199**（实现），调用 :8797/:12873 | ✗ |
  | 根 `CMakeLists.txt:1011` 的 `add_subdirectory` | `add_subdirectory(lib/algorithms/resample)` 在 **:1110**；:1011 是 target_link_libraries 列表行 | ✗ |
  | `lib/algorithms/resample/CMakeLists.txt:22`（进生产链接闭包） | :22 = `${CMAKE_CURRENT_SOURCE_DIR}/p3_rsmp_units.cpp`（在 `add_library(astrocs_p3_rsmp …)` 源列表内） | ✓ |
  | `grep -c p3_v6_export CMakeLists.txt = 0` | 复跑 = **0**；`p3_v6_export.{h,cpp}` 自注 STATUS=未接入生产 | ✓ |
  | `p3_session.cpp:166-172,396` 只透传 BUNIT | :166-172 `p3_sampler_open_ex(...,&bunit,...)` ✓；:396 `bunit.c_str()` 传写出器 ✓（无语义判定 ✓） | ✓ |
- **反方核验**：`grep "11440-11535|10113-10160" 独立审计/` → 0；前轮 PASS 表无 PHASE3 锚项。
- **建议改法**：C4/C5 三处 module_adapters 锚与根 CMake 行号改到现址；行为结论（调度节点面已接线、
  会话面未接线、`p3_v6_export` 未进构建）**已复核为真，结论不动**。
- **所属面**：④幻觉与锚

### S3B-黄4｜PHASE3_HIPS_TO_FITS.md:233：C1a「mosaic 端口仍为 UnitId::ADU」已陈旧（代码已改为 SURFACE_BRIGHTNESS）
- **文件:行**：`docs/science/PHASE3_HIPS_TO_FITS.md:233`（并行件 `docs/science/algorithms/PHASE3_PROJ_IMPL.md:637` 同句同错）
- **问题**：文档把 C1a 登记为「代码侧缺口（只登记，本文件不改）」，称 `p2_write_descriptor` 的 `mosaic`
  端口**仍为** `UnitId::ADU`（锚 :1040-1049）。现代码两处端口均已是 `UnitId::SURFACE_BRIGHTNESS`，
  即该缺口**已修**，登记与锚双双过期 ⇒ 文档在说「有、代码没接」，而实际「已接」。
- **证据（含反方核验）**：
  - 实开 `module_adapters.cpp`：`p2_write_descriptor` 现定义 **:1162**（文档锚 :1040-1057 现为
    `p2_coverage_descriptor` 区）；端口表 :1175 `{"integrated",…UnitId::SURFACE_BRIGHTNESS…}`、
    :1176 `{"mosaic",…UnitId::SURFACE_BRIGHTNESS…}`，:1170-1174 注释明记「GAP_AUDIT G3-4 / docs/ASTROCS_DESIGN §5.6：
    **写出端口单位 = SURFACE_BRIGHTNESS**（冻结单位表 signal_sb = ADU/sr）」。
  - `grep -n "p2_write_descriptor" module_adapters.cpp` → 仅 :1162（定义）与 :15494（注册表引用），
    **无第二份旧定义**（反方排除：不是还有个 :1040 的 ADU 版本）。
  - 反向核验：`PHASE3_PROJ_IMPL.md:635` 自己已把 registry 页那条标成「✅ 现行 UnitId::SURFACE_BRIGHTNESS」，
    即同一批文档内部对 phase2 写出端口的判定**两说并存**（:635 已修 vs :637 未修）。
- **建议改法**：按变更流程撤下或改写 C1a 条目（连同锚 :1162/:1175-1176 与 PHASE3_PROJ_IMPL:637 同批处理）；
  这是登记面更新，不动任何公式/容差。
- **所属面**：③跨文档冲突（正本 ↔ 实现现状、两份登记件互斥）＋④幻觉与锚

### S3B-黄5｜PSF.md:113：错误码 `DPSF_ERR_PARAM` 在仓内不存在，且与同文件 :51 相互矛盾
- **文件:行**：`docs/science/PSF.md:113`（§8 极端条件表首行）
- **问题**：两重错——① 证据行 `dpsf_fit:446 empty rect` 失准（现 :500）；② 行为列写错误码
  `DPSF_ERR_PARAM`，该符号**全 lib 不存在**；同文件 :51 已正确写 `DPSF_FIT_INVALID_PARAMS（= 2）`。
- **证据（含反方核验）**：
  - `grep -rn "DPSF_ERR" lib/` → **0 命中**（不存在的枚举）。
  - 实开 `lib/algorithms/psf/src/dpsf_psf.cpp`：:500 `dpsf_log(... "dpsf_fit: empty rect …")`、
    :502 `result->status = DPSF_FIT_INVALID_PARAMS`、:503 `return DPSF_FIT_INVALID_PARAMS`。
  - 同文件 :51：「…否则返回 `DPSF_FIT_INVALID_PARAMS`（= 2，`dpsf_psf.cpp:499-504`）」——**该锚实开完全相符**，
    故 :113 与 :51 是**同条件两说**（一真一幻觉）。
  - `grep "DPSF_ERR_PARAM" 独立审计/ 检查-*` → 0，非前轮已报项。
- **建议改法**：§8 首行改用 `DPSF_FIT_INVALID_PARAMS`（与 :51 统一），证据行改 `dpsf_psf.cpp:500`（或并入 :499-504）。
- **所属面**：②行文逻辑（文内两说）＋④幻觉与锚（幻觉符号）

### S3B-黄6｜PSF.md：符号表/§7/§8 行锚漂移（6 处，行为断言实开为真）
- **文件:行**：`docs/science/PSF.md:14（符号表 I(r)）/ :15（Q「同上」）/ :17（flux）/ :106 / :114 / :115 / :116 / :117`
- **证据（实开 `lib/algorithms/psf/src/dpsf_psf.cpp`）**：
  | 文档锚 | 文档主张 | 源码实况 | 判定 |
  |---|---|---|---|
  | :14 `dpsf_psf.cpp:13-18` | 符号 `I(r)` 定义处 | :13-18 = `#include` 行；Moffat 轮廓实现在 :84-118 | ✗ |
  | :15 「同上」 | 符号 `Q` 定义处 | 同上，落空 | ✗ |
  | :17 `dpsf_psf.cpp:368` | `flux = 2πA·sx·sy/3` | :368 = `sx0 = 0.15*rw`；flux 实在 :428-429（:427 注释逐字同式） | ✗（式 ✓） |
  | :106 `dpsf_psf.cpp:352-363` | θ 四候选消歧 | 实在 :411-423；:352-363 = bkg0/max_val/A0 | ✗ |
  | :114 `dpsf_psf.cpp:310` | `A<=0` → WARN 拒 | 实在 :363-365（"Amplitude<=0"） | ✗（行为 ✓） |
  | :115 `dpsf_psf.cpp:333` | 非有限/非正尺度 → WARN 拒 | 实在 :387-390（"Invalid fit params"） | ✗（行为 ✓） |
  | :116 `dpsf_psf.cpp:343` | FWHM 超窗 → WARN | 实在 :397（"FWHM exceeds rect"） | ✗（行为 ✓） |
  | :117 `dpsf_psf.cpp:186` | LM 不收敛返回非零 status | 实在 :205（"LM hit iteration limit" + `return DPSF_FIT_ITERATION_LIMIT`）；:186 = `}` | ✗（行为 ✓） |
- **反方核验**：`grep 检查-修复验证.md` → `PSF` **0 命中**（PASS 表不含 PSF 任何项）；前轮三报告同项 0 命中。
- **建议改法**：按 ANCHOR_CONTRACT 逐条改到现址；§8 行为列本身与代码一致，**不改语义**。
- **所属面**：④幻觉与锚

### S3B-黄7｜PSF.md:131-132：「非高斯残差下偏差可达 ±18%」被本行自己的数据证伪
- **文件:行**：`docs/science/PSF.md:131-132`
- **问题**：同句给的实测比值为 Gaussian 1.0004 / Uniform **1.1837** / Laplace **0.8024** / t(5) 0.8648 ⇒
  正向最大 +18.37%，**负向 Laplace 为 1 − 0.8024 = −19.76%**，绝对值已越过「±18%」的界；
  「可达 ±18%」作为**边界式**表述与其自身数据冲突（读者会把 18% 当上界去判门）。
- **证据（含反方核验）**：`run/SCI-FIX-STARPSF-01/results/e1_constants.txt §C` 为该组实测出处（文档自引）；
  独立复算：|0.8024−1| = 0.1976 > 0.18；|1.1837−1| = 0.1837 ≈ 0.18 ⇒ 只有正向贴合。
  `grep "±18|19.8" 独立审计/实验重做/总编对账/` → 0，非前轮已报项。
- **建议改法**：边界改写为「最大约 19.8%（Laplace）/ +18.4%（Uniform）」或去掉 ± 号只列四分布比值；
  属冻结文档数值表述，走变更流程。
- **所属面**：①科学性（结论边界与给定数据不一致）

---

## 三、绿级（optional）

### S3B-绿1｜PSF.md:95：`f_out > 3.1%` 与 `r_win/α < 1` 的配对不紧
`r_win/α = 1`（即文中 `r_win = 1.41σ`）处 `f_out = (1+1²)⁻³ = 12.5%`；`f_out = 3.1%` 对应
`r_win/α ≈ 1.478`。语句作为蕴含式为真（该域内 f_out 确 > 3.1%），但边界与数值不成对，
读者易把 3.1% 当成边界值。**面②**（表述精度）。建议：改为「该域内 `f_out ≥ 12.5%`」或注明 3.1% 的来源口径。

### S3B-绿2｜NOISE_MODEL.md:301：「`1.44 = 1.152×1.2533`」是四舍五入等式
`1.152 × 1.2533 = 1.443802`（相对 1.44 偏 +0.26%）；同文件 :99 用「中位数效率 **1.25**」得恰好 1.44，
两处对同一复合口径给了 1.25 / 1.2533 两种乘子。台账 D-04 已裁 `9216 = (1.44/0.015)²` 精确成立、
理论链 1.444 作保守上界 ⇒ **结论与常数无误**，只是等号语义不严。**面①**。
建议：把「=」改为「≈（取 1.44 记账）」并统一 :99/:301 的乘子写法（**不改 9216/1.44 冻结值**）。

### S3B-绿3｜PSF_SIGNAL_WEIGHT.md:52-53：同一句标定集描述逐字重复
「标定集为 1000 幅 4096² 合成图（背景高斯 σ=0.001、均值 0.015、平均 1500 颗可检测星、Moffat β=4 FWHM=5 px）
调至中位 PSFSW=1、中位 PSFSNR=中位标准 SNR=3.029」在 :52 与 :53 各出现一次（:53 多「Poisson+高斯噪声」一词
与 PCL 常数句相连）。**面②**（行文冗余，无数值冲突）。建议删其一。

### S3B-绿4｜PHOTOMETRY.md:32/:213：`MAD(r_inliers)` 未写中心，实现取 IRLS location
文档写 `sigma_residual = MAD(r_inliers)/0.6744897501960817`，实现
`lib/algorithms/photometry/cpp/src/star_matcher.cpp:620` 是 `median(|r − location|)`（location 为 IRLS 位置估计），
与 `NOISE_MODEL.md:62` 冻结写法 `median(|x − median(x)|)` 的中心不同（IRLS location ≠ median(r_inliers) 一般不等）。
代码侧注释也写「MAD(r_inliers)」故不算实现错，属**文档记号未消歧**。**面①**（定义域/记号）。
建议：在 :213 行内注 `# MAD 中心 = IRLS location`；`grep` 实验/photometric-magnitude 成稿（RESOLUTION:84 等）
沿用同一记号 ⇒ 同批改，避免又造一处两说。

---

## 四、与既有登记的关系（去重声明）

| 项 | 状态 |
|---|---|
| `检查-修复验证.md` 红 8/黄 16/行漂移 a–f（含 NOISE:301→:830-930、PHASE2_UPM:43-44/:23/:25、PHOTOMETRY §3⑥ :355→:356 等） | **已 PASS，本轮不报**；本报告已逐项核对未复现 |
| NOISE_MODEL.md:302 `collect_patch_sky :530-585` 失准 | `检查-修复验证 §二` 已登记遗留 ⇒ 不计新发现（随黄1 同批修） |
| NOISE `fill_impl :776-866 → :1412` | `AUD-101-DA02-算法推导.md:617` 已登记（另批次），本轮复核**仍未修**，并入黄1 |
| PHASE2_UPM §16.3 `module_adapters:7808-7813` 漂移 | `复核-AUD203.md:165` 已登记（另批次），本轮复核仍未修，并入黄2 |
| PSW:127「6 等（10 倍通量）」 | `AUD-101-DB-16.md:252` 已登记 ⚠⚠（另批次），本轮复核仍未修 ⇒ 仍按红2 报（红级为本切片定级） |
| 文档自declare「未独立核验」项（PixInsight PCL 头文件 404、LSST isrTask L431-442、Moffat/Górski「文章级未逐式核验」） | 文档已诚实标注核验态 ⇒ **不判幻觉**，不报 |

---

## 计数

- **红 2**（S3B-红1、红2）
- **黄 7**（黄1 NOISE 锚群、黄2 PHASE2 锚群、黄3 PHASE3 C4/C5 锚群、黄4 C1a 陈旧登记、黄5 PSF 错误码、黄6 PSF 锚群、黄7 ±18%）
- **绿 4**（绿1 f_out 配对、绿2 1.44 复合写法、绿3 重复句、绿4 MAD 中心记号）

---

## 五、已查无问题面

### ① 科学性（本切片查净的部分）
- **D-04**：`NOISE_MODEL.md:99`「c ≈ 1.152/√N … 1.44/√N_sky … N_sky ≥ 9216」——独立复算 `1.44/√9216 = 1.500%` ✓、
  `1.44/ln10 = 0.6254`（文中 0.625 ✓）、`(1.44/0.015)² = 9216` ✓；与 `实验/absolute-snr/REPORT_paper.md:33`
  「1.5% 控制点精度约定（N_sky=9216 预算口径）」**同口径**；1.144 残留只存在于 :301 的 D-04 订正注（旧对照，合法）。
- **1.4826 冻结条款 vs 源**：`NOISE_MODEL.md:312`「唯一权威写法 `1.482602218505602`、11 位简写仅「约等于」语境、
  绝对差 5.602e-12 / 相对差 3.779e-12」——与 `noise_model.cpp:119` 常数字面量**逐位一致**；
  三项误差独立复算 = 5.602e-12 / 3.779e-12 / `|0.6745−0.6744897501960817|/0.6744897501960817 = 1.5196e-05` 全 ✓；
  `q_psf` 常数 `0.7316727929211932` 在 `noise_model.cpp:95` 注释中与本文件同一语义 ✓。
- **D-07**：`PHASE2_UPM.md:88-103` 三口径（纯公式高估 ≈9.5%（精确 9.53%）/ 端到端 ≤±1.5% / 生产链裁剪臂低估 1.3–3.2%）
  与 `分歧台账 D-07` 及成稿 `absolute-snr/REPORT_paper.md:83`（9.53%、边界 N≈45–49）**三方一致**；
  自由度：κ(5)=1.4342/κ(17)=1.5308/κ(65)=1.5604、E[σ̂²]/σ²=0.906/0.986/≥0.997、偶 N +5.0/+2.8/+1.3%、
  median(σ̂)=0.746@n=5 —— **本切片 2×10⁶ 样本独立 MC 复算全部相容**（相对差 ≤0.2%）；`1.1666 = √1.361`
  （`photometric-magnitude/REPORT_experiment.md:48` 三腿）与之不混用。
- **D-08**：`PHASE2_UPM.md:113-127` 两因子 `k_gauss(N)×k_geo` + 全表（5→1.637/9→1.316/17→1.144/25→1.083/49→1.046/≥121≈1.00）
  与正本 `实验/healpix-polar/docs/DERIVATIONS-P3.md §D8`、`UNCERTAINTY_AND_COVARIANCE.md:53` **三处逐字同表**；
  N=9 的旧 1.26 只在订正注；**冻结 1.4 全文只以「代码默认/域外回退（实现记录）」出现**（:60/:67/:121/:288/:308），
  无任何「1.4 为冻结科学口径」的正文表述；消费规则「标定元组 + N 档声明、否则 fail-closed」在 :61/:119-120/:289 齐备；
  `sampler.cpp:83`（kControlCorrDefault=1.4）/:874-875（cvar 式）/:560-583（域外回退与打标）**实开相符**。
- **D-06**：`PHOTOMETRY.md:295/:429` 双锚——本轮直接取回 **PMC6768164 原文**，命中逐字句
  「Asymptotically, c = 4.685 yields 95% asymptotic efficiency at the Gaussian」✓；
  `https://doi.org/10.6028/jres.088.006` 解析为 Kafadar, *J. Res. Natl. Bur. Standards* **88(2), 1983**
  「The Efficiency of the Biweight as a Robust Estimator of Location」✓；Kafadar 1983 二级锚与
  `实验/photometric-magnitude/refs.md V1` 一致 ✓；arXiv:2208.00211 = Gaia DR3 summary ✓、
  Aitken DOI 10.1017/S0370164600014346 = 「On Least Squares and Linear Combination of Observations」✓、
  Andrae arXiv:1012.3754 = 「Dos and don'ts of reduced chi-squared」✓、Gaia XP 合成测光 arXiv:2206.06215 ✓、
  EDR3 光度 arXiv:2012.01916 ✓。
- **PHASE3 定量**：§9a-12 的 `sec²γ` 三档独立复算 1.030462/+3.046%、1.068539/+6.854%、1.121847/+12.185% 全 ✓
  （该组为 PASS 黄级订正后的口径，本轮只复核未重报）；§5 `order_needed` 例（hips_order=3, W=512, s_out=0.001°/px
  ⇒ order_needed=7、θ_pix=0.01431°=51.5″=14.31×）独立复算 ✓；`h_max = 2.13794/nside` 闭式
  `√(16/5+5π²/36) = 2.137937`、上界比 2.0892036 = 2.137937/√(π/3) ✓；双线性权重和 15u 记账与 :69 推导逐步复核 ✓。
- **PSF 定量**：`1.230310` 因子（:25/:104）、trimmed-mean 闭式 `0.7316730952806134` 与实现常数相对差 **4.13e-7**、
  有限-m 表（121/169/441/1024/9 的 −0.98/−0.47/−0.28/−0.11/−8.70%）逐格复算全 ✓、`flux=2πA·sxsy/3` 与
  `dpsf_psf.cpp:427-429` 逐字同式 ✓；窗口截断 `f_out=2.02e-3 ↔ 偏高 0.20%` 复算 `0.00202/(1−0.00202)=0.202%` ✓。
- **PHOTOMETRY/PSW 数值**：`mag_tolerance=3.0` 与 `pc_api.cpp:145/:435`、`star_matcher.cpp:241-248` ✓；
  `0.6744897501960817`/`4.685`/`1e-6` 与 `star_matcher.cpp:21-27`、:538-586 IRLS ✓；
  PSW §7a 全部数值对回 `实验/absolute-snr`：`max|z|=2.68`（REPORT_experiment:21）、`0.9980` 与 `32.5%`（REPORT_paper:91）、
  `E=7.17`（b6_gates_audit.json:22 eff_loss=7.16695）、`30.1%`（b4 eff_loss=0.30184 @zp_spread=1.0）、
  `≤0.6%/3.6%`（b4 anchored_over_oracle 1.0059/1.0362）、`Var 9.105 vs 9.071`（COMPARISON_TABLES:167）、
  `seed 20260921`（README:18）**逐项对上**；W_info = a²PᵀC⁻¹P = 1/Var(F_hat) 与 plugins 04_psf/07_noise_snr 同式三处一致。

### ② 行文逻辑（查净的部分）
- 六份文件均**通读到尾**（被工具裁剪的中段已逐段补读），`UNRESOLVED` 未被当结论用：
  PHASE3 §15 明写 `UNRESOLVED-SCIENCE=0`，§16 是「如实登记现状」并声明不改公式/容差；
  PHASE2_UPM §16.3 用「待统一/待裁决」**显式暴露**两入口不一致（:429 正向约束）；
  PSW §3 顶部「未独立核验」自declare、PHOTOMETRY §14「未逐页核验」自declare——诚实标注核验态，不判幻觉。
- 全部 `<!-- 订正 … -->` 注（PHOTOMETRY :308/:305、NOISE :100/:301/:366、PHASE2 :100/:116/:125/:223/:244/:472、
  PHASE3 :152、PHOTOMETRY :430 D-06 表等）**均闭合、均带旧值对照**，无「改了但留旧说法」的半截订正；
  D-07/D-08/A-P5-11/A-P5-01/A-P1-04 的旧值只活在订正注内。
- 除 S3B-红1、黄5、绿3 外**未发现新的文内两说**；`grep 检查-修复验证.md §三` 六处上下文抽查项（DRIZZLE/UNCERTAINTY/
  PHASE2_SAMPLER/UPM_SOLVER/DERIVATIONS/additive-sky README）本轮未扰动。

### ③ 跨文档冲突（查净的部分）
- **常数表**：`k_gauss` 全表在 PHASE2_UPM:115 / UNCERTAINTY:53 / DERIVATIONS-P3 §D8 三处逐字同源；
  `1.4826…` 在 NOISE:62/:312、PHOTOMETRY:296、PSF:188 四处同值且「MAD 与 trimmed-mean 常数不可互换」互相回指；
  `9216/1.44/1.5%` 与 absolute-snr 成稿同口径；`0.7316727929211932` 在 PSF:122/:148/:179/:188 与
  `noise_model.cpp:95`、NOISE:312 一致。
- **定义面**：PSW 的 `w = SNR²/F_ref² ≡ 1/σ_F²`、`C_out = R C_in Rᵀ`、`W_info` 与
  `CONTROL_WEIGHT_SNR`/plugins/INTEGRATION 引用方向一致（PSW:78/:93/:97 明确「估计量≠口径」「诊断不入 ivar」）；
  NOISE 的背景方差面/加权方差面两面分离在 §5/§5c/§9a 与 `07_noise_snr.md`、`DATA_SEMANTICS §31.1` 同向；
  PHASE3 的 `ADU/sr` 缺省与 `p3_resample.cpp:293`、`p3_output.cpp:284-285/:351`、DATA_SEMANTICS §31.1a 四处一致。
- **D-ruling 面**：D-01（外接圆 1.0415）不落本切片六文件（grep 0 命中，无冲突可报）；
  D-06/D-07/D-08/A-P5-05 的落地状态见 ① 面与红1。
- 本切片**唯一**的跨文档冲突为 S3B-黄4（PHASE3 ↔ PHASE3_PROJ_IMPL ↔ 实现现状 的 C1a 两说）。

### ④ 幻觉与锚（查净的部分）
- **实开核为真的锚**（抽样列举）：PHASE3 `p3_wcs.cpp:212-224/:266-290/:253-263/:114-115/:463-469/:456-461/:102-138`、
  `p3_output.cpp:267/:284-285/:290/:710/:724/:55-90/:348-372`、`p3_resample.cpp:48/:206-218/:293`、
  `hips_properties.cpp:120-121`（tile_width must be 512）、`p3_session.cpp:166-172/:396/:413`、
  `resample/CMakeLists.txt:22`、根 `CMakeLists.txt` add_subdirectory 链、`grep -c p3_v6_export = 0`；
  PHOTOMETRY 全部 star_matcher/pc_api/spectrum_integrator 锚（:21-27、:415-443、:486-500、:518-536、:538-586、:589-627、
  pc_api:145/:435、star_matcher:241-248）；PSF :25/:84-118/:248-249/:393-394/:284-311/:499-504；
  PHASE2 `sampler.cpp:83/:874-875/:93-104/:560-583/:864-877`、`p2_session.cpp:204`、`sky_plane.cpp:1082-1135`、
  `sky_plane.h:217-222`、`module_adapters.cpp:9557-9573/:9690-9695/:9734/:9800`（行为真值）；
  NOISE `snr_estimator.h:248` 的 `SNR_FLOOR_UNBOUND(-10)`、`noise_model.cpp:95/:119/:1262`。
- **文献抽验**：PMC6768164（逐字句命中）、Kafadar DOI、arXiv 1012.3754 / 2206.06215 / 2208.00211 / 2012.01916、
  Aitken DOI 全部题录相符；**未发现幻觉文献**（文中自declare「未核验」项如实标注，不计入幻觉）。
- **「文档说有、代码没接」面**：逐条反查——PHASE3 §16 C4 的「会话面未接线 / p3_v6_export 未进构建」**实测为真**；
  PHASE2 §16.3 的「两入口不一致（待统一）」**实测为真**（p2_session:204 未设 tolerance_relative ⇒ 绝对判据）；
  PSW §3 的「psfsw_robust 已退役 ⇒ fail-closed」与 `FZ-MODE-RETIRED` 引用方向一致（本切片未见反例）。
  唯一反向情形（文档说有缺口、代码已修）= S3B-黄4。
- **数值面**：§17.2/§17.3/§17.4 全部数字（8.42σ→3.7e-17、5.35→8.7e-8、63%/72.6%、1.378×、3.5σ、
  gate/(1−gate/2)=1.0050%、A 行族系 6.9e-4、检出表 0.997/0.844/1.471/0.717/1.023、1.59 倍解析高估）
  与 `run/SEAM-DERIV-01` 证据文件逐位对回并复算，**判据链无错数**（唯一问题见红1 的适用域条文）。
