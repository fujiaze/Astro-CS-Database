# ROOT-004 扩展轴 SCI2 — Phase2/mosaic + Phase3/export 科学一致性抽样审计

- 轴名：SCI2（Phase2/mosaic + Phase3/export 科学一致性抽样审计；重点：support 作权重族残留、ivar 降级、投影 registry 完备性、CRVAL2/nside 读写互斥性、IVOA 字段）
- 基线 SHA：开工 900916fb0dfe93e21bd36c09908a6cc679de5256（当时 HEAD=main=origin/main 三者相等）；收工 01754fab8618313bc39a4da3014e78cd0ad4e2a3（收工时三者再次相等）。
  审计期间 HEAD 多次前进，并出现 origin/main=b4afc135848d8b401ee36278effcb06699542fec 与 HEAD=main=180c8a0a 不等的抖动窗口；按父级更新政策未停工，仅在此登记。
  受影响条目（证据抖动面）：并行线在审计中把 lib/phase3_proj→lib/algorithms/projection、lib/astro_image_io→lib/infrastructure/aio、lib/phase2_int→lib/algorithms/integration、lib/hips→lib/algorithms/drizzle/hips、lib/healpix_db→lib/infrastructure/hips_browser 整体迁移。本轴全部 path:line 已在收工 SHA 01754fab 重新定位并复跑；旧路径在收工树中已不存在（SCI2-01/02/05/06/07/08 的定位行号均为收工树实测行号）。
