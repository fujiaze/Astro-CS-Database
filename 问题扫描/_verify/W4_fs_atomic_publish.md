# W4 · 文件系统与原子发布面 — 第二轮全仓静态审查（复验档案）

> 轴：路径拼接与相对根假设（cwd 依赖）· TOCTOU · 覆盖 vs 拒绝存在 · 目录创建失败处理 · fsync/rename 原子替换是否真按 §11 · 符号链接与长路径 · 大小写与保留名（Windows）· dirty 工作区产物落位 · 发布包清单与实际文件对账
> 时点：HEAD `a3a343a44080d917089e1f8d548ed2d0400b0c61`；全部行锚为该时点工作区实测（含 dirty 现场，锚以符号+行号双给）
> 纪律：纯静态（read/grep + 只读 python3 -B + `git --no-optional-locks` 只读子命令）；未跑任何仓内脚本/构建/ctest；结构化文件用解释器解析
> 权威顺序：①负责人设计意图 ②现行冻结文本（宪章 §11:404、§8.3、§16.1/16.2/16.3、§14.2 与 AGENTS 目录规范）③代码测试自洽
> 免重报：SUMMARY.md 簇表 14 机制与 INDEX.md §三 已通读；M9-G-4（511 塌缩）/M9-G-5（rename 无 fsync + E4 十站点互斥写法）/M9-G-6（write_properties 吞错）/M2b-C-01（HiPS 直写）/M6a-C-003（"原子发布内建"失实）/M9-C-1（UTF-8→UTF-16 双缺）/M5b_L12_L17（CLI 默认落 CWD，含 config init 模板与 benchmark 默认）/V9-b（dirty_ignore 裸根名）/V5-N-01/02（长路径拒绝缝）/M8a-C-006（发布包许可字面量）/M8a R7（check_release_layout 等零门注册）全部排除或仅作 related 锚，不重报其判词。

## 排除前缀（普查口径，全量）
`./run/`、`./问题扫描/`、`./设计大纲/`、`./工程控制/`、`./build/`、`./out/`、`./third_party/`（vendored 内部形态，维持 SUMMARY 未覆盖面声明；`lib/astro_image_io/third_party/cfitsio` 亦排除）、`./GaiaDR3/`、`./GaiaDR3SP/`、`./BASS DR3/`、`./AstroCS.wiki/`

## 条目（8 条 = P1×2 + P2×6；上限 15，宁窄而实）

### W4-R2-01（P1）aio_publish 删除/校验遍历的 snprintf 拼接一律不查截断，长路径下按「前缀路径」操作——rmrf 面可致整删同名兄弟目录
- 类别: C_ALG_IMPL/交付安全（主，§11 全有或全无不变量的对偶破坏）；次挂 G_GOV_GATE
- 位置: `lib/hips/src/aio_publish.cpp`::publish_rmrf（:166-177 Win 分支 child 拼接、:184-191 POSIX 分支）、::publish_fsync_tree（:221-222/:228/:249）、::publish_dir_is_nonempty（:126-127）；缓冲常量 PUBLISH_PATH_MAX=1024（:39/:54）
- 判据对照（同文件双纪律）: stage_create :299 与 stage_discard :328 对顶层 snprintf **有** `n>=sizeof` 检查并返回 PARAM；上述三处拼接后**直接递归使用**，无返回值检查
- 机理: child = `"%s\\%s"`(path,name) 截断结果 = path+name 的前缀；前缀可命中同父目录真实兄弟（如 `Npix_123` 截为 `Npix_12`）→ publish_stat_exists(:115) 判存在 → 递归 PUBLISH_UNLINK/PUBLISH_RMDIR 整删；fsync_tree 面则 fsync 错对象、n_files/total_bytes 错源（校验计数≠实际发布树）。promote 的"目标拒绝非空"(:356-361) 因同型截断（pat 缓冲）可在错路径上探测，非空拒绝失效
- 触发预算: strlen(parent)+1+len(name) ≥ 1024。Linux PATH_MAX=4096 → 511~4095 合法深径可覆盖（引 M9-G-4【E4-实测】行口径）；中文目录 3 B/字加剧。生产接线：astrocs.p1.hips 事务面 stage/discard/promote（引 M9-C-1 影响行"生产可达"口径）。实际深路径复现 = 需运行期，未判
- 权威依据: 宪章 §11:404（临时/原子提交与"失败不得留下可被误认半成品"）、§8.3（AIO 唯一 I/O 边界）、§14.4
- 目录创建失败处理子面（同文件同纪律族）: ::publish_mkdir_parents :98-113 —— 注释自述「EEXIST 忽略」而实现为 **PUBLISH_MKDIR 返回与 errno 全丢弃**（:107/:111 两处不判任何返回值）：中间段 EACCES/ENOSPC/EROFS 被吞，真实原因不可恢复；错误只能在 :311 末段 `PUBLISH_MKDIR(stage)` 处显形且塌缩为不带 errno 的 AIO_PUBLISH_ERR_IO（"归因可定位"违 §11 结构化诊断与 CLI_PROTOCOL"失败可归因"精神；对照 :312 EEXIST→STATE 尚有分档，父段失败连分档都没有）
- related: M9-G-4（不同文件不同机理：512 截断塌缩；本条是删除面越权）、M9-C-1（同文件 ANSI 面）、簇 6（新边界不继承老边界纪律：同文件 stage_create 已有正确检查形态）、W4-R2-04（CLI 侧 create_directories 吞错同族）
- 建议处置: 三处 snprintf 判 `n>=cap` 即返回 ERR_IO（对齐 :299/:328 写法）；或将缓冲提到 PATH_MAX 并统一动态分配；补「深径+同名兄弟」负例（断言拒删/错删零发生）

