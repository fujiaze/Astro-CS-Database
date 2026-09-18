# W2-DOC1 — docs/science/** + docs/algorithms/** 文件级穷尽审计（第二波）

- **轴名**：W2-DOC1（ROOT-004 第二波·文件级；域 = docs/science/** 与 docs/algorithms/**，全部 tracked，逐文件结论）。
- **开工基线**（`timeout 30 git rev-parse HEAD main origin/main`）：HEAD=main=`180c8a0ad9755e513670c7a1feb8fa215a87e1d7`，origin/main=`b4afc135848d8b401ee36278effcb06699542fec` —— **三 SHA 不等**；按规程等 2 分钟重试一次仍不等，**以 HEAD=main 相等为准开工并注明**。
- **收工基线**：HEAD=main=origin/main=`32a5f5f300700b817cfa1ceafe047acaeff4ee9b`（窗口后段三 SHA 重新相等；终验时三 SHA 再相等推进至 `2fc19b2188ef1c084625bc7f8c7ee47b93687d11` 且门复跑仍 rc=1；域内 `git status --porcelain docs/science docs/algorithms` 为空，全部证据可在收工树复跑）。审计窗口内 HEAD 持续推进（180c8a0→59981aa→23f42ffa→0565cad4→01754fab→32a5f5f3），各证据命令均在结果贴附时同跑 `git rev-parse HEAD` 记录。
- **枚举命令（逐字）**：`cd "/workspace/Astro CS Database" && timeout 60 git ls-files docs/science docs/algorithms` → **89 个 tracked 文件**（任务书估计约 116，本域实测 89；`git status --porcelain docs/science docs/algorithms` 域内无 UNTRACKED 新文件，窗口内 8 个 M 均已由并行线提交收编）。
- **方法**：只读权威、只登记不改写。① 域内自带机器门 `check_doc_line_anchors.py`（DOC-LINE-ANCHORS）真跑取证；② python 逐行抽取全部 docs/lib/tests/config/工程控制 路径引用并 `os.path.exists` 验真；③ DISP 偏差登记注册↔引用闭合（116 注册/118 引用逐 ID 比对）；④ SCI/ALG ID 互引闭合（含 docs/contracts/INDEX.yaml 注册表与集合号段展开）；⑤ §节互引存在性（1600+ 处）；⑥ 「以代码为准」裁决语、孤儿数值、版本化抬头普查；⑦ 与 docs/design/UNIFIED_MODEL.md §2 对象表逐项术语对读。
- **同源不重复立条**：support×snr² 权重（M3-A-002/M7-A-002）、八投影 registry/CRVAL2（M1a-A-003）、PHASE3_C 互斥（M1a-A-007/008）、ivar 单位（M3-A-004）等第一波 OPEN 账命中域内文本，本轮不另立条，仅在相关发现作同源标注。
- **发现计数**：P0 = 0 ｜ P1 = 4（W2-DOC1-1/2/3/5）｜ P2 = 4（W2-DOC1-4/6/7/8）。

## 发现表

| ID | 定位 | 违反的最新权威条款 | 当前证据（命令+输出≤3行） | 严重度 | 影响 | 整改建议 | 建议文件域 | 验收门（单命令） | GAP·TASK·第一波·旧账本同源 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| W2-DOC1-1 | docs/algorithms/{CALIBRATION_ALGORITHMS,COSMETIC_ALGORITHMS,NOISE_ESTIMATION,PHOTOMETRIC_FIT,PLATESOLVE,STAR_DETECTION_ALGORITHMS,STAR_PSF_ALGORITHMS,anchors/ANCHOR_CONTRACT,anchors/anchor_contract.json}.md + docs/science/{CALIBRATION,NOISE_MODEL,PSF}.md（95 错误逐条定位见日志） | docs/algorithms/anchors/ANCHOR_CONTRACT.md §2.1（锚失效必须同一提交内更新）+ §4 C2/C4；ENGINEERING_SPEC.md §8（删除/重命名无悬空引用） | 命令：timeout 240 python3 docs/algorithms/anchors/check_doc_line_anchors.py；输出：rc=1（@32a5f5f3）；anchors_total=808、errors=95（C2_anchor_resolved×79 + C4_symbol_binding×16）；by_status UNTRACKED=80 | P1 | 活动门 DOC-LINE-ANCHORS（ci/checks.json:1000，fast/linux-main/windows-main，waivable=false）在 main 上红：ARCH-001 迁移删除 lib/calibration、lib/snr_estimator、lib/plate_solve、lib/star_detector、lib/dynamic_psf 等旧树后，10 篇 SCI/ALG 文档行锚与 anchor_contract.json 的 resolvers/bindings（12+ 目标）未随迁移更新；「锚点可定位」合同失效，整改按锚定位将扑空；ANCHOR_CONTRACT.md §1 自报规模「36 文档/793 锚」亦陈旧（实测 808） | 按 §2.1 将各文档锚与 anchor_contract.json 指向迁后新址（lib/algorithms/** 与 lib/infrastructure/**），同步 §1 规模行；不改任何公式/容差语义 | docs/algorithms/**（含 anchors/）+ docs/science/{CALIBRATION,NOISE_MODEL,PSF}.md | timeout 240 python3 docs/algorithms/anchors/check_doc_line_anchors.py; echo rc=\$? → 期望 rc=0 | 第一波 LIB2 移交预警（AUDIT_INDEX.md:29「旧路径不同批更新则 main 红」已应验）；旧账本 M3b-E-01 同族（锚漂移主题，本条为迁移新发实例）；归 DOC-001（门恢复）+ARCH-001（迁移责任面） |
| W2-DOC1-2 | docs/algorithms/{DRIZZLE_GEOMETRY:49/:254-256,GAIA_QUERY:6-7/:175,HEALPIX_MAPPING:3,HIPS_WRITER:8-10/:339,PHASE2_MOSAIC_WRITE:403/:422,PHASE3_PROJ_IMPL:374-375,PHASE3_RESAMPLE:10,PHASE3_RSMP_IMPL:127,STAR_*} + docs/science/{ASTROMETRY:66/:72/:160-161,DRIZZLE:71/:131,PHASE2_UPM:120,PHOTOMETRY:65/:120-121,v6/observation/OBSERVATION_MODEL_REVIEW:196/:324} + anchors/anchor_contract.json 等（67 个失效路径、约 150 处、35 文件） | ENGINEERING_SPEC.md §8（无悬空引用）；ASTROCS_DESIGN.md §0（docs/science 公式权威文本自身失实即权威偏差）；ANCHOR_CONTRACT.md §2.1（语义同源义务） | 命令：timeout 240 python3 /tmp/w2doc1_refscan.py（@32a5f5f3）；输出：TOTAL: 67；例：lib/healpix_db/healpix_drizzle/drizzle_engine.cpp <- docs/algorithms/DRIZZLE_GEOMETRY.md:30, docs/science/DRIZZLE.md:131；lib/common/healpix（HEALPIX_MAPPING.md:3 自称「权威实现」）→ 现址 lib/algorithms/shared/healpix | P1 | 不带行号的「实现唯一生产源/权威实现/合同头」散文指针不受 DOC-LINE-ANCHORS 门覆盖（门只验 path:N 锚），迁移后成片悬空：FROZEN SCI（DRIZZLE/PHOTOMETRY/ASTROMETRY/PHASE2_UPM 等 9 篇）与 CONTRACT_READY ALG（HIPS_WRITER 抬头 :8-10、GAIA_QUERY 权威源码行、HEALPIX_MAPPING）对已不存在文件作现行事实断言；读者与下游整改按指针寻源必错 | 与 W2-DOC1-1 同批把散文指针改迁后新址；建议把「无行号 path 引用」纳入 DOC-LINE-ANCHORS 或新增 CI 检查（走 docs/ci 新增流程：能红能绿） | docs/science/** + docs/algorithms/**（35 文件清单见日志） | timeout 240 python3 -c "exec(open('/tmp/w2doc1_refscan.py').read())" 等价复跑 → 期望 TOTAL: 0（验收时可改指一次性同款扫描；或临时门 grep -rn "lib/astro_image_io\|lib/healpix_db\|lib/common/healpix\|lib/phase3_proj" docs/science docs/algorithms \| wc -l → 0） | GAP-006/013 族（目录并存/唯一 AIO——代码侧已由迁移收口，文档侧未闭合）；第一波 AIO/ARCH 各条以代码为对象未含此文档面；归 DOC-001 |
| W2-DOC1-3 | 全域 32 篇以「宪章/CONSTITUTION」为权威锚（文件名级显式引用 12 篇：docs/science/PHASE3_HIPS_TO_FITS.md:5-8、docs/algorithms/PHASE3_PROJ_IMPL.md:350-359、v6/phase1/{ALG_P1_001_PHASE1_ALGORITHM_SPEC.md:22,alg_p1_001_spec.json}、v6/phase2-point/ALG-P2-POINT-001_SPEC.md、v6/phase2-{psfsw,surface}/README.md、v6/phase3/ALG-P3-001_{SPEC,VERIFICATION}.md、v6/adjudication/SCI-ADJ-001_CONFLICT_MATRIX.md:10、v6/phase2/SCI-P2-001_THREE_MODE_REVIEW.md、v6/phase3/PHASE3_PROPAGATION_REVIEW.md、v6/psfw/PSFW_FREEZE_RESEARCH.md:9/:350） | ASTROCS_DESIGN.md §0（本文与其他任何文档冲突以本文为准；权威链 DESIGN>AGENTS>ENGINEERING_SPEC>CONTROL_PACK_SPEC>docs/ci>docs/plugins）；SHARD_BRIEF.md §2（ASTROCS_PROJECT_CONSTITUTION.md 不在权威链上；其要求无对应者 VOID）；ENGINEERING_SPEC.md §8（活动文档无历史状态冒充现行） | 命令：timeout 20 test -f ASTROCS_PROJECT_CONSTITUTION.md && echo E \|\| echo MISSING；输出：MISSING；命令：git log --oneline -1 --diff-filter=D -- ASTROCS_PROJECT_CONSTITUTION.md；输出：01db973b chore(root): 清除作废世代治理入口（ROOT-007） | P1 | v6 规范整波文档把已废止且已从树中删除的宪章列为最高上位权威（「冻结宪章 > PROJECT_SPEC > PHASE 详细设计 > … > 代码」分层直接抵触 DESIGN §0），裁决效力、supersession 依据（PHASE3_HIPS_TO_FITS「宪章 supersession 生效」更新块、02_SIGNOFF SO 表「权威：宪章 §1.2」列）悬于不存在文件；科学合同的上位来源不收敛 | 由 DOC-001 统一将 v6/宪章系权威锚改射 DESIGN §0 权威链（保留「历史依据」注记），条款号映射按 SHARD_BRIEF §2 旧->新表逐条替换；不得由 Agent 自行放宽（DESIGN §0） | docs/science/** + docs/algorithms/**（32 篇，文件级 12 篇优先） | timeout 60 grep -rl "ASTROCS_PROJECT_CONSTITUTION\|ASTROCS-CONSTITUTION-001" docs/science docs/algorithms \| wc -l → 期望 0 | 与 GAP-002/GAP-019 同源（旧宪章/废止控制包引用，本轴=docs 子集不删条）；第一波 GOV-1/GOV-2/GOV-4 同主题（根面/索引面/测试面），本条为文档域补集；归 GOV-001+DOC-001 |
| W2-DOC1-4 | 15 文件 16 处指向已废止控制包 工程控制/AstroCS_PARALLEL_SCIENCE_IMPLEMENTATION_V6_20260915/**：docs/algorithms/v6/phase1/{README.md,EVIDENCE.md:4,tools/verify_alg_p1_001.py:5,ALG_P1_001_PHASE1_ALGORITHM_SPEC.md:4,alg_p1_001_spec.json:6}、v6/phase2-point/{README.md:3,ALG-P2-POINT-001_SPEC.md:5}、v6/phase2-psfsw/README.md:4、v6/phase2-surface/README.md:4、v6/phase3/README.md:9（CONTROLLER_LOG）、docs/science/v6/{adjudication/README.md:3,observation/OBSERVATION_MODEL_REVIEW.md:3,phase2/README.md:3,psfw/PSFW_FREEZE_RESEARCH.md:4} | CONTROL_PACK_SPEC.md §4（控制包目录唯一模板 工程控制/<包ID>/）；SHARD_BRIEF.md §2（旧控制包路径 AstroCS_* 已删）；ENGINEERING_SPEC.md §8（删除/重命名无悬空引用）、§2 精神（禁任务流水入正文） | 命令：timeout 20 ls 工程控制/；输出：PROJECT-GOVERNANCE-01 SCI-RES-01（无 AstroCS_*）；命令：timeout 60 grep -rl "AstroCS_PARALLEL_SCIENCE_IMPLEMENTATION_V6_20260915" docs/science docs/algorithms；输出：15 个文件（清单见左） | P2 | v6 规范正文与机器伴生（verify 工具、spec.json）的任务抬头/写域依据指向不存在目录；「由控制器验收」判据（phase2-point/README.md:6 建议状态 PASS 的验收出处）不可复核，追溯链半断 | 各文档「任务:」行改指现行控制包归口（工程控制/PROJECT-GOVERNANCE-01/…）或就地降级为「历史出处（包已废止）」注记；verify_alg_p1_001.py 仅注释路径，同步改注 | docs/science/v6/** + docs/algorithms/v6/** | timeout 60 grep -rl "AstroCS_PARALLEL_SCIENCE_IMPLEMENTATION_V6_20260915" docs/science docs/algorithms \| wc -l → 期望 0 | 与 GAP-019 同源（同一失效路径全仓引用 337 处的 docs 子集，不删条）；第一波 GOV-2 同族（索引登已删包）；归 DOC-001 |
| W2-DOC1-5 | docs/algorithms/NOISE_ESTIMATION.md:151-153 ↔ docs/science/NOISE_MODEL.md:37/:48/:82 | ASTROCS_DESIGN.md §0（科学公式权威=docs/science；代码不得裁决 SCI）；ENGINEERING_SPEC.md §3（默认容差不可修改、唯一源）；ANCHOR_CONTRACT.md §2.3+§4.1（SCI 漂移须在 ALG 侧以登记条目显式化，现行实例 FROZEN_SCI_CLAIM/DISP-*） | 命令：timeout 10 sed -n '37p' docs/science/NOISE_MODEL.md; sed -n '152p' docs/algorithms/NOISE_ESTIMATION.md；输出：…min_samples（patch 样本数阈）默认 5；…SCI §4 "min_samples 默认 5" 为旧稿数字——**不改 SCI**，以代码为准登记；（§13.3 DISP-NOISE-001..009 无此条） | P1 | FROZEN SCI 默认容差（5）与实现默认（64）互斥并存 12+ 天；ALG 以「以代码为准」裁决语活动残留替代正式登记——既不撤 SCI 文本、又不立 DISP 条目、亦未走负责人变更流程（§1.2 通道），下游按任一数字实现均无权威依据；patch 合格阈直接改变噪声场控制点密度（科学影响面） | 登记为 DISP-NOISE 新条目（缺陷=SCI 数字与实现不一致，整改归 SCI 变更流程），或由负责人授权按 b0353303 P5 先例订正 SCI §4 后删除裁决语；禁止保留「以代码为准」为终态 | docs/science/NOISE_MODEL.md + docs/algorithms/NOISE_ESTIMATION.md | timeout 20 grep -c "以代码为准" docs/algorithms/NOISE_ESTIMATION.md → 期望 0（且 DISP/SCI 二选一收口可复核） | 第一波 M7-A-104 同源相邻（该条判 N=5 的统计缺陷 OPEN，本条判 5-vs-64 冲突+未登记裁决语，角度不同不合并）；GAP 无同号；归 P1-002+DOC-001 |
| W2-DOC1-6 | docs/algorithms/CALIBRATION_ALGORITHMS.md:391 vs :415（DISP-CAL-010 双义）；docs/algorithms/STAR_DETECTION_ALGORITHMS.md:40（DISP-STAR-006 悬空） | ANCHOR_CONTRACT.md §4.1（登记纪律：一条一对象可复核）；ENGINEERING_SPEC.md §8（偏差登记须唯一指向）；DESIGN §12 发布前提（全部偏差已登记） | 命令：timeout 10 sed -n '391p;415p' docs/algorithms/CALIBRATION_ALGORITHMS.md；输出：- DISP-CAL-010（B2-A6 登记）generate_master_flat 步骤1 的帧 median …；- DISP-CAL-010 ac:: 层 n_frames<=0 仅日志不返回 …；（DISP-STAR-006 全文 grep 注册=0，仅 :40 引用） | P2 | 同一 ID 承载两条不同缺陷：B2-A6 销账时无法区分对象，闭合状态可能互相掩盖；DISP-STAR-006 被引用为「已登记」而注册不存在（该缺陷= norm 硬编码 65535 只散落于 §2 正文） | CALIBRATION_ALGORITHMS §10 第二条改号 DISP-CAL-012（或按内容拆分）并保留互见；STAR_DETECTION_ALGORITHMS §11.3 补 DISP-STAR-006 条目或删除 :40 引用 | docs/algorithms/{CALIBRATION_ALGORITHMS,STAR_DETECTION_ALGORITHMS}.md | timeout 20 grep -c "^- DISP-CAL-010" docs/algorithms/CALIBRATION_ALGORITHMS.md → 期望 1 | DISP-STAR-006 与旧账本 M3b-E-01/M3b-H-03 同源（两账均判「补或删」OPEN）；DISP-CAL-010 双义为全仓首报；归 DOC-001 |
| W2-DOC1-7 | docs/science/PSF.md:106（ALG-STARPSF-002）、docs/science/ASTROMETRY.md:155（ALG-WCS-002）、docs/science/PHASE2_UPM.md:115（ALG-UPM-004）、docs/science/STAR_DETECTION.md:32/:35/:61（SCI-PHOTOMETRY-001/SCI-ASTROMETRY-001） | ENGINEERING_SPEC.md §8（算法引用有效 SCI/ALG、无悬空引用）；旧宪章 §12.3-9 之现行替代即此条（SHARD_BRIEF §2 映射） | 命令：timeout 20 grep -c "STARPSF-002" docs/algorithms/STAR_PSF_ALGORITHMS.md; grep -c "ALG-WCS-002" docs/algorithms/PLATESOLVE.md; grep -c "ALG-UPM-004" docs/algorithms/UPM_SOLVER.md；输出：0/0/0（三个被 FROZEN SCI 点名的下游 ALG ID 在对应文档与 INDEX.yaml 均无定义） | P2 | SCI §12「关联 ALG ID」清单与 ALG 文档实际覆盖面脱节：θ 消歧/SIP 解析/UPM 持久化三项科学条款无推导权威承接；SCI-PHOTOMETRY-001、SCI-ASTROMETRY-001 两个不存在的 ID 变体（实名 SCI-PHOT-001、SCI-WCS-001/SCI-AST-001 别名）造成互引假象 | ALG 文档补 002/004 覆盖号段（或在 SCI 侧撤条目走变更流程）；STAR_DETECTION.md 三处 ID 改实名；纳入 DOC-001 引用收敛批 | docs/science/{PSF,ASTROMETRY,PHASE2_UPM,STAR_DETECTION}.md + 对应 ALG | timeout 30 grep -rc "ALG-STARPSF-002\|ALG-WCS-002\|ALG-UPM-004" docs/algorithms/STAR_PSF_ALGORITHMS.md docs/algorithms/PLATESOLVE.md docs/algorithms/UPM_SOLVER.md → 三文件均 ≥1 且 SCI 变体清零（grep SCI-PHOTOMETRY-001 命中 0） | 第一波与 GAP 未含（新报）；归 DOC-001 |
| W2-DOC1-8 | docs/science/SCIENCE_SCOPE.md（全篇）、:62；docs/science/UNCERTAINTY_AND_COVARIANCE.md:21/:26/:30-48；docs/algorithms/HEALPIX_MAPPING.md:3 | ENGINEERING_SPEC.md §8（活动文档无陈旧版本号/历史状态冒充现行）；ASTROCS_DESIGN.md §12 + ENGINEERING_SPEC §7（Alpha 前版本信息下线）；SHARD_BRIEF §2（docs/traceability 不在权威链） | 命令：timeout 10 head -6 docs/science/SCIENCE_SCOPE.md; sed -n '62p' 同文件；输出：# Science Scope/## 目的（无 ID/状态/日期抬头，兄弟 16 篇 SCI 均有「> ID: … 状态: FROZEN(日期)」）；SCI-DRZ-* … # B5-06 同步在 TRACEABILITY.csv 增 …status=VERIFIED…（废止追溯面流水残留） | P2 | 权威链入口文档（SCI-SCOPE-001 为全部 SCI 的上游引用对象）自身无版本化抬头，且内嵌 B5-06 任务流水与对已废止 TRACEABILITY.csv 的 VERIFIED 宣称；UNCERTAINTY 通篇以「V19/V19R3」旧版本号限定现行权威、SNR-012 legacy ID 裸用；HEALPIX_MAPPING 无抬头且「权威实现」指针失效（并入 W2-DOC1-2 处理路径） | 三篇补「ID+状态+冻结日期」抬头（照兄弟文档格式）；删 B5-06 行与 V19 限定词或改中性表述；legacy SNR-012 改指 SCI-NOISE 新号 | docs/science/{SCIENCE_SCOPE,UNCERTAINTY_AND_COVARIANCE}.md + docs/algorithms/HEALPIX_MAPPING.md | timeout 30 grep -c "V19\|B5-06" docs/science/SCIENCE_SCOPE.md docs/science/UNCERTAINTY_AND_COVARIANCE.md → 均 0 | 第一波未覆盖（SCI1/SCI2 轴中止无发现）；与 GOV-1 精神同族（陈旧权威状态）；归 DOC-001 |

## 覆盖清单

域内 89 个 tracked 文件，每行一条 `<相对路径>\t<VERDICT>`（VERDICT ∈ OK / FINDING:<ID>；多命中按最小 ID 归主条，余见发现行定位列）。无 generated/binary/moved-wip/duplicated-by-wip 对象：域内全部为文本权威文档，窗口内 M 状态均已由并行线提交收编（收工时域内 porcelain 为空）。

```tsv
docs/algorithms/ACR_EQUIVALENCE.md	OK
docs/algorithms/CALIBRATION_ALGORITHMS.md	FINDING:W2-DOC1-1
docs/algorithms/COSMETIC_ALGORITHMS.md	FINDING:W2-DOC1-1
docs/algorithms/DRIZZLE_GEOMETRY.md	FINDING:W2-DOC1-2
docs/algorithms/GAIA_QUERY.md	FINDING:W2-DOC1-2
docs/algorithms/HEALPIX_MAPPING.md	FINDING:W2-DOC1-2
docs/algorithms/HIPS_WRITER.md	FINDING:W2-DOC1-2
docs/algorithms/INTEGRATION_ALGORITHMS.md	OK
docs/algorithms/NOISE_ESTIMATION.md	FINDING:W2-DOC1-1
docs/algorithms/PHASE2_COVERAGE.md	OK
docs/algorithms/PHASE2_INTEGRATION.md	OK
docs/algorithms/PHASE2_MOSAIC_WRITE.md	FINDING:W2-DOC1-2
docs/algorithms/PHASE2_REJECTION.md	OK
docs/algorithms/PHASE2_SAMPLER.md	OK
docs/algorithms/PHASE2_SESSION.md	OK
docs/algorithms/PHASE2_UPM_IMPL.md	OK
docs/algorithms/PHASE3_FITS_IMPL.md	OK
docs/algorithms/PHASE3_PROJ_IMPL.md	FINDING:W2-DOC1-2
docs/algorithms/PHASE3_RESAMPLE.md	FINDING:W2-DOC1-2
docs/algorithms/PHASE3_RSMP_IMPL.md	FINDING:W2-DOC1-2
docs/algorithms/PHOTOMETRIC_FIT.md	FINDING:W2-DOC1-1
docs/algorithms/PLATESOLVE.md	FINDING:W2-DOC1-1
docs/algorithms/REJECTION_ALGORITHMS.md	OK
docs/algorithms/STAR_DETECTION_ALGORITHMS.md	FINDING:W2-DOC1-1
docs/algorithms/STAR_PSF_ALGORITHMS.md	FINDING:W2-DOC1-1
docs/algorithms/UPM_SOLVER.md	OK
docs/algorithms/anchors/ANCHOR_CONTRACT.md	FINDING:W2-DOC1-1
docs/algorithms/anchors/anchor_contract.json	FINDING:W2-DOC1-1
docs/algorithms/anchors/check_doc_line_anchors.py	OK
docs/algorithms/v6/frozen/00_README.md	OK
docs/algorithms/v6/frozen/01_NUMERIC_THRESHOLD_FREEZE.md	OK
docs/algorithms/v6/frozen/02_GATE_AND_MUTATION_FREEZE.md	FINDING:W2-DOC1-3
docs/algorithms/v6/phase1/ALG_P1_001_PHASE1_ALGORITHM_SPEC.md	FINDING:W2-DOC1-3
docs/algorithms/v6/phase1/ALG_P1_001_TEST_MATRIX.md	FINDING:W2-DOC1-3
docs/algorithms/v6/phase1/EVIDENCE.md	FINDING:W2-DOC1-4
docs/algorithms/v6/phase1/README.md	FINDING:W2-DOC1-4
docs/algorithms/v6/phase1/alg_p1_001_spec.json	FINDING:W2-DOC1-3
docs/algorithms/v6/phase1/alg_p1_001_test_matrix.json	FINDING:W2-DOC1-3
docs/algorithms/v6/phase1/tools/verify_alg_p1_001.py	FINDING:W2-DOC1-4
docs/algorithms/v6/phase2-point/ALG-P2-POINT-001_GATES.md	OK
docs/algorithms/v6/phase2-point/ALG-P2-POINT-001_SPEC.md	FINDING:W2-DOC1-3
docs/algorithms/v6/phase2-point/README.md	FINDING:W2-DOC1-4
docs/algorithms/v6/phase2-psfsw/PSFSW_ALGORITHM_SPEC.md	FINDING:W2-DOC1-3
docs/algorithms/v6/phase2-psfsw/PSFSW_BASELINE_COMPARISON.md	OK
docs/algorithms/v6/phase2-psfsw/PSFSW_FROZEN_THRESHOLDS.md	OK
docs/algorithms/v6/phase2-psfsw/PSFSW_GATES_AND_MUTATIONS.md	OK
docs/algorithms/v6/phase2-psfsw/README.md	FINDING:W2-DOC1-3
docs/algorithms/v6/phase2-surface/ALG-P2-SURF-COVARIANCE-EPSF.md	OK
docs/algorithms/v6/phase2-surface/ALG-P2-SURF-GLS.md	FINDING:W2-DOC1-3
docs/algorithms/v6/phase2-surface/ALG-P2-SURF-PIXIVAR-GATE.md	OK
docs/algorithms/v6/phase2-surface/ALG-P2-SURF-REJECTION.md	FINDING:W2-DOC1-3
docs/algorithms/v6/phase2-surface/ALG-P2-SURF-UPM.md	FINDING:W2-DOC1-3
docs/algorithms/v6/phase2-surface/ALG-P2-SURF-VERIFICATION.md	FINDING:W2-DOC1-3
docs/algorithms/v6/phase2-surface/README.md	FINDING:W2-DOC1-3
docs/algorithms/v6/phase3/ALG-P3-001_KERNEL_REGISTRY.md	OK
docs/algorithms/v6/phase3/ALG-P3-001_SPEC.md	FINDING:W2-DOC1-3
docs/algorithms/v6/phase3/ALG-P3-001_VERIFICATION.md	FINDING:W2-DOC1-3
docs/algorithms/v6/phase3/README.md	FINDING:W2-DOC1-4
docs/science/ACR_EQUIVALENCE.md	OK
docs/science/ASTROMETRY.md	FINDING:W2-DOC1-2
docs/science/CALIBRATION.md	FINDING:W2-DOC1-1
docs/science/CONTROL_WEIGHT_SNR.md	OK
docs/science/DRIZZLE.md	FINDING:W2-DOC1-2
docs/science/INTEGRATION.md	OK
docs/science/NOISE_MODEL.md	FINDING:W2-DOC1-1
docs/science/PHASE2_UPM.md	FINDING:W2-DOC1-2
docs/science/PHASE3_HIPS_TO_FITS.md	FINDING:W2-DOC1-3
docs/science/PHOTOMETRY.md	FINDING:W2-DOC1-2
docs/science/PSF.md	FINDING:W2-DOC1-1
docs/science/PSF_SIGNAL_WEIGHT.md	OK
docs/science/REJECTION.md	OK
docs/science/SCIENCE_SCOPE.md	FINDING:W2-DOC1-8
docs/science/STAR_DETECTION.md	FINDING:W2-DOC1-7
docs/science/UNCERTAINTY_AND_COVARIANCE.md	FINDING:W2-DOC1-3
docs/science/UNIFIED_SCIENCE_MODEL.md	OK
docs/science/v6/adjudication/README.md	FINDING:W2-DOC1-4
docs/science/v6/adjudication/SCI-ADJ-001_CONFLICT_MATRIX.md	FINDING:W2-DOC1-3
docs/science/v6/adjudication/SCI-ADJ-001_FREEZE_LIST.md	FINDING:W2-DOC1-3
docs/science/v6/frozen/00_README.md	FINDING:W2-DOC1-3
docs/science/v6/frozen/01_SEMANTIC_FREEZE.md	FINDING:W2-DOC1-3
docs/science/v6/frozen/02_SIGNOFF_CONTROLLER_SUPERSEDED.md	FINDING:W2-DOC1-3
docs/science/v6/observation/OBSERVATION_MODEL_REVIEW.md	FINDING:W2-DOC1-2
docs/science/v6/phase2/COVARIANCE_AND_EFFECTIVE_PSF.md	FINDING:W2-DOC1-3
docs/science/v6/phase2/README.md	FINDING:W2-DOC1-4
docs/science/v6/phase2/SCI-P2-001_THREE_MODE_REVIEW.md	FINDING:W2-DOC1-3
docs/science/v6/phase2/VERIFICATION_AND_EVIDENCE.md	OK
docs/science/v6/phase2/WEIGHT_PROVENANCE_GATE.md	FINDING:W2-DOC1-3
docs/science/v6/phase3/PHASE3_PROPAGATION_REVIEW.md	FINDING:W2-DOC1-3
docs/science/v6/psfw/PSFW_FREEZE_RESEARCH.md	FINDING:W2-DOC1-3
```

files_total = 89
verdict_counts = {OK: 28, FINDING:W2-DOC1-1: 12, FINDING:W2-DOC1-2: 13, FINDING:W2-DOC1-3: 27, FINDING:W2-DOC1-4: 7, FINDING:W2-DOC1-7: 1, FINDING:W2-DOC1-8: 1}
枚举命令逐字 = cd "/workspace/Astro CS Database" && timeout 60 git ls-files docs/science docs/algorithms

## 查而未立（宁缺毋滥记录）

- docs/algorithms/v6/phase1/ALG_P1_001_PHASE1_ALGORITHM_SPEC.md:24 引 FREEZE_LIST §3：目标实际含 3.1..3.8 族节，指向成立，不立条。
- docs/algorithms/HIPS_WRITER.md:15「SCI 无覆盖处以实现为准登记」：与 ANCHOR_CONTRACT §2.3 规程一致，属合规登记语而非裁决残留，不立条（对照 W2-DOC1-5 的不合规实例）。
- docs/algorithms/DRIZZLE_GEOMETRY.md DISP-DRZ-005「原登记已反转」条目：保留纠错语境与三处实测锚，属登记纪律内的原位修正，不立条。
- SCI-CW §4 support×snr_v²、PHASE2_INTEGRATION ivar:support 回退、PHASE3_HIPS_TO_FITS §9a 内部矛盾、八投影 TODO：第一波 OPEN 账在案（M3-A-002、M7-A-002、M1a-A-007/008、M1a-A-003），本轮逐字复核仍在，不重复立条。
- docs/science/PHASE3_HIPS_TO_FITS.md:141「UNRESOLVED-SCIENCE=0」正面宣称：与同文 :66/:81 段互斥并存——缺陷本体即 M1a-A-008（同源不另立）。
- DISP-COV-003（已划~~关闭~~）、DISP-UPM-003（注册于 PUBLIC_API.md，域外存在）：闭合成立，不立条。
- reports/v6/**、docs/contracts/v6/frozen/*.json、docs/owner/PROJECT_SPEC.md、docs/design/PHASE{1,2,3}_DETAILED_DESIGN.md：引用目标实测存在（宪章链问题除外，已立 W2-DOC1-3）。

## 本轴合规声明

零修复零 git 写（仅 rev-parse/ls-files/log/status/cat-file 与只读 python 解析）；未触碰工程控制/**、其他线工作区与仓库根条目；未读取 FATDUCK_ACCESS.md（全域扫描仅路径正则，未打开该文件名）；本轴写入 = 本报告 + run/PROJECT-GOVERNANCE-01/ROOT-004/logs/audit/w2_DOC1.log 两文件；grep/find 全程限定 docs/science、docs/algorithms 与点名路径，巨目录未入扫描；所有外部命令带 timeout 10~300。