- 方法：只读抽样。判据按权威链 ASTROCS_DESIGN §0（DESIGN > AGENTS.md > ENGINEERING_SPEC > 插件文档 docs/plugins/**；docs/science=公式权威、docs/algorithms=推导权威；ASTROCS_PROJECT_CONSTITUTION.md/宪章、docs/standards/**、docs/contracts/** 为非权威，仅历史参照，不作判据）。每条发现给「公式锚(文档+§节号) + 实现锚(path:line) + 测试面」，证据为本轮真跑命令与逐字输出（≤3 行）；科学偏差用本机 astropy 7.0.1 与仓库既有编译产物 build/v6_p3_proj/v6_p3_proj_probe 交叉对拍。零修复、零 git 写。
- 摘要（≤10 行）：
  1. 投影 registry 收工树仍只 4 投影（TAN/SIN/CAR/AIT）；DESIGN §5.3 与 14_projection.md §4/§8 冻结八投影并要求八投影全覆盖测试，缺口被在册测试当不变量固化。
  2. 实测复现（真二进制 + astropy 同头卡对拍）：CAR/AIT 的 CRVAL2 只进头卡不进映射，CRPIX 参考像元解出 dec=0 而头部声明 CRVAL2=30 → 产品像素网格与自身 WCS 相差 |CRVAL2| 度（实测 30.0°；TAN 对拍差 0）。
  3. 该缺陷无任何防线：唯一 astropy 对拍夹具把 dec0 钉为 0，Python「独立 Oracle」逐式复制同一 θ₀=+90° 约定（同构即自证）。
  4. 公式权威 CONTROL_WEIGHT_SNR.md §4 把 weights=support×snr² 标为 weight_mode=2，实现里 mode2=ivar、mode0=support×snr²，v6 节点链与权重来源门 R3 双双判该形态非法 → 文档/工具/节点链三面互斥。
  5. 同一开关 legacy_allow_weight_fallback=true 在工具面降级为 weights=support（无量纲顶替 ADU⁻²）且逐像素不计数，在节点面降级为等权 1.0 并标 uncertainty_available=false → 语义分叉。
  6. Phase2 马赛克 HiPS 产品的 properties 由共用写侧无条件写 obs_description=Phase1 单帧文案与 prov_progenitor=ivo://astrocs/phase1/drizzle → 产品自称错阶段，测试面零断言。
  7. HEALPix nside 幂次门读写不互斥（写侧只查 nside>=512 且 ilog2 截断，读侧强制 NSIDE==2^(K+9)）；正确门只存在于另一条实现面。
  8. hips_pixel_scale 无机器门：三处 in-tree 取值无一能由 ALG 冻结公式在任何合法 nside 上复算得到。
  9. 生产 export 目前仅 TAN（p3_session/p3_wcs 大小写敏感硬拒 + 合同 const TAN），且 lib/algorithms/projection/module.yaml:80 entrypoint: MISSING → 第 2/7 条为「挂载即爆」面，当前不污染已交付产品，但已污染 registry 契约面与 owner 状态宣称。
- 发现计数：P0 = 1 ｜ P1 = 4 ｜ P2 = 3（合计 8 条）

## 发现表

| ID | 一句话 | 严重度 |
|---|---|---|
| SCI2-01 | CAR/AIT 的 CRVAL2 进头卡不进映射，像素网格与自身 WCS 相差 CRVAL2 度（实测 30.0°） | P0 |
| SCI2-02 | 投影 registry 仍只 4 投影，缺 STG/MOL/CEA/ZEA，且被测试断言固化为正确 | P1 |
| SCI2-03 | 公式权威把 support×snr² 标为 weight_mode=2，与实现/v6 节点链/权重来源门三向互斥 | P1 |
| SCI2-04 | 同一 ivar 降级开关两实现面语义分叉；工具面缺 ivar 产品分支逐像素不计数 | P1 |
| SCI2-05 | Phase2 马赛克产品 IVOA properties 无条件写 Phase1 单帧身份（provenance 错标） | P1 |
| SCI2-06 | nside 2 的幂门写侧缺失、读侧强制 → 可产出自家 reader 必拒的产品 | P2 |
| SCI2-07 | projection 字面量口径互斥：插件文档小写 tan，实现只认大写 TAN | P2 |
| SCI2-08 | hips_pixel_scale 无机器门：三处 in-tree 取值无一满足 ALG 冻结公式 | P2 |

---

### SCI2-01

- 定位：lib/algorithms/projection/p3_proj_v6.cpp::car_pix2world（:169 theta_deg = yd、:174 *dec_deg = theta_deg）、::ait_pix2world（:206 同族）、::fits_keywords（:410 写 CRVAL2 = %.10f）、::make（:356 out->crval_dec_deg = centre_dec_deg；:327 声明域守卫只限制 |dec|<=85，允许非零中心）；legacy 同族 lib/algorithms/projection/p3_projection.cpp:198-206（theta = -yd*kRad，注释「θ₀=+90°: δ=θ」）。
- 违反的最新权威条款：docs/plugins/algorithms_phase3/14_projection.md §4（每种投影声明 CRPIX/CRVAL/CD/PC/CDELT/CTYPE；FITS 1-based 与内部 0-based 转换唯一；正反变换必须互逆）+ §7（轴手性/CRPIX 单位错误 → fail-closed）；ASTROCS_DESIGN.md §5.3（内置多种投影，每种声明 CRPIX/CRVAL/CD/CTYPE）。要点：头卡声明的参考点必须就是映射所用的参考点，不允许两套口径。
- 当前证据（本轮真跑）：
  - 命令：timeout 120 ./build/v6_p3_proj/v6_p3_proj_probe CAR 350.0 30.0 0.02 97 89 3 | grep -F "ROW x=48 y=44 "
  - 输出：V6PROBE proj=CAR ... crval2=30 ... crpix1=49 crpix2=45 ｜ ROW x=48 y=44 status=0 ra=350 dec=0 omega_excess=...
  - astropy 7.0.1 用同一 CRPIX/CRVAL2=30/CD=diag(0.02,-0.02) 解同一像元：astropy same pix -> ra/dec 350.0 30.0  |ddec|= 30.0（AIT 亦 30.0；TAN 亦 0.0）。
- 测试面（缺陷无防线的原因）：tests/unit/v6_p3_proj/p3_proj_legacy_deviation.py:24 CASES = [("CAR", 0.0, 0.0, 1.0, 97, 97), ("AIT", 0.0, 0.0, 1.0, 64, 64)]（唯一 astropy 对拍面，dec0 恒 0 → 该缺陷在结构上不可见）；tests/backend/test_p3_projection_oracle.py:153-157 CAR 参照式 dec = -y_deg（不引用 dec0，与实现同构，非独立锚）。
- 建议严重度：P0
- 影响：任何 |CRVAL2|>0 的 CAR/AIT 导出产品整幅赤纬错位（守卫上限 85°），且头卡表面自证合法 → 下游 WCS 校验发现不了。
- 整改建议（最小改动面）：二选一。(a) car_*/ait_* 在解出 (θ,φ) 后套一次 θ₀=crval_dec_deg 的球面旋转（与本文件 :71-90 zenithal 核同构），使映射与头卡一致；(b) 收紧契约：CAR/AIT 在 make/plan 里强制 centre_dec_deg==0（非零即 kParam 显式拒），并把 CRVAL2 改写为 0。两种都需同时把 p3_proj_legacy_deviation.py 的 CASES 扩到 dec0 ∈ {-30, 0, 30}，并让 CAR/AIT 参照改为读回 astropy 头卡（真独立）。
- 建议文件域：lib/algorithms/projection/、tests/unit/v6_p3_proj/、tests/backend/test_p3_projection_oracle.py
- 验收门：timeout 120 ./build/v6_p3_proj/v6_p3_proj_probe CAR 350.0 30.0 0.02 97 89 3 | grep -F "ROW x=48 y=44 " | grep -c "dec=30" 期望 1（本轮实测 0；若二进制不在先跑 cmake --build build --target v6_p3_proj_probe）。
- GAP/任务关系：GAP-011 只登记「两个 registry 各只有 4 投影」的数量缺口，未覆盖本条 CRVAL2 不自洽 → 新事实；归 TASK_LIST P3-001（治理八投影 registry 与独立 Oracle），装配面连带 P3-002。
- 旧清单同源：M1a-A-003（CAR/AIT 投影丢失 Paper II fiducial 偏移（CRVAL2 不进映射）且 Y=−θ 手性与 CD 合成北南镜像，问题扫描/findings/A_SCI_DEF/p0/M1a_L01_L02.md）— 与旧清单同源不删条；本条补充收工树实测复现 + 「夹具 dec0 恒 0 与 Oracle 同构」的无防线证据。