### W4-R2-02（P1）aio_publish 递归删除随符号链接（POSIX stat 判目录 / Win 不检 REPARSE_POINT）+ stage 名完全确定 + 「同名残留自愈删除」→ 并发互踩与树外删除（CWE-59），Windows 大小写不敏感放大
- 类别: C_ALG_IMPL/交付安全（主）；次挂平台盲区（M9 §4 总述下新实例）
- 位置: `lib/hips/src/aio_publish.cpp`::publish_stat_exists（:115-120 用 `stat()`——POSIX 随符号链接解析）、::publish_rmrf（:157-163 判 is_dir 后 :181-196 opendir 递归删除；Win :164-179 仅 `FILE_ATTRIBUTE_DIRECTORY`，`FILE_ATTRIBUTE_REPARSE_POINT` 全文件 0 出现）、::stage_create（:296-307 确定性名 `<parent>/.<base>.hips_staging.tmp`（publish.h:47 `ASTROCS_HIPS_STAGE_BASENAME ".hips_staging.tmp"`，无 pid/随机段）+ :302-307「RAII 自愈：同名残留先确定性删除」→ :311-313 EEXIST 才判"病态并发占用"——防线在删除之后，永不先到）、::promote（:351-362 无树内容校验）
- 注释失实子事实: fsync_tree :264-265 注释「非常规项…病态输入由 promote 前 STATE/IO 兜底」——promote 实际只判 stage/target 目录态与目标非空，无任何"非常规项"检出；symlink/fifo 随 rename 整树带走进正式产品
- 机理链: ①两进程并发写同一 out_dir：后进入者在 stage_create 直接 rmrf 先进入者**在途** staging（其树被整删后 finalize 报 ERR_IO，半成品事务被第三方摧毁；§11 全有或全无的破坏形态从"留下半成品"翻为"删掉别人的在途树"）；②out_dir 父目录被预置同名 symlink→外部目录：自愈逻辑整删外部树（unlink+rmdir 全走 stat 判定，POSIX 分支 :251 fsync_tree 反而用了 lstat——同文件两套语义）；③Windows 大小写不敏感：`Hips`/`hips` 两个"不同"out_dir 共享同一 stage 目录名 → ①的跨目录放大；保留名（CON 等）在 _mkdir 面行为 = 需运行期，未判
- 权威依据: 宪章 §11:404、§8.3；同仓正对照：`runtime/module_loader/secure_loader.c` 已建立 realpath+「symlink escape 拒绝」纪律（:12/:437-448）——产品面未接线已由 M5b 在册，此处仅证明"知道怎么写对"
- related: M9-G-4/5（同族确定性命名与同提交点）、M9 §4 平台盲区总述、簇 6、W4-R2-01（同文件）
- 建议处置: rmrf 全链改 lstat/判 REPARSE，链接一律只 unlink 不进入；stage 名并入 pid+随机段；自愈删除仅在"本会话持有"（锁文件）前提下执行；promote 前对树内非常规项计数即拒

