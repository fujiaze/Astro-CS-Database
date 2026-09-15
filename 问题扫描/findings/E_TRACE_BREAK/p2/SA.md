# SA（前台自身取证面之债）· P2 三条｜扫描时点 a3a343a4

### SA-N-01（P2）悬空锚主口径：**3135 出现 / 1008 种**，其中裸文件名 570 种 / 2278 出现（72.7%）⇒ 全部违反 `E4`「锚必须可解析」
- 我先前报的「421 个锚解析不到」**虚高且口径错**：SA 独立复算确认 421 是「裸文件名形锚 × 真源索引 basename 零命中」的**出现数**，其中 **149 出现 / 70 种是我抽取器正则缺尾界造成的伪命中**（`astrocs.p1.c` 截自 `astrocs.p1.calibration`、`result.c` 截自 `result.coverage_ok`），另 28 出现是成员访问噪声（`img.wcs.c`）⇒ **真无宿主 131 出现 / 71 种**。
- **禁行号锚（`C-18`）由此获得实测支撑**：悬空中带行号者 1466 出现；对宿主可给的 1370 出现做符号推算 ⇒ **行文本仅 27 处印证、1127 处低置信、216 处行号位无定义** ⇒ **即使路径修对，行号锚也不可信**。
- related `M6b-E-002`（行锚系统性失效同族不同事实）、`E4`、`C-18`、`E13`

### SA-N-02（P2）凭空符号 **2 处**（我写下的）：`ipv_wcs.cpp::build_fits_wcs_from_solution`、`aio_hips_writer.cpp::write_tile_core` —— HEAD 代码面与全历史代码提交**双零命中**
- `git log -S` 仅命中审计 docs 提交 ⇒ 其所在句（「防清理凭据是不存在的测试符号」型宣称）**事实成立，但锚必须改述为「零命中 + 检索口径」**，不得留悬空 `::符号`。另 5 出现属 SYMBOL_ELSEWHERE（符号在他处）⇒ **禁撤条、只换宿主**。
- 同批还有**改名链**与**已删除类**：`schemas/*.schema.json` 13 出现系两段改名（`64c1e988` 至 contracts/schemas_tmp/，`009ee419` 至今日路径，中间态已消失）⇒ 一跳映射会把锚写进已消失路径，须**链到 HEAD 实存宿主**；已删除 10 种 / 12 出现（各附删除 commit）指历史事实的**改述为「曾存在，commit X 删」**（`C-18` ③ 退役登记式），指现行状态的整句撤，**禁改锚到同名近似宿主**。
- related `C-18`、`V8-N-01`（凭据为不存在符号，同一机制我用在自己身上）

### SA-N-03（P2）三类结构性悬空：自指档案 94 种 / 272 出现、未入库影子树 13 种 / 34 出现、gitignored 运行区 22 种 / 26 出现
- **自指**：`_merge/M7.md` 30、`40_OWNER_DECISIONS.md` 14、`_cache/L23.md` 12 —— 补前缀后字面可解析（第一轮整目录排除所致），但**以我方档案充当 VERIFIED/证据宿主一律不合格**（`M6b-E-001` 同机制新面）⇒ 53 种 / 208 出现自动规范化，其余证据角色行**标待人工换真宿主**。
- **影子树**：`lib/snr_estimator/src/{module_entry.cpp,noise_model.cpp}` 等（HEAD 只有 `cpp/` 面，`ls-files` 对该目录计数 0 实证）、`lib/phase1/tests/p1phot/` 两件、根级 `resource_samples.csv`/`worker_balance.csv`/`alloc_*`（同名真身在 `evidence/v6_1_rework/tasks/MON-001/logs/`）⇒ **锚到未入库文件＝对 clean checkout 不可解析＝等同悬空**（与 `W1` 终报的「门注册与门脚本分裂于不同跟踪态」同根）。
- **gitignored 运行区**：`run/审计执行层/` 下 5 种取证脚本、`build/CMakeCache.txt` 等 ⇒ 违 `C-15`；**要把证据留在 findings 就必须把脚本回拷 `_verify/` 入库，否则锚面删**。
- related `C-15`、`C-18`、`W1-N-03`、`E4`、`E13`