### SCI2-02

- 定位：lib/algorithms/projection/p3_projection.cpp:266 const P3ProjectionSpec kRegistry[4] = { 与 :280 if (count) *count = 4;；lib/algorithms/projection/p3_proj_v6.cpp:225/:300 同；lib/algorithms/projection/p3_projection.h:29-32（ProjectionId 仅 4 值）。测试固化面：tests/unit/p3_projection_test.cpp:572（CHECK_MSG(cnt == 4, "registry 恰 4 行(TAN/SIN/CAR/AIT, §18.1)")）与 :377-378（find("ZEA") == nullptr，注释引「宪章 §18.1 只注册四投影」）；tests/unit/v6_p3_proj/v6_p3_proj_test.cpp:133（n == 4）与 :151-152（registry_find("ZEA") == nullptr）。
- 违反的最新权威条款：ASTROCS_DESIGN.md §5.3（第 264 行）「首批冻结 TAN / SIN / CAR / AIT / STG / MOL / CEA / ZEA」；docs/plugins/algorithms_phase3/14_projection.md §1:5、§4:22（同集合）、§5:31（配置取值 tan/sin/car/ait/stg/mol/cea/zea）、§8:51「八投影全覆盖测试」；docs/plugins/00_INDEX.md:51 同集合。要点：冻结集与验收都是 8 个投影。
- 当前证据（本轮真跑）：
  - 命令：timeout 60 grep -cE '"(STG|MOL|CEA|ZEA)"' lib/algorithms/projection/p3_projection.cpp
  - 输出：0（exit 1，零命中）
  - 命令：timeout 60 grep -n 'kRegistry[4]' lib/algorithms/projection/p3_projection.cpp lib/algorithms/projection/p3_proj_v6.cpp → p3_projection.cpp:266、p3_proj_v6.cpp:225
- 建议严重度：P1
- 影响：用户按 §5 配置 projection=stg/mol/cea/zea 时，按 §7「未注册投影 → 拒绝」必然失败，交付能力比 DESIGN 冻结面少一半；且缺口被在册测试锁死（补投影即红），后续修复成本被抬高。
- 根因（权威链定位）：实现与测试的冻结依据引用的是非权威文档「ASTROCS-CONSTITUTION-001 §7.3/§18.1」（p3_projection.h:2、p3_projection_test.cpp:572），而最高权威 DESIGN §5.3 写 8 投影；同时推导权威 docs/algorithms/PHASE3_PROJ_IMPL.md 同篇内 §1:20 与 §11:190「不实现 SIN/ZEA/CAR/AIT」对 §15:348「版本化 projection registry 与冻结四投影（TAN/SIN/CAR/AIT）」并存 → 三处文本互相打架。按权威链应以 DESIGN 为准。
- 整改建议：二选一并先走文档变更流程（agent 不改 DESIGN）：(a) 按 §5.3 补 STG/MOL/CEA/ZEA 四行 registry + 每行独立正反往返 Oracle + 八投影全覆盖测试，并把 cnt==4 断言改为 cnt==8 与 ZEA 正例；(b) 若负责人裁定 alpha 期只交付四投影，须先改 DESIGN §5.3、14_projection.md §1/§4/§5/§8、00_INDEX.md:51、PHASE3_PROJ_IMPL.md §1/§11 的旧「非目标」句，再让测试断言引用新节号。属 AGENTS.md §8「两篇权威文档打架」→ 须升级负责人裁决，不得由实现单方面结案。
- 建议文件域：lib/algorithms/projection/、tests/unit/p3_projection_test.cpp、tests/unit/v6_p3_proj/、（文档面）docs/algorithms/PHASE3_PROJ_IMPL.md、ASTROCS_DESIGN.md
- 验收门：timeout 60 grep -cE '"(STG|MOL|CEA|ZEA)"' lib/algorithms/projection/p3_projection.cpp 期望 ≥4（本轮 0）；补投影后跑 timeout 600 python3 ci/run_checks.py --check CHK-ORACLE --quiet 期望 PASS。
- GAP/任务关系：与 GAP-011 同源不删条（GAP-011 另记 module.yaml/memory.md 自述「TAN/SIN/ZEA/CAR/AIT」与实际四投影不符）；归 P3-001；构建装配面连带 GAP-012 / P3-002；docs/owner/SCIENCE_OVERVIEW.md 现以「冻结四投影 IMPLEMENTED」对外宣称，须随裁决同步。
- 旧清单同源：M5b-C-07（ALG 文档把未冻结的 ZEA 列为扩展并漏 AIT）、M6b-C-003（同一 ACTIVE 文档内投影集新旧两版并存）、M7-G-101（宪章四投影 vs SCI 侧仅 TAN）— 均与旧清单同源不删条。

### SCI2-03

- 定位：docs/science/CONTROL_WEIGHT_SNR.md §4（:66-71，注释行「# 像素级 SNR 权重（stage2 排异/积分，weight_mode=2）」+ 体内 weights[s] = support[s] × snr_v²）；实现 lib/phase2/src/stage2_common.cpp:378（weight_mode = 2; // 默认 ivar）、:381-382（"support_x_snr2" → weight_mode = 0，注为 legacy 仅 ablation/诊断）、:384（错误串把 support_x_snr2 列为合法取值）；lib/phase2/tools/stage2.cpp:1112/:1379（mode2 → weights[s] = iv）、:1141/:1414（mode0 → weights[s] = support_v[s] * snr_v * snr_v）。
- 违反的最新权威条款：ASTROCS_DESIGN.md §4.3（逆方差叠加；SNR 只用于换算权重，不是直接加权）与 §2:82/84（signal/variance/ivar/support 各是各；帧级 SNR 是信噪比不是权重）。要点：公式权威自身 §4 的模式编号标签必须与实现编号一致，否则照文档实现即越权；CONTROL_WEIGHT_SNR.md §2 符号表 :32-40 又称 w_snr=snr_v² 为相对质量权重，与 §4 标签自相矛盾。
- 当前证据（本轮真跑）：
  - 命令：timeout 60 sed -n '63,71p' docs/science/CONTROL_WEIGHT_SNR.md → 首行 # 像素级 SNR 权重（stage2 排异/积分，weight_mode=2）；体内 weights[s] = support[s] × snr_v²
  - 命令：timeout 60 grep -n 'weights[s] = iv|weights[s] = support_v[s] * snr_v' lib/phase2/tools/stage2.cpp → 1112、1379（mode2=ivar）；1141、1414（mode0=support×snr²）
  - 命令：timeout 60 sed -n '4262,4270p' lib/core/src/module_adapters.cpp → "weight_mode 0 (legacy SNR) is not a science variance surface in the node chain; only 1 (equal) or 2 (ivar) are legal (DATA-UNC-001 §30.1)"
- 建议严重度：P1
- 影响：按 §4 标签实现「weight_mode=2」的人会把相对质量权重当逐样本逆方差写入 mosaic（量纲与语义双错）；同时 §4 的存在让工具面保留一条与 v6 生产合同互斥的权重族入口，三面不能同时为真。
- 整改建议：文档面（需负责人/文档变更流程，只读审计不代改）：把 §4 代码块标题的模式标签改为 weight_mode=0（legacy 相对质量权重，仅 ablation/诊断），并在同节并列 weight_mode=2 = 逐样本 ivar 的连续定义。代码面：从 lib/phase2/src/stage2_common.cpp:381-384 删除 support_x_snr2 取值与错误串中的该项，并把 stage2.cpp:1123-1141 / 1395-1414 的 mode0 分支置于 ablation 编译开关或直接删除。
- 建议文件域：docs/science/CONTROL_WEIGHT_SNR.md（权威文档，走变更流程）、lib/phase2/src/stage2_common.cpp、lib/phase2/tools/stage2.cpp
- 验收门：timeout 60 grep -c 'support_x_snr2' lib/phase2/src/stage2_common.cpp 期望 0（本轮 3）；配套 timeout 60 grep -n 'weight_mode=2' docs/science/CONTROL_WEIGHT_SNR.md | grep -c support 期望 0（本轮 1）。
- GAP/任务关系：与 GAP-010/U-03（未见「由输入帧 SNR 反算权重」的调用链证据）同族但事实不同（本条是文档标签与实现编号互斥）；归 P2-002（治理按需逆方差与三科学目标权重）。
- 旧清单同源：M3-A-002（SCI-CW 把 weight_mode=2 定义为 support×snr² 与实现及另两份冻结合同互斥，P0）、M7-A-002 / M7-A-125（ALG 层把 ivar 缺失回退 support 写成 mode2 唯一语义）、M7-A-210（weight_mode 枚举两表不闭合）— 与旧清单同源不删条。

### SCI2-04

- 定位：lib/phase2/tools/stage2.cpp:1119-1121（并行路 else 分支 weights[s] = support_v[s];，上一行注释「缺 ivar 产品（ivr==nullptr）：仅显式 fallback 可用」）与 :1390-1392（串行路同）；对照 :1117-1118 与 :1388-1389（tile 读失败分支有 ++ivar_tile_fallback_px 且先 fail-closed）；诊断面 :1749-1753（diag 仅 weight_mode / local_ivar_used / ivar_product_missing（帧级）/ legacy_allow_weight_fallback / ivar_tile_read_fallback_pixels）。生产节点面 lib/core/src/module_adapters.cpp:4483-4499：weight_mode==2 && !fallback 才取 ivar，fallback 时整块跳过使 w 保持 1.0，并由 :4290-4305 标 uncertainty_available=false。
- 违反的最新权威条款：docs/science/CONTROL_WEIGHT_SNR.md §6/§7（量纲区隔；不可接受变化含「把 local_snr / support 与逐像素 variance/ivar 当作同一权重语义」）+ docs/science/INTEGRATION.md（integration 权重来源为 SCI-NOISE ivar / SCI-UPM 权重链）+ ASTROCS_DESIGN.md §2:82、§4.3。要点：support 无量纲 [0,1] 不得顶替单位 ADU⁻² 的 ivar，降级须显式且可审计。
- 当前证据（本轮真跑）：
  - 命令：timeout 60 grep -n "weights\[s\] = support_v\[s\];" lib/phase2/tools/stage2.cpp | grep -vc ivar_tile_fallback_px → 2（两处 support 顶替赋值不带计数）
  - 命令：timeout 60 sed -n '4483,4499p' lib/core/src/module_adapters.cpp → if (weight_mode == 2 && !fallback) { ... }（fallback 时权重不改写，即等权）
  - 命令：timeout 60 grep -n 'diag["ivar' lib/phase2/tools/stage2.cpp → 1751 ivar_product_missing（帧级）、1753 ivar_tile_read_fallback_pixels（仅读失败）
- 建议严重度：P1
- 影响：同一个配置键在两条实现面给出两套数值（工具=无量纲 support；节点=等权 1.0），配置与结果跨面不可比；且工具面「整帧缺 ivar 产品」的降级面积在逐像素诊断里完全不可见。
- 整改建议：(1) 在 stage2.cpp:1120 与 :1392 各补一个逐像素计数（新增 ivar_product_missing_px，与 ivar_tile_fallback_px 分列）并写入 diag；(2) 把该分支权重从 support_v[s] 改为与节点面一致的等权（weights[s] = 1.0），或干脆 fail-closed 与 :549-577 的产品整体缺失门合并；(3) 若确需保留 support 顶替，须由文档变更流程在 CONTROL_WEIGHT_SNR §4/§6 显式冻结其量纲语义（当前 §7 判为不可接受变化）。
- 建议文件域：lib/phase2/tools/stage2.cpp（对齐面：lib/core/src/module_adapters.cpp）
- 验收门：timeout 300 ctest --test-dir build -R phase2_ivar_wiring --output-on-failure 期望 Passed（在册门，覆盖 fail-closed 与降级计数面）。
- GAP/任务关系：与 GAP-010 相邻（同属权重面治理）；归 P2-002。
- 旧清单同源：M4-C-03（stage2 ivar tile 读失败逐像素静默换 support 作权重，P0）— 与旧清单同源不删条；该条的 tile 读失败子面已在收工树修复（显式开关 + 计数 + fail-closed），本条登记其残留子面（整帧缺 ivar 产品不计数）与节点面语义分叉。

### SCI2-05

- 定位：lib/infrastructure/aio/src/hips/aio_hips_writer.cpp:968-969（kv 无条件 push obs_description="AstroCS Phase1 single-frame HiPS product"、prov_progenitor="ivo://astrocs/phase1/drizzle"）与 :1209-1210（SNR 目录面同）；默认值面 :496（ps->creator_did = creator_did ? creator_did : "ivo://astrocs/phase1"）。Phase2 调用方：lib/phase2/tools/stage2.cpp:592-595 与 lib/core/src/module_adapters.cpp:4663-4665 以 "ivo://astrocs/phase2" / "AstroCS Phase2 Mosaic" 调同一 aio_hips_product_begin → aio_hips_finalize 落盘。
- 违反的最新权威条款：ASTROCS_DESIGN.md §2:82（provenance 与 signal/variance/support/coverage 等一样「各是各」，禁止互相冒充）+ §1.2（三命令各自独立产品，阶段间只通过磁盘产品 + manifest + 哈希交换）。要点：产品自带的 IVOA 描述与来源必须指向产出它的阶段，不得继承上游阶段文案。
- 当前证据（本轮真跑）：
  - 命令：timeout 60 grep -cE 'AstroCS Phase1 single-frame|ivo://astrocs/phase1/drizzle' lib/infrastructure/aio/src/hips/aio_hips_writer.cpp → 4
  - 命令：timeout 60 grep -n 'ivo://astrocs/phase2' lib/phase2/tools/stage2.cpp lib/core/src/module_adapters.cpp → stage2.cpp:595、module_adapters.cpp:4665（同一 begin 传 phase2，但两键硬编码 phase1）
  - 命令：timeout 60 grep -rn 'obs_description|prov_progenitor' tests lib/infrastructure/aio/tests → 零命中（测试面零断言）
- 建议严重度：P1
- 影响：Phase2 马赛克产品对外自称「AstroCS Phase1 单帧产品」、prov_progenitor 指向 ivo://astrocs/phase1/drizzle → 阶段溯源与产品身份错标，任何按 properties 判读 progenitor 的下游取到错上游。
- 整改建议：把 obs_description 与 prov_progenitor 提为 aio_hips_product_begin（或 finalize）入参，或由 creator_did/阶段参数派生；删除 :968-969 与 :1209-1210 的字面量；:496 的 phase1 兜底默认改为「未提供即报错」；补一条 properties 断言（Phase2 产物的 obs_description 不得含 Phase1，prov_progenitor 须等于本阶段 did 派生值）。
- 建议文件域：lib/infrastructure/aio/src/hips/、lib/phase2/tools/stage2.cpp、lib/core/src/module_adapters.cpp
- 验收门：timeout 60 grep -cE 'AstroCS Phase1 single-frame|ivo://astrocs/phase1/drizzle' lib/infrastructure/aio/src/hips/aio_hips_writer.cpp 期望 0（本轮 4）。
- GAP/任务关系：GAP_AUDIT GAP-001..023 无对应条目（GAP-013 只登记 IO/HiPS 多套并存）→ 新事实；归 AIO-001（IO 读写边界唯一化）+ P2-001（治理 mosaic 固定 DAG 与产品合同）。
- 旧清单同源：idmap.csv 全文以 obs_description / prov_progenitor / creator_did 关键字检索零命中 → 无同源旧条目。

### SCI2-06

- 定位：写侧 lib/infrastructure/aio/src/hips/aio_hips_writer.cpp:481（nside < 512 是唯一 nside 判据，无 2 的幂检查）+ :493 与 :1896（ps->leaf_order = ilog2_u64(nside)，该函数为向下取整截断）；读侧 runtime/io/hips_core.c:245（h->nside = 1ULL << (order + 9u)）与 :632-637（tile NSIDE 卡与 h->nside 不符 → 报错）；正确门存在于另一面 lib/algorithms/drizzle/hips/src/module_entry.cpp:342（nside < 512 || nside > (1ull << 24) || (nside & (nside - 1)) != 0）。
- 违反的最新权威条款：docs/science/DRIZZLE.md §4:31（输入有效域：NESTED 唯一 ordering，nside=2^order 为 2 的幂）与 §3a:137；docs/algorithms/HIPS_WRITER.md:146（FITS cards ORDERING=NESTED + NSIDE=2^k）。要点：非 2 的幂 nside 属非法输入，须在入口拒绝，不得截断静默接受。
- 当前证据（本轮真跑）：
  - 命令：timeout 60 grep -c 'nside & (nside - 1)' lib/infrastructure/aio/src/hips/aio_hips_writer.cpp → 0
  - 命令：timeout 60 grep -n 'nside < 512|leaf_order = ilog2_u64' lib/infrastructure/aio/src/hips/aio_hips_writer.cpp → 481、493、1896
  - 命令：timeout 60 grep -n 'nside = 1ULL|tile NSIDE' runtime/io/hips_core.c → 245、635
- 建议严重度：P2
- 影响：以 nside=1000 一类非幂值调用公开 C API 会产出 NSIDE=1000 + Norder0（ilog2 截断）的产品：自家 C reader 必拒（跨面不可读），而 C++ 读面（aio_hips_reader.cpp 内 NSIDE 卡 0 命中，无头卡校验）不拒 → 同一产品在两条读面一拒一收。当前树内调用方均以 1<<(order+9) 计算（如 stage2.cpp:525）故未触发。
- 整改建议：在 aio_hips_product_begin 的判据里补 if ((nside & (nside - 1)) != 0 || nside > (1ull << 24)) → 参数无效（判据照抄 module_entry.cpp:342，与已存在的正确实现收敛为一份）；顺带在 aio_hips_reader 侧补 PIXTYPE/ORDERING/NSIDE 头卡一致性校验，与 hips_core.c 对齐。
- 建议文件域：lib/infrastructure/aio/src/hips/（参照实现 lib/algorithms/drizzle/hips/src/module_entry.cpp）
- 验收门：timeout 60 grep -c 'nside & (nside - 1)' lib/infrastructure/aio/src/hips/aio_hips_writer.cpp 期望 ≥1（本轮 0）。
- GAP/任务关系：归 AIO-001；与 GAP-013（IO/HiPS 面多套并存、无统一构建成员）同族。
- 旧清单同源：M2b-A-01（HiPS 写入口对 nside 零校验：非 2 的幂与 order>29 均被接受并静默产出错产品，P0）、M2b-A-02（tile 几何合同三源互斥）— 与旧清单同源不删条；本条为收工树复验（写侧仍无幂门；旧路径 lib/hips 已迁至 lib/algorithms/drizzle/hips）。

### SCI2-07

- 定位：lib/phase3_session/p3_session.cpp:96-97（const std::string proj = doc.value("projection", std::string("TAN")); if (proj != "TAN") → ACS_ERR_UNSUPPORTED，大小写敏感）与 lib/phase3_session/p3_wcs.cpp:59-66（同一大小写敏感判据）；对照 p3_session.cpp:100-104 的 frame 判据同时接受 "icrs" 与 "ICRS"。
- 违反的最新权威条款：docs/plugins/algorithms_phase3/14_projection.md §5 配置表:31（projection 默认 tan，取值 tan/sin/car/ait/stg/mol/cea/zea，全小写字面量）；ASTROCS_DESIGN.md §5.3:266（用户通过 projection 字段选择，缺省 TAN）。要点：插件文档的字面量集合与实现接受的集合必须一致（§7「未注册投影 → 拒绝」要求口径唯一）。
- 当前证据（本轮真跑）：
  - 命令：timeout 60 sed -n '96,104p' lib/phase3_session/p3_session.cpp → if (proj != "TAN") { s->last_error = "projection must be TAN"; return ACS_ERR_UNSUPPORTED; } 紧随 if (fr != "icrs" && fr != "ICRS")
  - 命令：timeout 30 sed -n '31p' docs/plugins/algorithms_phase3/14_projection.md → | projection | tan | —— | tan/sin/car/ait/stg/mol/cea/zea |
- 建议严重度：P2
- 影响：用户照插件文档写 "projection": "tan" 会被显式拒绝（同类大小写在 frame 面已做双式兼容），文档给出的取值集合不可执行；也让合同面（contracts/data/phase_product_exchange.schema.json:223 const "TAN"）与文档面形成两套字面量而无人裁定。
- 整改建议：最小改动面 ≤3 行：在 p3_session.cpp:96 与 p3_wcs.cpp:61 对 projection 做大小写归一（与 frame 同等处理），或反向由负责人裁决把 14_projection.md §5 表的字面量改为大写并把默认写作 TAN；两者都要补一条大小写用例。
- 建议文件域：lib/phase3_session/p3_session.cpp、lib/phase3_session/p3_wcs.cpp（或文档面 docs/plugins/algorithms_phase3/14_projection.md §5）
- 验收门：timeout 60 grep -c 'proj != "TAN"' lib/phase3_session/p3_session.cpp lib/phase3_session/p3_wcs.cpp 期望两文件均 0（本轮各 1）；归一后跑 timeout 600 python3 ci/run_checks.py --check CHK-SYNTH-P3 --quiet 期望 PASS。
- GAP/任务关系：GAP 清单无对应条目 → 新事实；归 P3-002（治理反向重采样、模式与流式 FITS 输出；含配置口径）。
- 旧清单同源：idmap.csv 无 projection 字面量/大小写同源条目（M5b-C-07/M6b-C-003 只涉投影集合，不涉取值大小写）。

### SCI2-08

- 定位：写侧 lib/infrastructure/aio/src/hips/aio_hips_writer.cpp:984（kv.push_back({"hips_pixel_scale", buf})，buf 来自 :943 的 3600*180/π*√(π/3)/nside 公式，%.6f，口径为角秒）；推导权威 docs/algorithms/HIPS_WRITER.md:186「hips_pixel_scale = 3600·180/π·√(π/3)/nside arcsec」；in-tree 另三处取值 tests/io/make_hips_fixture.py:169 = 11.519173063162576、lib/phase2/tools/controlled_rejection_truth.py:52 = 0.0001125（同文件 hips_order=7、NSIDE=1<<16）、lib/infrastructure/hips_browser/healpix_browser_qt/tools/gen_geometry_truth.py:177 = 58.6；消费面只把它当字符串纳入哈希（lib/phase2/src/sampler.cpp:352 keys[] 含 hips_pixel_scale）。
- 违反的最新权威条款：docs/algorithms/HIPS_WRITER.md:186（推导权威给出唯一公式与单位）+ ASTROCS_DESIGN.md §2:82（同一数据对象只允许一种口径）。要点：既有冻结公式，则任何生产者/夹具/真值文件的该键取值都应能由公式在合法 nside 上复算。
- 当前证据（本轮真跑）：
  - 命令：timeout 120 python3（常量与三值反解 nside）→ const(arcsec*nside) = 211076.2851
  - 输出：scale=11.519173063162576 -> nside=18323.9095 log2(nside)=14.161440 2的幂? False
  - 输出：scale=58.6 -> nside=3601.9844 ... False ｜ scale=0.0001125 -> nside=1876233645.7072 ... False
- 建议严重度：P2
- 影响：hips_pixel_scale 无一致性门，夹具与真值文件可写任意量级值（差 3600 倍也不会红），据该键判读尺度的消费面（HiPS 浏览器/客户端、逐帧溯源）与产品合同不可信。
- 整改建议：(1) 由写侧公式生成一份 order→合法尺度 的共享常量表，三处夹具/truth 改为按 hips_order 计算而非手填；(2) 在 properties 校验面（lib/infrastructure/aio/v6/src/v6_hips_manifest.cpp 的 validate_hips_properties，或 lib/phase3_session/hips_properties.cpp 的必需键值域检查）增加 hips_pixel_scale 与 hips_order 的一致性判据并在键名/文档中显式声明单位。
- 建议文件域：lib/infrastructure/aio/src/hips/、lib/infrastructure/aio/v6/src/v6_hips_manifest.cpp、lib/phase3_session/hips_properties.cpp、tests/io/make_hips_fixture.py、lib/phase2/tools/controlled_rejection_truth.py、lib/infrastructure/hips_browser/healpix_browser_qt/tools/gen_geometry_truth.py
- 验收门：timeout 120 python3 -c "import math,glob;C=3600*180/math.pi*math.sqrt(math.pi/3);[print(f, [l.strip() for l in open(f) if 'hips_pixel_scale' in l], C/(1<<(int(__import__('re').search(r'hips_order=(d+)', open(f).read()).group(1))+9))) for f in ['lib/phase2/tools/controlled_rejection_truth.py']]" 期望打印值与文件值相等（本轮 0.0001125 vs 3.22077 不等）。
- GAP/任务关系：归 AIO-001；非权威标准登记面 docs/standards/STANDARDS_REGISTRY.md:98 现判 CONFORMANT，须随整改回写（该文档不作本条判据）。
- 旧清单同源：M2b-B-03（hips_pixel_scale 写角秒、标准规定单位为度，相差 3600 倍，P0）、M9-C-2（公开 C-ABI aio_wcs_pixel_scale 返回角秒且无单位声明）、M9-F-2（所谓独立重算 O4 与产品同公式做字符串等值比较 → 无独立防线）、W1-N-12（HEALPIX_SCALE_PER_NSIDE_ARCSEC 双实现）— 与旧清单同源不删条。声明：本轴未独立复核 IVOA HiPS 外部标准原文（PDF 无法抓取，HTML 版无该键），故本条只按「in-tree 三源无一满足 ALG 冻结公式 + 无机器门」立 P2，不主张外部标准单位结论。

---

## 附：本轴查过但未立条的项（宁缺毋滥）

- support 作权重族在 v6 生产权重面已封死：lib/algorithms/integration/v6/src/phase2_integrate.cpp:105-131 有 P33 撤销守卫（禁键含 support_x_snr2 / support_x_snr / median_snr_weight）与 kForbiddenWeightSources（含 support / support_area / coverage / source_snr / median_snr），与 docs/science/v6/phase2/WEIGHT_PROVENANCE_GATE.md R3/R6（:38-40）同口径 → 「v6 生产路径用 support 当科学权重」未复现；残留只由 SCI2-03/04 登记（工具面 + 公式文档标签面）。
- ivar tile 读失败的静默降级（M4-C-03 原描述）在收工树已不成立：stage2.cpp:1117-1118/1388-1389 已加 legacy_allow_weight_fallback 显式开关、fail-closed（rc=7）与逐像素计数，:549-577 另有产品整体缺失门 → 该子面判为已整改，不重复立条（仅 SCI2-04 登记其残留分支）。
- h->nside 与 hips_order 的单位口径：写侧 properties 的 hips_order 写的是 tile_order（叶 order 减 9），读侧以 1<<(order+9) 还原，两侧自洽（runtime/io/hips_core.c:245 与 aio_hips_writer.cpp:493-495）→ 未复现「order 口径互斥」。
- NSIDE=2^(K+9) 与 Norder{K}/Dir{ipix/10000}/Npix{ipix%10000} 分目录：写侧与 C 读侧一致（hips_core.c:558-560），未见偏差 → 未复现。
- 逆方差叠加主式与 support 的 canonical reducer（docs/science/INTEGRATION.md:58/:126）在 module_adapters.cpp 节点实现中未见相反数值口径 → 未复现。
- creator_did 本身：Phase1/Phase2 调用方分别传 ivo://astrocs/phase1 与 ivo://astrocs/phase2，除 SCI2-05 的两键外未见跨阶段错值 → 不单独立条。
- obs_collection：v6 写面必需（v6_hips_manifest.cpp:95-98），legacy 生产写面完全不产（grep 0 命中），Phase3 读面必需键集也不含它（hips_properties.cpp:112-116）→ 属「两套并存写面各自自校验」而非跨面互斥破坏（未见跨面消费链），故只作 SCI2-06/08 背景事实，不单独立条。
- 八投影中的 STG/MOL/CEA/ZEA 是否在其他目录另有实现：对 lib/**、runtime/** 全量检索 "(STG|MOL|CEA|ZEA)" 带引号字面量 → 零命中，确认零实现（非漏看）。