### W4-R2-03（P2）phase1_session p1sess 测试套件 TMPDIR/TEMP/TMP 三缺回退 `"."`，产物散落仓库根且被 `.gitignore`:56 `*.fts` 全局忽略 → AGENTS 目录规范违反 + dirty 门永久失明
- 类别: G_GOV_GATE（dirty 工作区产物落位）
- 工作区实存证据（时点 a3a343a4）: 根目录 `astrocs_p1sess_neg/ astrocs_p1sess_perf/ astrocs_p1sess_props/ astrocs_p1sess_test/` 4 目录（ls mtime 09-12 17:16；du 16K/168K/68K/96K；find 实见 light1.fts、master_bias.fts 等）
- 命令与输出（判缺失/判不可见三级复核）: ①`git --no-optional-locks status --porcelain astrocs_p1sess_neg astrocs_p1sess_perf astrocs_p1sess_props astrocs_p1sess_test` → 输出 0 行（rc=0，git status 完全不见）；②`git --no-optional-locks check-ignore -v astrocs_p1sess_neg/light1.fts astrocs_p1sess_props/master_bias.fts` → 均命中 `.gitignore:56:*.fts`；③`git --no-optional-locks ls-files astrocs_p1sess_*…` → 0 行（非跟踪）。同法对 `alloc_report.json/alloc_samples.csv` 命中 .gitignore:128/129（该两件的裸根名豁免已由 V9-b 在册，不重报）
- 锚: `lib/phase1_session/tests/p1sess/p1sess_tests_units.cpp`:136-142（`tmp_root = d?d:(w?w:(w2?w2:"."))`，d=TMPDIR/w=TEMP/w2=TMP）；perf:57 / negative:54 / properties:102 同型；`lib/phase1_session/tests/test_p1_session_manifest.cpp` 同名单复用
- 机理: 测试把 CWD 当临时目录回退根——在仓根跑测试即建根目录产物（违 AGENTS「任何新产物必须落位到对应目录，禁止散落根目录」「确需新增根目录条目必须先登记」）；且产物后缀 .fts 被全局 ignore ⇒ 该违规对 git/CI dirty 面**零可见性**（V9-b 讨论的"豁免名单化"至少还在 status 视野内，本形态连视野都没有）
- 权威依据: AGENTS.md 目录规范（强制）；宪章 §14.1/§15.1 工作区纪律
- related: M5b_L12_L17（CLI 主体同机理，不同主体=测试套件）、V9-b（dirty_ignore）、簇 1 机制②变体（不可见=不拦截）
- 建议处置: 回退根改 `run/`（该目录 gitignore 且为工作域规范落点）或强制 TMPDIR 缺失即 fail；现存 4 目录归位并登记去向（AGENTS 违规处理条款）

