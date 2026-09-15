# W4（第二轮 · 文件系统与原子发布）片2-3 · P2 五条｜时点 HEAD a3a343a4｜W4 轴共 8 条

### W4-R2-04 CLI「完成标记」与完整性锚的吞错三处：manifest 末块 flush 不判即 rename、sha256 失败向冻结 schema 字段注入空串、目录创建 ec 丢弃
- 截断的 run manifest 被原子落位：`cli/commands.cpp::write_run_manifest:452-453` 写成 dump 后判 `if(!f.good())return IO;` ⇒ **good() 在析构 flush 之前判**，尾缓冲最后 flush 失败（ENOSPC）零检查，`:455` 的 rename 把截断件原子落位；而 run manifest 就是完成标记（`:407` ARCH-002 §5 自述）⇒ 违 §11「失败不得留下可被误认成正式产品的半成品」。
- 错误归因被覆写：`:443` create_directories 的 ec 不判；`write_run_graphs:483` 的 ec 被 `:486` 第二次调用覆写、`:487` 的 if(ec) 只能归因 gdir ⇒ out_dir 失败被掩盖；`:478` 注释自述图产物损坏只记 warning 而实现是无事件静默 return（注释与实现不符）。
- sha256 吞错四处：`:120`/`:138`/`:466`/`:579` 均为置 ok=false、调 file_sha256、**弃 ok**；`cli/parser.cpp:238-249` 失败返回空串——而 `contracts/schemas/jsonl_event_v1.schema.json` 把 sha256 定为 type [string,null] 加 64 位十六进制 pattern ⇒ **空串是违反自订冻结 schema 的 string**（pattern 对 string 生效，只有 null 合法）。测试 `tests/cli/test_cli004_process_protocol.py:211` 只在 golden 正常路径断言，失败路径零覆盖。
- 同仓正对照（判漏不判口径）：`p3_output.cpp` R10-C 哈希失败即整体失败并 unlink（`:361-365` 区）、`io_adapter.cpp:60-69` close 后判流、`aio_publish` 的 fclose 判 ⇒ 同族纪律未扩到 CLI。限定：触发需运行期复现未判，结构缺陷纯静态成立。
- **related** `M9-G-6`、`M5b_L12_L17`、`V15` 弱断言族、`V10-N-05`（同类死码计数器）、簇 1

### W4-R2-05 「把旧错误口径当权威」专项命中：发布包对账工具验的是仓库内不存在的幽灵布局，且四套清单命名互斥
- 对账侧：`tools/check_release_layout.py:14` 的 DIST 指向根下 dist/astrocs-alpha、`:23` 要求必含 README.txt 与 checksums.sha256、**`:31` 无条件 read_text**（文件缺失时先 traceback 崩溃，missing 分支永不到达）；`tools/check_release_consistency.py:26`/`:27`/`:35` 同幽灵根加 SBOM.json。
- 生成侧：`tools/make_linux_release.py` 与 `make_windows_release.py` 在 tempfile.mkdtemp 内直出 tar/zip，产物名是 SHA256SUMS、MANIFEST.json、SBOM.spdx.json ⇒ 从不产生 dist/，也不用 checksums.sha256/README.txt/SBOM.json 任何一名；冻结文本又是第三式：`docs/standards/RELEASE_STANDARD.md:4` 定审核包为 AstroCS_Review_*.zip 加 SHA256SUMS.txt。宪章 §16.2 只锁语义不锁名 ⇒ 三套哈希清单名与两种 SBOM 名两两互斥，任一对账工具跑真包**必红或验错物**。
- 实证与免重报：ls -d dist 不存在（rc=2）；六工具名全仓引用三级复核（排除 ./run、./问题扫描、./设计大纲、./工程控制、./build、__pycache__、.git 与自身）全部 0 外部引用；三面无门（未注册 CI）已在册（`M8a §12/R7`）⇒ **本条不重报未注册，只报判据对象与现行任何产物布局都不一致这一新事实**。唯一历史引用 `evidence/refactor/gates/G11/CHECKLIST.md:7` 的一次 PASS，其判据对象即该幽灵布局，不可复算。
- **related** `M8a R7`、`M5b L12-L17`（product.json sha256 全 null 面）、**簇 1 机制③**、`V14-b`、`C-19`

### W4-R2-06 发布包 build provenance 时间戳是硬编码常数，且 VERSION 缺失时以废止版本字面量兜底 ⇒ provenance 半真半假、§16.1 出现第二版本输入
- `tools/make_linux_release.py:76` 的 generated_at_utc 为常数 2026-08-30T10:45:00Z、`:100` SBOM creationInfo.created 同常数；`make_windows_release.py:90`/`:115` 同型（16:00:00Z）。**与同包内实时 rev-parse 的 commit 字段（`:55`/`:67`）自相矛盾** ⇒ a3a343a4 之后打的每个包都自称 2026-08-30 生成 ⇒ 触 §16.2 build provenance 必备项、§11 禁止把计划值伪装成实际值同型。
- 版本面：linux `:54` 与 win `:66` 为读 VERSION 文件、isfile 否则取 0.10.0-alpha.2 ⇒ **根 VERSION 缺失时静默用硬编码旧版本号继续出包**，违 §16.1 根 VERSION 是产品版本唯一输入（正确形态是缺失即拒绝）；该字面量同时是已迁移废止版本，与 `check_release_consistency.py:19` 现行订正口径（不比较硬编码版本，硬编码会造成单源迁移即恒 FAIL）方向相反 ⇒ 后者已订正、前者仍留旧口径。限定：历史包是否带假时间戳需运行期未判。
- **related** `M8a-C-006`、`M5b` 版本簇、`W4-R2-05`、`C-17`