### W4-R2-04（P2）CLI「完成标记」与完整性锚的吞错三处：manifest 末块 flush 不判即 rename、sha256 失败丢弃 ok 向 schema 冻结字段注入空串、目录创建 ec 丢弃
- 类别: C_ALG_IMPL（次挂 B_STD_MISMATCH——违自订 schema）
- 位置: `cli/commands.cpp`::write_run_manifest
- 证据（三处）:
  - flush 不判即 rename: :452-453 `f << m.dump(2) << "\n"; if (!f.good()) return astrocs::IO;` —— `good()` 在 ofstream 析构 flush **之前**判；尾块停留流缓冲，最后的 flush 失败（ENOSPC/设备错误）零检查，:455 `fs::rename(tmp, final, ec)` 于是把**截断的 run manifest** 以正式名原子落位。run manifest = 该次 run 的完成标记（:407 ARCH-002 §5 单元自述），违 §11「失败不得留下可被误认成正式产品的半成品」。
  - sha256 吞错 ×4: :120/:138/:466/:579 均为 `bool ok=false; return file_sha256(x,&ok);` **丢弃 ok**；`cli/parser.cpp`:238-249 失败返回**空串**（:240 `return {};`，非 null）。而 `contracts/schemas/jsonl_event_v1.schema.json`（解释器解析）sha256 定义为 `{"type":["string","null"],"pattern":"^[0-9a-f]{64}$"}` —— 空串属 string 且不匹配 pattern（JSON Schema pattern 对 string 生效）⇒ 失败态 emit 的事件**违反自订冻结 schema**；`tests/cli/test_cli004_process_protocol.py:211` 仅在 golden 正常路径 assertRegex，失败路径零覆盖。另 :467/:122 size_bytes 失败出 null，与 :118-119 注释自述「文件 artifact 必带 sha256+size_bytes（目录 artifact 才允许 null）」相互矛盾（注释 vs 实现）。
  - 目录创建吞错: :443 与 :483 `create_directories(u8path(...), ec)` 的 ec 均不判；:486 第二次调用**覆写同一 ec 变量**，:487 `if (ec) return;` 只可能归因 gdir 失败——out_dir 失败被掩盖；且 :478 注释自述「图产物损坏只记 warning」而 :487/:493/:495 实为无事件静默 return（注释-实现不符）。
- 权威依据: 宪章 §11:404（不得吞错后继续生成看似成功的产品）、CLI_PROTOCOL_V1.md:37（artifact 词表）+ :44（"不得留下看似完整的"）、contracts/schemas/jsonl_event_v1.schema.json（sha256 pattern）
- 同仓正对照: `lib/phase3_session/p3_output.cpp` R10-C（哈希失败 = 整体失败 + unlink，:363-367 与 :546-547 两站点）、`lib/io/src/io_adapter.cpp`:60-69（close 后再判流状态）、`aio_publish` fclose/fsync 判
- related: M9-G-6（write_properties 同吞错族，不同主体）、M5b_L12_L17（同文件 CWD 面）、簇 1 机制④邻族（字段存在≠字段可信）
- 建议处置: ①写 tmp 后显式 `f.close(); if(f.fail())` 再 rename；②ok=false 时该 artifact 事件 sha256 出 null（schema 允许）且同事件 severity=warn，或整体按 IO 失败退出；③create_directories 失败给独立报错

### W4-R2-05（P2）发布包「清单与实际文件对账」工具验的是仓库内不存在的幽灵布局，且四套清单命名互斥——旧口径以现行对账工具形态存续
- 类别: G_GOV_GATE（主，§16.2 对账面）；次挂 B_STD_MISMATCH
- 位置: `tools/check_release_layout.py`（:14 `DIST=REPO/dist/astrocs-alpha`、:23 必含 `README.txt`+`checksums.sha256`、:31 无条件 `read_text`——必含项缺失时先崩溃于 traceback 而非报"missing"）、`tools/check_release_consistency.py`（:29 同幽灵根、:31 `checksums.sha256`、:39 `SBOM.json`；:30 `if dist.is_dir():` 守卫——幽灵根永不存在则②③两半整体静默跳过仍 exit 0，"采集 0 记 PASS"簇 1 机制⑤的前置件形态）；生成侧 `tools/make_linux_release.py`（:60 `tempfile.mkdtemp`、:146 `SHA256SUMS`、:136 `MANIFEST.json`、:113 `SBOM.spdx.json`）与 `make_windows_release.py` 同型；冻结文本 `docs/standards/RELEASE_STANDARD.md`:4（第三种名 `SHA256SUMS.txt`）
- 对账矩阵（实测）: 布局根 dist/astrocs-alpha —— 生成器**从不产出**（打包在 mkdtemp 内直出 tar/zip）；`ls -d dist` → 不存在（rc=2）；哈希清单名 checksums.sha256 / SHA256SUMS / SHA256SUMS.txt 三式并存；SBOM 名 SBOM.json vs SBOM.spdx.json 两式。宪章 §16.2 只规定"SHA256 清单"语义未锁名——三套名字无一与两两之间互证，任一对账工具跑真包必红（找不到的文件）或验错物
- 引用面三级复核: 全仓 grep `check_release_layout\|check_release_consistency\|gen_visual_views\|gen_audit_pack\|pack_audit_package\|make_linux_release`（排除 ./run ./问题扫描 ./设计大纲 ./工程控制 ./build __pycache__ .git 与自身）→ **6 个工具全部 0 外部引用**；ci/checks.json 零注册（"三面无门"总述已在册 M8a §12/R7——本条不重报"未注册"，只报"即便接线，判据对象与任何现行产物不一致"的失配新事实）。历史引用仅剩 `evidence/refactor/gates/G11/CHECKLIST.md:7` 一次"PASS"记档（其时点判据对象即幽灵布局，§12.3 意义上的"绿"不可复算）
- 权威依据: 宪章 §16.2（SHA256 清单为发布必备）、§12.3（机器一致性与交付物同源）、§14.1（原地演进，禁第三套功能实现体系——此处是第三套**清单词表**）
- related: M8a R7（在册"无门"事实，本条互补"验错对象"）、M5b L12-L17（product.json sha256 全 null 面）、簇 1 机制③（校验对象≠交付对象）、V14-b（白名单 units 字面量同族）
- 建议处置: 定一个权威清单名/布局根（建议对齐生成器现名 SHA256SUMS + MANIFEST.json），对账工具改读 tar/zip 实际成员并与 SHA256SUMS 三向核对（缺文件/多文件/哈希不符三红），并把 REL-001 类工具登记进 ci/checks.json 或删除防止被再引用为"PASS 证据"

### W4-R2-06（P2）发布包 build provenance 时间戳为硬编码常数、版本缺失时以字面量兜底——provenance 半真半假 + §16.1 第二版本输入
- 类别: C_DOC_CODE_GAP（清单/自述与实际不符）；次挂 G_GOV_GATE
- 位置: `tools/make_linux_release.py`:76（backends.manifest.json `"generated_at_utc": "2026-08-30T10:45:00Z"`）、:100（SBOM `creationInfo.created` 同常数）；`tools/make_windows_release.py`:90/:115（同型，`16:00:00Z`）
- 证据: 两工具每次构建产物内该两字段恒等于写死时间，与包内真实 `commit` 字段（:55 实时 rev-parse）自相矛盾——任何 a3a343a 之后打的包自称 2026-08-30 生成；SBOM/manifest 属 §16.2 必备"build provenance"面
- 版本面: linux :54 / win :66 `base = open(VERSION_FILE).read().strip() if os.path.isfile(VERSION_FILE) else "0.10.0-alpha.2"` —— 根 VERSION 缺失时静默用**硬编码版本**继续出包（§16.1"根 VERSION 是唯一输入"的违例；正确形态=缺失即拒绝打包），且该字面量本身是被废止的旧版本号，与 `check_release_consistency.py:19`「不比较硬编码版本」的现行口径相反（后者已按迁移史订正，前者仍留旧口径）
- 权威依据: 宪章 §16.1、§16.2、§11:402"禁止把计划值伪装成实际值"（时间戳同理）
- related: M8a-C-006（同文件许可字面量面，不同字段集）、M5b 版本簇（在册版本串正则面）、W4-R2-05
- 建议处置: 两时间字段改 `datetime.now(timezone.utc)`（或 SBOM 规范要求的 UTC 时钟）；VERSION 缺失 → 非零退出并拒绝出包