### W4-R2-07 审核包生成器 `tools/gen_audit_pack.py`：清单与实际内容脱钩且自宣 PASS、含全量 git 历史 ⇒ 一经使用即产违 §16.3 的假清单（现为死工具，按旧错误口径存续登记）
- 自宣完成：`:129-134` 的 verification 字段硬编码 regression_tests 为 352/352 PASS、tasks_done 为 31/31、gates_passed 为 G0-G8 (9/9)、verdict 为 PASS——**无任何采集或校验来源**（§11 禁计划值冒充实际；§16.2 不得用文档声明冒充完成）。宣称数不等于实装数：`:126-127` 固定宣称 15 个与 28 份，而 `:82-88` 实拷是 if exists 条件式、缺失只 print 不进清单 ⇒ 无对账。
- 直撞 §16.3 白名单：`:48-51` 用 git bundle create --all 把完整 git 历史装进审核包，而 §16.3 明列只包含当前源码基线且禁止 .git 与历史控制包集合。
- 仓库根副作用与算错根：`:33-34` 模块级 OUT 指向 REPO 下 audit 并 mkdir ⇒ **导入即在仓库根造 audit/**（AGENTS 固定顶层条目未登记；影子树 run/rqs-fix/B1-memory/clean/ 与历史 docs 引用证明该形态曾实跑）；`:30` 的 _deduce_root 回退 parents[2] 差一层（tools/ 上两级才是根，parents[1] 才对）⇒ 回退路径算到仓外。
- 现状限定：引用面三级复核 0 命中（同 `W4-R2-05` 手法）⇒ 不阻断现行流程，P2 封顶；但文件**无 DEPRECATED 标记**，留在 tools/ 即待用隐患。**related** `W4-R2-05`、簇 7、`C-15`

### W4-R2-08 IO 与 CLI 临时文件命名可预测 + 全仓零独占创建 + overwrite 只在 begin 时点检查 ⇒ TOCTOU 与 CWE-377 与 CWE-59 族（§11 提交点第三缺口）
- 站点 1 保护窗口不含写出期：`runtime/io/fits_core.c::acs_fio_writer_begin_v1` 的 overwrite 分支是 fopen 探测、存在即拒（`:1215-1221`）**只在 begin 判一次**，而 end 的 rename(tmp, target)（`:1480`）POSIX 语义对 begin 之后新出现的 target **无条件替换** ⇒ 拒绝已存在形同虚设（两并发事务中后完成者顶掉先完成者的完整产品）。
- 站点 2 可预测名加无 O_EXCL：同文件 `:1095-1100` 的 tmp 为 target 加 .tmp. 加 pid 加 seq（seq 进程内自 0 起 ⇒ 全可预测），`:1231` 用 fopen(tmp, w+b) 即 O_CREAT|O_TRUNC **无 O_EXCL** ⇒ 目录内预置同名符号链接或他人文件被静默跟随清空。
- 站点 3 与 4：`lib/io/src/io_adapter.cpp:42-44` 同型（ofstream trunc 无独占），其使用者仅 `lib/orchestrator/cpp/checkpoint.cpp` 而 orchestrator 不在构建图（引 `V10-c` 在册口径）⇒ 不判生产可达；`lib/astro_image_io/src/aio_pipeline.cpp::aio_frame_save_cache:877` 的 tmp 是 target 加 .tmp，**连 pid 都不带** ⇒ 两进程同目标并发即同 inode 混写，且 A rename 走后 B 的已开 fp 仍写改名后的 inode 即正式路径，B 失败路径 unlink(tmp) 落空 ⇒ §11 禁的半截产品直接落在正式名上；调用面仅两个测试文件 ⇒ 未接线封顶 P2。
- 判缺失三级复核：全仓（排 third_party）检索独占创建四类写法 **0 命中**；放宽 symlink 与 exclusive 命中全在 vendored 与噪声；四文件定点读复核写打开形态。lstat 全生产面仅 `aio_publish:251` 一处；同仓正对照是 `secure_loader.c` 的 symlink escape 拒绝（`:12`/`:437-448`）。升降条件：任一站点接线生产写出 ⇒ 按 M9 接线即升 P0 同批处理。**related** `M9-G-4`、`M9-G-5`、`M2b-C-01`、`W4-R2-01`、`W4-R2-02`

## W4 轴负结果（覆盖证明，勿当缺陷）与未判清单
- 大小写与保留名（Windows checkout 面）全为零：git ls-files 的 **5447 条路径**解释器解析 ⇒ 大小写折叠碰撞 0 组、CON/PRN/AUX/NUL/COMn/LPTn 0、尾点尾空格段 0、最长路径 177 字符。长路径声明已在 `M9-G-4`【E4】定档 ⇒ 不重报。
- §11 fsync 与 rename 正对照登记在案（合规面查过）：`p3_output.cpp:330-360`、`profile_store.cpp:90-119`、`module_entry.cpp:857-1030`（含 PUBLISH_REJECT 传播）；promote 竞态本身 fail-closed（ENOTEMPTY 或 MoveFileEx 失败转 STATE）⇒ 不立条。
- 一条观察（不立条，需负责人口径）：`cmd_config_init` 静默覆盖已有 --output——协议未定拒绝义务、设计意图不可考。
- 需运行期未判五项：深径不小于 1024 的实删复现、Windows 保留名运行期行为、ENOSPC flush 时序、历史发布 zip 是否带假时间戳（禁执行令下未验）、CRT rename 对已存在 target（沿 `M9-G-5` E4①）。