### W4-R2-07（P2）审核包生成器 gen_audit_pack.py 的清单与实际内容脱钩且自宣 PASS/全历史入库——一经使用即产出违 §16.3 的假清单（现为死工具）
- 类别: G_GOV_GATE（§16.3 审核包对账面）
- 位置: `tools/gen_audit_pack.py`
- 证据: ①manifest `verification` 字段硬编码 `"regression_tests": "352/352 PASS"、"tasks_done": "31/31"、"gates_passed": "G0-G8 (9/9)"、"verdict": "PASS"`（:129-134），无任何采集/校验来源（§11:402 禁计划值冒充实际；§16.2"不得用文档声明冒充完成"）；②contents 描述固定 `"关键契约与控制文档 (15 个)"/"28 份"`（:126-127），实际复制为 `if sp.exists()` 条件式（:82-88），缺失只 print 不记录 → 清单宣称数 ≠ 实装数 无对账；③:48-51 `git bundle create --all` 把**完整 git 历史**打包进审核包，直撞 §16.3 白名单（只含"当前源码基线…"）与"禁止包含 .git"条文语义；④:33-34 模块级 `OUT=REPO/"audit"...; OUT.mkdir(...)`——导入即在仓库根造 `audit/` 目录（AGENTS 固定顶层条目未登记该目录，且实跑 `run/rqs-fix/B1-memory/clean` 影子树显示历史上确以根 `audit/` 落位）；⑤:30 _deduce_root 回退 `parents[2]` 差一层（tools/ 上两级才是根，parents[1] 即根）——回退路径算错仓外
- 现状限定（诚实口径）: 全仓引用面三级复核 = 0 命中（见 W4-R2-05 命令）→ 当前不阻断任何流程；但文件在 tools/ 真源、名含 "audit pack" 且无 DEPRECATED 标记，§16.3 一旦按其重建审核包即违宪——按"把旧错误口径当权威改回去"专项登记
- 权威依据: 宪章 §16.3（白名单 + 禁 .git/历史集合）、§16.2、§11、AGENTS 目录规范
- related: W4-R2-05（同族幽灵对账）、V5-N-…（豁免证据指向 run/** 的形态对照）、簇 7（状态声明多头）
- 建议处置: 删除或移 `engineering/archive/` 加 DEPRECATED 头；若保留，verification 字段改为"由 --evidence <file> 注入且文件必须存在"，bundle 步骤移除，落点改 `artifacts/`

### W4-R2-08（P2）IO-001/astro_image_io/CLI 临时文件命名可预测 + 全仓零独占创建（O_EXCL/CREATE_ALWAYS/lstat 判链接 0 命中）+ overwrite=0 仅 begin 时点检查——TOCTOU/CWE-377/59 族
- 类别: C_ALG_IMPL（交付安全，§11 提交点族的第三缺口）
- 位置与机理:
  1. `runtime/io/fits_core.c`::acs_fio_writer_begin_v1 —— `if(!overwrite){FILE* probe=fopen(path,"rb");…}`（:1215-1222）**只在 begin 判一次**；提交 end 的 `rename(tmp,target)`（:1480）POSIX 下对 begin 之后新出现的 target **无条件替换** ⇒ "拒绝已存在"保护窗口 = 整个写出期之外（并发第二写者的完整产品在 end 时被本事务半成品顶掉或反之）；probe 自身也是 stat-then-use
  2. 同文件 ::fio_writer_make_tmp_name（:1095-1100）：`<target>.tmp.<pid>.<seq>`，seq 进程内自 0 起——同名**完全可预测**；随后 `fopen(tmp,"w+b")`（:1231）= O_CREAT|O_TRUNC **无 O_EXCL**：目录内预置该名的符号链接/他人文件被静默跟随并清空重写
  3. `lib/io/src/io_adapter.cpp`:42-44 同型（`ofstream(tmp, trunc)`，无独占）——使用者仅 `lib/orchestrator/cpp/checkpoint.cpp`，该目录不在构建图（口径引 V10-c「都不在构建图」），故不判生产可达
  4. `lib/astro_image_io/src/aio_pipeline.cpp`::aio_frame_save_cache :877：tmp = `path+".tmp"` **连 pid 都不带**——两进程并发保存同一 cache 路径 → 同 inode 混写；A 先 rename 走后 B 的已开 fp 仍写"改名后的 inode"= **正式路径**（B 的后续失败 unlink(tmp) 落空）→ §11 禁的"半截可误认产品"直接落在正式名上。现调用面仅 `lib/astro_image_io/tests/{pipeline_frame_contract_test.cpp:56,dataflow_fuzz.cpp:101}`（全仓 grep 复算）⇒ 未接线，P2 封顶
- 全仓独占创建三级复核（判缺失）: ①`grep -rnE "O_EXCL\|CREATE_NEW\|_O_EXCL\|CREATE_ALWAYS" lib runtime cli modules providers`（排除 third_party）→ **0 命中**；②放宽 `symlink/exclusiv` 大小写 → 命中仅 json/orchestrator third_party 噪声（L2 记录）；③定点 read 上述四文件确认写打开形态。lstat 全仓生产面仅 aio_publish:251 一处（fsync_tree）
- 条件升级判据: 任一站点接线生产写出（对齐 M9-G-4/5 的"接线即升 P0"条款，本条与其为同一提交点簇的第三缺口，建议同批修复同批升降）
- 权威依据: 宪章 §11:404、§8.3；同仓正对照 secure_loader symlink 拒绝（W4-R2-02 引）
- related: **M9-G-4**（同函数截断塌缩面）、**M9-G-5**（同函数耐久性面 + E4 待实测清单①"裸 CRT rename 对已存在目标失败还是替换"——本条第 1 点若判 Linux rename 语义属 POSIX 标准语义而非待实测项，Windows CRT 面仍按其口径未判）、M2b-C-01、簇 6
- 建议处置: tmp 用 mkstemp/CreateFileX(TEMP_EXCLUSIVE) 语义（Windows `CREATE_NEW` + 随机段）；overwrite=0 改 begin+end 双查 + end 用无替换提交（Windows `MoveFileEx` 不带 REPLACE_EXISTING / POSIX 用 `linkat+unlink` 或 renameat2 RENAME_NOREPLACE，后者不可用退 link）；失败 unlink 前先判 tmp 未被链接替换（fstat vs 自开 fd 同 inode）

## 负结果记录（轴覆盖证明，非缺陷）
1. **大小写/保留名（Windows 检出侧）**: `git --no-optional-locks ls-files -z` 5447 路径（解释器解析，时点 a3a343a4）→ 大小写折叠碰撞 **0** 组、`CON/PRN/AUX/NUL/COM[0-9]/LPT[0-9]` 保留名 **0**、尾点/尾空格段 **0**、最长 177 字符。checkout 面安全；生成名面的保留名行为 = 需运行期未判（见 W4-R2-02 ③）。
2. **长路径**: longPathAware/activeCodePage/GetLongPathName 真实源码 0 命中、`.manifest/.rc/.res` 0 文件、install-tree.contract.json 16 单元无 .exe.manifest —— 已由 M9-G-4【E4】定档"没声明"，不重报；本轴仅在其上补 W4-R2-01 的 1024 缓冲截断新事实。
3. **§11 fsync/rename 正对照面（登记为"该轴查过且合规"）**: `lib/phase3_session/p3_output.cpp`:330-360（fflush→fsync→close→rename→发布后哈希失败即 unlink）；`lib/backend_host/profile_store.cpp`:90-119（_commit/fsync + MoveFileEx REPLACE 兜底 + fsync_directory）；`lib/hips/src/module_entry.cpp`:857-1030（stage→fsync 树→promote→失败 discard 全链错误传播 + PUBLISH_REJECT 诊断码）。IO-001（fits_core.c）与 HiPS writer 的缺口即 M9-G-4/5/6、M2b-C-01 在册件，本轴复核其锚仍在。
4. **promote 目标占用**: POSIX rename 对非空 target 返 ENOTEMPTY→STATE、Win MoveFileEx 失败→STATE（aio_publish:364-374），TOCTOU 检查后竞态在该支为 fail-closed，不立条。
5. **cmd_config_init 静默覆盖已有 --output 文件**（commands.cpp:110 ofstream trunc）: CLI_PROTOCOL 未规定拒绝义务，设计意图不可考 → 需负责人口径，不立条（登记观察）。

## 需运行期，未判（汇总）
- W4-01 深径 ≥1024 下误删兄弟的实际发生（静态判据充分，复现未做）
- W4-02 Windows 保留名/大小写目录并存的运行期行为
- W4-04 ENOSPC 时序下 flush 失败的具体触发
- W4-05/06/07 现行发布是否仍走 make_*_release / 是否有历史包含假时间戳（禁执行令下未验证任何 zip 内容）
- W4-08 Windows CRT rename 对已存在 target 行为（该半条沿用 M9-G-5 E4 待实测清单①，未重复立案）

## 复算脚本
判读用只读 python3 已内联执行（大小写碰撞/保留名普查）；无独立脚本落盘。
