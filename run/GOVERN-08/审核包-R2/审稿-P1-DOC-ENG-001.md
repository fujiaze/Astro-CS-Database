# 审稿 P1 · DOC-ENG-001（第 1 遍）

> 基线：仓库 `/workspace/Astro CS Database`，HEAD = `f9650dd0aed97d7f261e5e6547f4fdb505bc313b`，工作树干净（`git status --porcelain` 空）。
> 片清单：`run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml:802-850`
> （层 `docs/engineering`，成员 41 份，目标行数 11000，实际行数 8213，超容量 false，划片依据「SRS-1 层内 LPT 均衡装箱（n=ceil(层行数/11000)，严格不跨层）」）
> 一遍口径：按 UNRESOLVED_REGISTER.md §74.2「一遍 = 对同一片材料的一次完整重读」。本片 41 份**逐份全文读完**，未抽样。
> 纪律：零 git 写、零编译、零 ctest/pytest/实验脚本、零仓内文件改动；证据一律 `文件:行` 或可复现命令；未读 `/tmp/acsd_g08/`。

---

## 1. 读完了吗

| 项 | 值 |
|---|---|
| 成员份数（权威版片清单） | **41** |
| 实读份数 | **41** |
| 成员总行数（片清单声明 / `wc -l` 实测，两者一致） | **8213** |
| 实际读了多少行 | **8213** |
| **覆盖率** | **41/41 份 = 100%；8213/8213 行 = 100%** |
| 未读完的部分 | **无** |

复算命令：

```bash
cd "/workspace/Astro CS Database" && git rev-parse HEAD
# 逐份行数（输出 TOTAL: 8213，与片清单 实际行数 8213 一致）
for f in $(sed -n '810,849p' "run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml" | sed 's/^ *- "//;s/"$//'); do wc -l "$f"; done | awk '{s+=$1} END{print "TOTAL:",s}'
```

**取证附读**（非本片成员，为验证本片结论所读）：`docs/ACSD_DESIGN.md` 目标节（109-132 / 195-204 / 243-277 / 441-532 / 545-554）、`lib/infrastructure/cli/protocol.h:30-75`、`lib/include/acsd/common_abi_v1.h:50-80`、`eng/contracts/schemas/jsonl_event_v1.schema.json`、`docs/engineering/VALIDATION_EVIDENCE_STANDARD.md:161/:436`、前轮审稿件（线索用）、`实验/TAUTOLOGY_REGISTER.md` §0-§1。

---

## 2. 本片判定

**判定：阻断（BLOCKING）**。41 份工程正本里，**承载判据与冻结结论的关键引用面大面积悬空或自相矛盾**：两份正本对「现行性能基线」给出互斥结论且都无仓内归档；冻结结论所依据的 `ACCEPTANCE_GATES.md` 在仓内不存在；`PIPELINE_BLOCK_CONTRACT.md` 的判据编号在同一文件内两表含义互斥；`RESOURCE_MONITORING_CONTRACT.md` 宣称的「不可手工合成」由其自己列出的机制（公开 salt + 无密钥的 sha256 链）结构上无法交付，且其**已公布的复算配方本身与代码不符**。

**计数（本片口径见 §9）**：**阻断 12 条 / 须修 39 条 / 建议 15 条**。其中**相对前三轮已记内容的新问题约 35 条（阻断 9–10 条）**—— 余下条目的缺陷本体已在 `UNRESOLVED_REGISTER.md` 登记，本遍指出的是「标准侧缺状态标记」或「本切片内的精确计数」，已在各条「注」中标明，不计为新问题。

最重的 3 条：

1. **【阻断】两份工程正本对「现行性能基线」给出互斥结论，且两者的归档都不在版本控制内。**
   `docs/engineering/SCIENCE_FREEZE.md:46` 声明 `PERFORMANCE_BASELINE = FINAL`（真实 16 帧 Phase1 cold median 145.4s / warm 142.4s、Phase2 24.0-25.1s、Browser pan p50 34.7ms）；
   `docs/engineering/BASELINE.md:14` 与 `:35` 声明本层全部读数是「历史读数、原始 JSON 未入库」「**不构成现行基线结论**」，并给出**另一组**数字（Phase1 ~67.35 s/frame、Drizzle ~64 s/frame、RSS 37.5 GB→1.2 GB）。
   两者相差 ~2.2×，都无仓内归档可核（`.gitignore:17` 是 `run/*`，`git ls-files run/ci` = 0 条）。
   ⇒ 「发布前全量、性能门、无 >5% 回退」这些 MUST 级判据**没有唯一可核的基线**。
   叠加：`SCIENCE_FREEZE.md:5` 用「`ACCEPTANCE_GATES.md` G1-G10 全部 PASS」作为整个冻结结论的前提，而 `ls docs/ACCEPTANCE_GATES.md` → **不存在**（全仓零命中）。**冻结状态所依赖的两个证据面同时不可达。**

2. **【阻断】`PIPELINE_BLOCK_CONTRACT.md` 的判据编号在同一文件内两表含义互斥，且三处「编号区间」声明互不相符。**
   - `:79` 导语称判据为「**C1–C6**」，其下表实际含 **C1、C2、C3、C3b、C4、C5、C6** 七行 —— `C3b` 落在区间外。
   - `:97` 导语称「**C4–C7**」，其下表实际含 **C4、C5、C5b、C6、C8、C7** —— `C8` 落在区间外，且行序 C8 在 C7 之前。
   - **同一标识在两表含义不同**：`:88` C4 = 方向一致 / `:101` C4 = 无幻边；`:89` C5 = 载体合同 / `:102` C5 = 序为拓扑序；`:90` C6 = 非退化 / `:104` C6 = psf 在 wcs 之后。
   - UNRESOLVED_REGISTER.md:1409 又以「**C1–C8**」统称这三处。
   ⇒ 一份以「机器可判据」为存在理由的合同，其判据 ID 不可解析。任何报告、豁免台账、门禁注册面引用「C4」都指向两个不同判据。

3. **【阻断】`RESOURCE_MONITORING_CONTRACT.md` 宣称的「原始 CSV 不可手工合成」由它自己列出的机制结构上无法交付 —— 属本项目已固化的「门看起来在，实际不在」类。**
   `:15` 与 `:20` 称「原始 CSV **不可手工合成**（header 指纹链 + 写后只读 + 时间戳单调断言）」；
   `:99-105` 给出的机制 = `sha256(公开常量 salt ‖ 前一行指纹 ‖ 行字符串 ‖ 行号)`，且 salt 在 `:105` **逐字公开**；
   实现 `lib/infrastructure/observability/monitoring/monitor.py:94`(`_fingerprint`)、`:112`(`_seed_fingerprint`) 为纯 canonical-JSON sha256，
   `git grep -c "hmac|secret|signature|private_key" -- .../monitor.py` → **0**。
   `:118-127` 的 `verify_csv` 七项检查全部只验**内部自洽**（header 逐字、seed 指纹自洽、链式自洽、seq 连续、run_id 一致、ts 单调、phase 词表）。
   ⇒ 任何脚本按公开公式生成的 CSV 都能 100% 通过该校验。**该校验证明「文件自洽」，不证明「数据来自真实采集」**，而 §9 B1–B7 七条验收全部建立在这个被高估的能力上。
   同一形态的次级项：`:126` 的 `t_iso_utc` 单调检查带 `--no-ts-monotonic` 开关可被调用方关闭。

   **并且该机制的公开配方本身已经写错**（本遍由 SA4 报出、我独立复核成立）：
   - `:105` 公布 `salt = b"acsd-monitor-timeseries-v1"`，实现 `monitor.py:91` 是 `_FP_SALT = b"acsd-monitor-v1"`（**无 `-timeseries`**）；
   - `:100` 公布 `seed_fp = sha256(salt | "seed" | **sorted(HEADER)** | run_id)`，实现 `monitor.py:114-115` 是 `json.dumps(HEADER, ..., sort_keys=True, ...)`，而 `sort_keys` 对 **list 是 no-op** ⇒ 实际是 `json(HEADER)` 原序，不是 `sorted(HEADER)`。
   ⇒ **审计方照合同复算必然全链失配**，而 §4.1 的全部卖点正是「篡改/追加/删行 → 链式断裂可检出」。这使 B3 从「能力高估」升级为「能力高估 + 已发布配方失效」双重失效。

---

## 3. 逐文件清单

41 份全部读完。下表逐份给「读了什么 → 看到什么 → 判定」，每条带 `文件:行`。

| # | 文件 | 行 | 读了什么 | 看到什么（带 `文件:行`） | 判定 |
|---|------|---:|---|---|---|
| 1 | `docs/engineering/UNRESOLVED_REGISTER.md` | 4178 | 全部 81 节逐行读完 | `:18` 标题「方向裁决（18 条）」vs `:45` 自核「§2 共 22 条」vs 实测 22 行 → 标题计数陈旧；`:105`(§7 SCI-P2-1 状态=阻塞待裁) vs `:380`(§16 SCI-P2-1 状态=已裁决) 同编号两处状态互斥；`:193` 仍把 SCI-P2-1 当未决；`:6-12` 声明「按处置性质分三层」，实际有 §5–§81 共 81 节且无索引；`:5` 冻结前提指向不存在的 `ACCEPTANCE_GATES.md`；`:46` 性能基线与 BASELINE.md 互斥；`:419-421` 处置纪律用「假绿」定性而非归类 | **阻断**（自相矛盾 + 冻结前提悬空） |
| 2 | `docs/engineering/data/DATA-002_PHASE_PRODUCT_EXCHANGE.md` | 329 | §2a `invalid_handling` 块、§4a/§31.1a 引用链 | `:2a` 块是 NUMERIC_STANDARD:80 指定的 `invalid_handling` 唯一正本，存在；`:146` 归一分母写 `D_p²`（UNRESOLVED_REGISTER:1111 已登记为 SCI-DRZ-V1 第 4 处，science 层未裁定前不得改） | 需修（已登记，本遍复核成立） |
| 3 | `docs/engineering/HIPS_STORAGE_FORM_CONTRACT.md` | 274 | 冻结面 §2-§10 全部规则表与不变式 | `:12` 称冻结「四件事」但列了 **6** 条（编号 1-6）→ 计数不一致；`:108` 登记面 `eng/contracts/ledgers/dead_config_keys.json` 与 `docs/KNOWN_LIMITATIONS.md` 均为**零消费者/不存在**目标（后者不存在，见 §4-B9）；`:177-180` 检查器载体全部指向「门禁注册面（G08-10 重建）」 | 需修（`:12` 计数 + `:108` 死指针） |
| 4 | `docs/engineering/io/IO_003_ATOMIC_OUTPUT_PUBLISH.md` | 274 | §1-§9 发布流水线与错误语义 | `:258` 验收表引「**§4.7**」、`:256` 引「**§4.2**」—— 全文标题只有 §4 与 §4.1（`grep -nE "^#{1,4} "` 已列全），两处**章内引用指向不存在的节**；`:148` 写「verify（**CHECKCODE** / CHECKDATASUM）」，`CHECKCODE` 非 FITS 关键字（标准为 `CHECKSUM`/`DATASUM`），与本文件 `:272-273` 自身口径矛盾；`:53-56` 四条路径均存在（正面） | **须修**（坏引用 + 伪关键字） |
| 5 | `docs/engineering/CONFIG_CONTRACT.md` | 230 | §0-§12 三类配置、defaults 分组、phase_config、滤镜库、cpu_profile | `:44-55` 分组字段数 1+1+2+14+18+6+1+1+1+1+1+3 = **50**，而 `:32` 声明「权威 = defaults.json 的 `field_count`」；实测 `defaults.json` `field_count=59` / `len(fields)=59` → **两处口径差 9**；`:114` 内引「**§9.68** 后取代逐帧 `inputs[].filter`」—— 本文件无 §9.68（§9 是旋钮登记册）；`:45/:48/:51/:54/:67` 五处指向的 science 正本均存在（正面）；`:126` 登记 `Astronomik UV-IR Block L-2` 转录偏差而 `:163` 门表仍写「滤镜库 45/45 逐字一致」 | 需修（计数差 + 坏引用 + 门表与登记不一致） |
| 6 | `docs/engineering/SCIENTIFIC_REFERENCES.md` | 184 | A-N 全 14 节 | **编号 61/62/63/64/65/66/67 各被两条不同文献占用**（`:92-94` vs `:164-166`；`:107-108` vs `:167-168`；`:123-124` vs `:169-170`），而本档案被 CONFIG_SCHEMA:91 / SCI 正本以「官方式[18]/[19]」式编号引用 ⇒ **编号不唯一，引用不可解析**；`:85` 自述「项目现仅登记官方 RCR 2.4.7 软件参考，缺该论文引用」而下一行 `:86` 已补 arXiv:2301.07838；`:44-47` §F 四条仓内来源（`设计大纲/…`、`artifacts/evidence/review-package/`、`run/perf-fix/P5-snr/`）待核 | **须修**（编号唯一性） |
| 7 | `docs/engineering/observability/RESOURCE_MONITORING_CONTRACT.md` | 182 | §3.0 双工件消歧、§4 指纹链、§5 verify_csv、§9 验收 | `:15/:20` 「不可手工合成」不可交付（见 §2-3）；`:46-52` 双工件消歧**质量高、已核实正面**（21 列 header 与 `resource_timeseries.csv` 20 列声明分立）；`:126` `--no-ts-monotonic` 可关；`:156` 缺口登记面 `docs/KNOWN_LIMITATIONS.md` 不存在 | **阻断**（能力高估） |
| 8 | `docs/engineering/LOG_AND_ERROR_CONTRACT.md` | 176 | §2-§9 错误模型与判据 R1-R5 | `:98-107` 域→码映射 8 行用 7 个码（2/2/4/5/7/10/9/70），与 `protocol.h:31-35` 的 11 条冻结退出码域一致（正面）；`:74` 声明缺日志工件 ⇒ exit 8（`INTEGRITY`），但 §5 表**无任何 ErrorDomain 映射到 8** ⇒ 8 的产生路径在本合同内无定义；`:36` 「`event`/`seq` 与运行事件流 `kind`/`sequence` 各自独立」与 API-001:63 完全一致（正面）；`:165-173` 判据 R1-R5 载体全部为「门禁注册面（G08-10 重建）」 | 建议（exit 8 无归属域） |
| 9 | `docs/engineering/observability/STRUCTURED_LOGGING_CONTRACT.md` | 169 | §1.1 消歧表、§2.2 字段表、§6 校验项、§8 验收 A1-A7 | `:31` 消歧表称运行事件流 `kind` 枚举 = **5** 项（progress/resource/artifact/backend/final），而 `protocol.h:41-55` 与 `jsonl_event_v1.schema.json` 的 `kind.enum` 均为 **10** 项（多 `stage_start`/`stage_end`/`graph`/`resource_gate`/`v6_mode_route`），API-001:62 取 **7** 项 ⇒ **5/7/10 三方互斥**，且本表标题是「**消歧（唯一口径）**」；`:9/:126` 校验项无载体；`:152-162` 验收 A1-A7 的「证据」列全部写「schema required + 测试」而**不指名任何测试文件** | **阻断**（自称唯一口径却与实现正本差 5 项） |
| 10 | `docs/engineering/observability/RUN_GRAPH_CONTRACT.md` | 164 | §5 JSON 合同、§6 verify、§7 字段语义 | `:16-17` 与 `:107-120` verify 五项：`:369`/`:756` 两处聚合均调**同文件** `aggregate_trace`（`:140`），`:753` 虽尝试 import `trace_replay.replay_from_jsonl` 但 verify 主路径走 `:756` 的 `aggregate_trace` ⇒ **图与「外部参照」同源于同一实现**，属往返自证型（型 3）：聚合器自身的缺陷两侧同时出现、门不红。`:148` 却称「replay **双实现**」 | 须修（自证型，§5 反例） |
| 11 | `docs/engineering/TEST_STANDARD.md` | 159 | §1-§8 覆盖/确定性/覆盖率/回归/入口/变更/执行范围 | `:150`「（**§6 的 Q3** 可归因）」—— 本文件 §6 是「测试入口」（4 行表格，无 Q3）；Q3 实际定义在 `VALIDATION_EVIDENCE_STANDARD.md:161`，属另一文件；`:56` 测试入口表列 `eng/tools/docs_machine_consistency.py`，`git ls-files | grep docs_machine` → **0**（死指针）；`:66-159` §8 全节（三范围/改动集/glob/构建图反查/四档/指纹缓存/结果面）**未指名任何载体**，且 `:116-117` 承诺的三条注入负例在仓内找不到；`:24` 引 `execution_options_contract.md §5` 存在（正面） | 须修（坏引用 + 死指针 + 无载体规范） |
| 12 | `docs/engineering/CONFIG_SCHEMA.md` | 156 | 抬头口径声明、Stage2 config 段、排异档位、生产调用链表 | `:9-19` 与 `CONFIG_CONTRACT.md §3` 的双向消歧**质量高、已核实正面**；`:102-104` 生产路由三档与 `SCIENCE_FREEZE.md:19` 一致（正面）；`:71` `>15 → linear_fit` **自带诚实括注**（「该档 WBPP 2.4.0+ 为 ESD，本仓该档取 linear_fit = WBPP ≤2.3.x 旧表」），与 `SCIENTIFIC_REFERENCES.md:91` 不冲突 ⇒ **此项为我在盲复算中提出、后被原文推翻的假设，记为已否决**；`:125/:128` 「OpenMP 16」与 `CONCURRENCY_STANDARD.md:21`「16 不作硬编码值」构成口径冲突（后者为 SHOULD 级） | 建议（16 的口径冲突） |
| 13 | `docs/engineering/NUMERIC_STANDARD.md` | 132 | 标度词表、线性/面亮度标度律、NaN 契约、权重节 | `:15` 与 `:3` 逐字引「每个科学量写清**五件事：**单位、坐标系、归一化、精度要求**、**有效有限域」—— 原文 `docs/ACSD_DESIGN.md:202` 是「每个科学量写清单位、坐标系、归一化、精度要求**与**有效有限域」，**无「五件事：」、是「与」不是「、」**（两处均带引号 ⇒ 伪引）；`:120-122` 三处上游 §-引错（§2.1 是「P1 通量积分拟合」，不含 HiPS 存什么/权重公式；真出处为 §2.2:126/:130/:132）；`:59` 数值复算通过（nside=2^18 ⇒ A_cell=1.5239e-11 sr、1/A_cell²=4.306e21、21.63 dex ✓）；`:45-49` 引 `run/RELEASE-05/vis/out/m42_p1_t3/` 为空目录（死指针，另见 §4-M4） | **须修**（伪引 + 三处 §-引错） |
| 14 | `docs/engineering/cpu/CPU_003_AVX2_PROVIDER.md` | 128 | §1-§10 注册热点、加载门、容差冻结 | `:93-95` §7 的**传递性论证不成立**：`|avx2−baseline|≤ε` 且 `|baseline−f64|≤ε` 推不出 `|avx2−f64|≤ε`（上界 2ε）；`:110-119` §9 四组测试**无任何独立 f64 参考量**（`provider_avx2_oracle_main.cpp` 只 dlopen 两个 DSO），该前提在本判据面之外；且 2e-4 = **3.36×10³ × f32 eps(5.96e-8)** ⇒ 单元素判别力极弱；`:88/:98`「共享源 `baseline_kernels_impl.inc`」**为假**（`cpu/avx2/src/avx2_kernels.cpp:25-30` 只 include `avx2_provider_v1.h`/`cpuprov_kernels_v1.h` + 三个标准头，其自带注释 `:33-42` 自述「写为标量源码…自 avx2_provider.cpp 逐字符搬移」；真正 include 该 .inc 的是**另一个** provider `backend_host/avx2_backend_kernels.cpp:41`）⇒ 若前提为真，`:118` 的 ≤2e-4 就是**结构对称型恒真门**；`:13-14` 实现面清单漏列唯一带 `-mavx2 -mfma` 的计算面 TU `src/avx2_kernels.cpp` | **阻断**（假前提 + 传递性跳步 + 门无独立参照） |
| 15 | `docs/engineering/DATA_ARTIFACTS.md` | 124 | §1 DataArtifact 表、§1.2 统一对象、§2 歧义映射、§3 机器校验 | `:43` 称 UNIFIED_MODEL §2 的 **13** 个对象，表内实测恰 13 行（正面）；`:60` `DATA-OBJ-PROVENANCE-001` 的 `scalar` 写 `int`，而同组其余为 `f32|f64|int` / `f32|f64` ⇒ provenance 是 JSON 容器文档，`int` 系从 VALIDITY 行复制；`:60` 「可否作权重：——」为未填占位；`:91-96` 「端口词汇面偏差」把 `module_adapters.cpp` 与 `lib/infrastructure/scheduler/src/module_adapters.cpp` 当两个来源列，但二者是**同一文件的短路径与全路径** ⇒ 登记的两派其实是同一处 | 建议（三处登记瑕疵） |
| 16 | `docs/engineering/abi/ABI_003_SECURE_LOADER.md` | 111 | §1-§7 信任边界、错误模型、测试清单 | `:72` 的 `ACS_ERR_INTERNAL` **确实存在**（`lib/include/acsd/common_abi_v1.h:66`、`status_codes.h:165`）⇒ 我原假设「该码不存在」**被原文推翻，记为已否决**；但本文件 `:72` 依赖的 `COMMON_ABI_V1.md:38-42` 枚举表**漏列它** ⇒ 依赖方正确、被依赖方不完整；`:89-101` 的 detail 2/3/4/5/6/8/9/10/11/12/14/15/16/17 与 `:65-72` 的 detail 2-18 分组逐条自洽（正面）；`:103` `python3 eng/tests/abi/test_secure_loader.py` 存在（正面） | 建议（依赖方枚举不完整） |
| 17 | `docs/engineering/PIPELINE_BLOCK_CONTRACT.md` | 110 | §1-§7 与 §7.1 全部判据表 | 见 §2-2：`:79`(C1–C6) / `:88-90` 与 `:101-104` 编号含义互斥 / `:97`(C4–C7) 但表含 C8 / `:97` 导语写「C4–C7」而 `UNRESOLVED_REGISTER.md:1409` 写「C1–C8」；`:66-75` §6「负例（必须能红）」**质量高**（含「恒真比较无证据资格」与「空注册表判红」两条非退化要求）；`:75` 与 `:90/:106` 两处非退化下界已正确排除恒真 | **阻断**（判据 ID 不可解析） |
| 18 | `docs/engineering/API-001.md` | 101 | §1-§7 分层、错误模型、机器输出、变更纪律 | `:62` 事件枚举列 **7** 项，与 `protocol.h:41-55`/schema 的 **10** 项差 `graph`/`resource_gate`/`v6_mode_route`；`:26` 「`ErrorDomain` → 退出码映射 = LOG_AND_ERROR_CONTRACT §5」**正确**（正面）；`:82-83` 列出的 `API-AIO-001`/`API-P2-REJECT-001` 在其 `:86` 自己指名的正本 `PUBLIC_API.md` 中**零命中**（仅 `:84` 的 `API-P2-UPM-001` 命中 `PUBLIC_API.md:1615`）⇒ 「已登记的 ID 举例」三条中两条未登记；`:63` 与 STRUCTURED_LOGGING:36 的「各归各流」声明一致（正面） | **须修**（枚举 + 未登记 ID） |
| 19 | `docs/engineering/COMMON_ABI_V1.md` | 99 | §1-§5 命名、类型、并发模板、头独立性、落点映射 | `:15` 「前缀 `acs_`（函数）/ `ACS_`（类型/常量）」与 `:24-74` **全部类型使用 `acsd_*`**、实测 `lib/include/acsd/common_abi_v1.h` 中 `acs_`（非 `acsd_`）**零命中** ⇒ **§1 命名规则被本文件 §2 自身违反**（UNRESOLVED_REGISTER:1890 记载「`COMMON_ABI_V1.md` 须同步改写」，改名只改了类型侧）；`:38-42` 枚举列 10 项，头文件 `:54-66` 有 **11** 项（多 `ACS_ERR_INTERNAL = 70`）⇒ 被 `ABI_003:72` 依赖的码在本文件缺载；`:88` 「单头以 `gcc -x c -std=c11` 独立编译通过」的判据实现是 `eng/tests/api/test_common_abi.py:8-15` 内的**硬编码 7 行 stub**，**从不读真头** ⇒ 该门结构上不可能因真头的 C++ 专有构造翻红（判据读不到真实对象）；且 `:6` 载体的 `docs/api/COMMON_ABI_V1.md` 路径不存在（`test_common_abi.py:6`） | **阻断**（命名规则自违 + 枚举缺载 + 判据读桩） |
| 20 | `docs/engineering/CODE_STANDARD.md` | 86 | MUST/SHOULD/MAY、告警口径、命名保留面 | `:81` 门 `CHK-NAMING-SURFACE` 的扫描集写作「三族字面量（混写 `ACSD`、小写别名 `acsd`、**全大写命名空间 `ACSD`**）」—— 第 1 族与第 3 族**逐字相同** ⇒ 三族退化，按此枚举无法区分大小写混写；`:60` 判定规则（`AstroCS`→`ACSD`）经 `UNRESOLVED_REGISTER.md:2356` 复核**已复原正确**（正面）；`:64-75` 十类保留面枚举具体且可判定（正面）；`:46` 根 `CMakeLists.txt` 的 MSVC 分支定义两个咨询宏（正面） | 须修（门枚举退化） |
| 21 | `docs/engineering/PROJECT_SPEC.md` | 86 | §1-§11 权威体系、使命、三阶段、数据对象、科学门 | `:15` `docs/science/UNIFIED_SCIENCE_MODEL.md` **存在**（正面）；`:53` 「实验单元二已对拍」是**已发生事实的断言**而非现行设计，与本文件 `:5`「不描述某次工程修复流水账」及 `AGENTS.md §5`（正文无历史叙事）冲突；`:86` §11「当前迁移原则」整节是**任务流水叙事**（「本轮优先完成三阶段设计和统一科学合同，再做合同/代码差距审计，最后实施」）⇒ 与 `:5` 自订定位直接矛盾 | 须修（历史流水叙事入正本） |
| 22 | `docs/engineering/SCIENCE_FREEZE.md` | 80 | 全文 | `:5` 冻结前提 `ACCEPTANCE_GATES.md` G1-G10 **不存在**；`:46-49` `PERFORMANCE_BASELINE = FINAL` 与 `BASELINE.md:14/:35` 互斥；`:17` 含日期「M3 裁决 **2026-09-25**」、全文含 V14/V15/V16/V17/V18R2 版本号 ⇒ 违 `AGENTS.md §5`（正文无日期、版本号、任务流水编号）；`:20-25` WBPP 档界**自带诚实括注**且与 `SCIENTIFIC_REFERENCES.md:91` 不冲突 ⇒ **此项为我提出后被原文推翻的假设，记为已否决** | **阻断**（前提悬空 + 基线互斥） |
| 23 | `docs/engineering/ASYNC_IO_CONTRACT.md` | 79 | §1-§10 模型、所有权、取消、测试、生产接入 | `:68` 「`BoundedAsyncQueue` 当前**无生产调用点**（唯一使用面 = `lib/algorithms/coverage/tests/async_io_test.cpp`）」—— `git grep BoundedAsyncQueue -- lib eng` 实测 5 类命中（头定义、`async_io.cpp:2` 注释、`async_io_test.cpp`、`eng/tests/cli/test_parallel_queue.py`），**生产调用点确为 0** ⇒ **该自述诚实，成立**；但 `:70-79` §10「**运行时判据**」= `eng/tests/cli/test_parallel_queue.py`，它只实例化头内模板、测的是与生产并行的另一条路径（`:66` 自认「计算并行是现行生产形态…**不经过本合同的队列**」）⇒ **判据读不到真实对象**；`:68` 把「未接线」状态指向 `docs/KNOWN_LIMITATIONS.md`，该文件**不存在** ⇒ 诚实披露的落点丢失 | 须修（判据面 + 披露落点丢失） |
| 24 | `docs/engineering/SCHEDULER_CONTRACT.md` | 70 | §1-§6 总则、三阶段形态、确定性、探针 schema | `:3` 上游写「§9（探针驱动优化）」，而 `docs/ACSD_DESIGN.md:530` §9 标题是「**CPU 后端与资源**」⇒ 条款号伪引；`:58` 「本 schema 与 `jsonl_event_v1.schema.json` 兼容——`ts`/`kind`/`value`/`unit` **复用其字段名**」，实测该 schema 的 `required` = `[schema_version, event_id, run_id, **timestamp_utc**, sequence, kind, severity, phase, stage, message]`，properties 含 `unit` 与 `timestamp_utc`，**无 `ts`、无 `value`、无 `tags`** ⇒ 4 个复用名中 3 个不存在，而 `:46-56` 又把这三字段声明为「字段**冻结**」；`:5` 机器 schema `eng/contracts/schemas/scheduler_probe_event.schema.json` **存在**（正面）；`:62-63` 退出码 9/10 与 `LOG_AND_ERROR_CONTRACT.md:106/:105` 一致（正面） | 须修（伪引 + 3 个冻结字段不存在） |
| 25 | `docs/engineering/02_PIPELINE.md` | 62 | §1-§7 触发/job/超时/门禁/产物 | **全文描述的 CI 工作流在仓内不存在**：`ls -d .github` → 无，`git ls-files .github` → **0 条**（`UNRESOLVED_REGISTER.md:2994` 记载 `.github/workflows/ci-linux.yml`(188 行)/`ci-windows.yml`(155 行) 已随 `e5f589a6` 删除）⇒ `:14` cron `17 19 * * *`、`:18` 「`on:` 面只含 push/workflow_dispatch/schedule」、`:38` `timeout-minutes: 330`、`:29-30` artifact 名与 retention 全部**无仓内载体可核**；`:16` 门禁名 **`nwoker`** —— `git grep nwoker -- docs/` → 0 命中，且同句其余门名（sanitizer/coverage/invariant）均为真词 ⇒ 笔误应为 `worker`；本文件**未标注**「载体待 G08-10 重建」，与 TEST_STANDARD/CPU_BACKEND_ARCH 的处理方式不一致 | **阻断**（整篇对象不存在且无缺口标注） |
| 26 | `docs/engineering/PHASE3_MODULE_ARCH.md` | 62 | §1-§5 模块、数据结构、并发、错误、追溯 | `:38/:45/:61` 三处引用 **`docs/engineering/THREAD_BUDGET_ARCH.md`** —— `ls` → **不存在**（不在本片，也不在 DOC-ENG-002 的 43 份清单内）⇒ 三处悬空；`:41` 冻结内存上界称「**与输出总图大小 `W_out·H_out` 无关**」，而 `:42` 紧接给出 `max_tiles` 默认 `min(1024, ceil(W_out·H_out/W²)+16)` ⇒ **默认配置下 `max_tiles` 正是 `W_out·H_out` 的增函数**（未触及 1024 上限时），同一文件两行自破其「冻结」不变式；`:22-27` 实现锚四份文件均存在（正面）；`:5` 「不把科学决策藏进 cache/loader」与 `:33` 的 cache 职责边界一致（正面） | **阻断**（冻结不变式被自身下一行推翻）+ 须修（3 处悬空） |
| 27 | `docs/engineering/RT-001.md` | 56 | §1-§4 冻结接口、边界规则、验收 | `:3` 上游写「§8.1（顶层结构）」，实测 §8.1(:441) 标题是「**总原则：唯一入口、阶段独立调度器**」、§8.4(:483) 才是「顶层结构」，且 §8.4 的下级索引**逐字列出 `docs/engineering/RT-001.md`** ⇒ 上游指错；`:12` 声明的五个 Runtime 方法（`load_pipeline`/`run`/`cancel`/`inspect`/`node_statuses`）与 `create_runtime` 在 `lib/include/acsd/core/runtime.h` 均存在，`:54` 的 `RT-001_ABI_PASS` 在 `eng/tests/unit/rt001_abi_test.cpp:110` 逐字存在（正面）；`:50` 指名 `eng/tests/unit/rt001_abi_test.cpp` **存在**（正面） | 建议（上游条款号） |
| 28 | `docs/engineering/CPU_BACKEND_ARCH.md` | 51 | §1-§8 变体策略、编译隔离、六查、C ABI、kernel 粒度、失败回退、发布 | `:28` 「**七类：**」后实际列 **8** 项（calibration pixel transform / noise-SNR reductions / WCS-PSF 批量 / drizzle overlap-accumulate-normalize / UPM SpMV-residual-weight / rejection statistics / integration accumulate / HiPS bulk transform-encode）⇒ 计数与清单自相矛盾；`:15` 六查、`:22` 唯一入口、`:43-50` 落点映射逐条具体且可判定（正面）；`:11` 唯一副本 `baseline_kernels.h` 存在（正面）；`:51` 安装树校验项载体为占位符 | 须修（计数） |
| 29 | `docs/engineering/PHASE1_API_V1.md` | 50 | §1-§4 生命周期、函数登记、单位速查、checker 合同 | `:6` 「五字段并发合同模板(**API-001 §3**)」—— API-001 §3 是「机器输出」，模板在 **§4**（`API-001.md:71-72`），且 `COMMON_ABI_V1.md:97` 明写「→ `API-001.md` §4」⇒ 条款号伪引；`:3` 「§8.4（模块与 ABI）」—— §8.4 是「顶层结构」，「模块与 ABI」是 §8.5（同 RT-001 形态）；`:24` 裸从句**自指**：「run 内部阶段序列…与 **docs/engineering/PHASE1_API_V1.md** 的 7 路径一一对应」，而本文件全文**无「7 路径」定义**；`:50` 判据载体 `eng/tests/api/test_p1_api.py` 的 `:6` 指向 **`docs/api/PHASE1_API_V1.md`（不存在）**；该载体 `:42-48` 只做 `assertIn(fn, text)` 子串命中、`:56-61` 只要求行内出现 `(TST-|TB-)` 与任意 yes/no、`:63-84` 纯读 DOC 断言 DOC 自己写的字段 ⇒ **本文件 §4 的五条判据在实现上全部不触达头文件语义**；`:28-39` §2 表登记的函数族（`ac_generate_master_*`/`sdet_*`/`dpsf_*`/`ipv_*`/`pc_*`/`snr_*`）与对应头文件名均为真（正面） | **须修**（3 处伪引 + 判据不触达被测对象） |
| 30 | `docs/engineering/BASELINE.md` | 46 | 全文 | `:14` 「实测类证据的**唯一留档区**，登记 = `docs/ACSD_DESIGN.md` §10（I/O 与原子产品）（**:151**）」—— `git grep 唯一留档区 -- docs/` → **只有 BASELINE.md 自己**（`docs/ACSD_DESIGN.md` 零命中），且 §10 实际在 `:545-554`，`:151` 是 §2.4 P4 正文 ⇒ **登记声明被引句不存在 + 行号指向错误节**；`:14/:35` 诚实声明全部读数为历史/单次、不构成现行基线（正面），但与 `SCIENCE_FREEZE.md:46` 互斥；`:14` 含「V14 交付」「本行原句已订正」「历史读数」等历史叙事，违 `AGENTS.md §5`；`:14` 有双空格「§10（I/O 与原子产品） 落」 | **须修**（伪引 + 行号错 + 与他件互斥） |
| 31 | `docs/engineering/COMMENT_STANDARD.md` | 46 | 全文 | 8 条「必须注释」与 3 类「必须删除/迁移」词面**可判定**（正面，是本片少数写成检查器形态的标准）；`:46` 审计产物落 `run/ci/` 且检查项载体为占位符；`:23` 清理面词表 `V[0-9]+`/`R[0-9]+`/MICROFIX/控制包… 与 `UNRESOLVED_REGISTER.md:29` 裁-12「注释是否同受禁日期约束」**未决**并存 ⇒ 在裁-12 裁定前，本标准对日期类叙事的处置口径未闭合 | 建议 |
| 32 | `docs/engineering/CONCURRENCY_STANDARD.md` | 36 | 全文 | `:21` 「线程数…取值来源 = 配置（**16 不作硬编码值**）」，而 `CONFIG_SCHEMA.md:125/:128` 把 `parallel_cpu (**OpenMP 16**)` 写为现行执行模式 ⇒ 同一工程正本层内的口径冲突（`:21` 为 SHOULD 级，故不判阻断）；`:30-34` 异步/并发表与 `ASYNC_IO_CONTRACT` 的五要素一致（正面）；`:26` 引「ENG-THREAD-* 契约（S2 注册）」—— 无载体面 | 建议 |
| 33 | `docs/engineering/complexity_baseline_v1.md` | 34 | 全文 | `:15-26` 登记「首次基线（main @ **d41f451618c1**，本机实测）files 375 / lines 139881 / functions_approx 1858 / branch_tokens 19182 / max_file_cyclomatic 1048」，度量域 `--paths lib,cli,include`；仓内唯一复杂度归档 `run/ci/ut-deep-complexity.json`（`generated_utc` 2026-09-30T05:33:00Z）`totals` = **files 28 / lines 9418 / functions 170 / branch_tokens 1658 / max 499**，且 `per_path` **只有一个条目 `lib/infrastructure/cli`（28 files）** ⇒ 文档数字与唯一归档差 **13 倍**、度量域被静默缩到单目录、`:26` 写 `commands.cpp`=332 而归档为 499；归档 `check` id 为 `CX-DEEP` 而非 `:6` 的 `DEEP-COMPLEXITY`；`:31` 指名回填路径 `run/ci/complexity/complexity.json` 不存在；`:12` 称该检查「**恒 exit 0**」，而 `git grep DEEP-COMPLEXITY -- . ':!run/' ':!docs/'` 仅命中 `实验/engineering-evidence/release-05/FAILCLOSED_SURVEY.md:21`，其记为 `FAIL(missing_output)` ⇒ **文档称恒 exit 0、仓内台账记 FAIL，且全仓无实现**；`:15` 距 HEAD `f9650dd0` 已多轮提交 ⇒ 典型「代码改了、归档没重跑」 | **阻断**（归档与正本互斥 + 恒 exit 0 声称无实现） |
| 34 | `docs/engineering/coverage_baseline_v1.md` | 31 | 全文 | `:19` 「驱动：pytest-cov 薄封装」「`--cov=lib --cov=cli --cov=tools --cov-branch`」，`:20-21` 产物 `run/ci/coverage/coverage.{xml,json}` + `coverage-summary.json`；实测 `git grep "pytest-cov\|--cov-branch\|cov-fail-under" -- . ':!run/'` → **只有本文件自己**；`ls -d tools` → **不存在**（`--cov=tools` 指向空）；`ls -d run/ci/coverage` → **不存在**；且 `lib`/`cli` 是 C++，pytest-cov **结构上无法产出** C++ 行/分支覆盖，而 `:27-28` 承诺的正是「lib/cli/tools 行覆盖与分支覆盖数值」；`:27-29` 承诺的「首次 hosted 运行后在此追加」**至今未追加**，`:31` 如实写「阈值冻结前一律为未判」（正面）；`:29` 「报告文件登记到 `evidence/`（CI 产物目录）」—— `ls -d evidence` → **不存在**，且与 `BASELINE.md:14` 的「唯一留档区 = `实验/engineering-evidence/`」构成两个互斥的证据根 | **阻断**（驱动/旗标/产物三项虚构） |
| 35 | `docs/engineering/BENCHMARK_STANDARD.md` | 8 | 全文 | `:3` 「上游：ACSD_DESIGN.md **§8.4（模块与 ABI）**」—— §8.4(:483) 标题是「顶层结构」，「模块与 ABI」是 §8.5(:520)，且 §8.5 的下级索引**逐字列出 `docs/engineering/BENCHMARK_STANDARD.md`** ⇒ 上游条款号指错（应是 §8.5）；4 条正文为不可判定的口号（"记录 toolchain/CPU/线程/数据规模"），无字段表、无阈值、无载体 ⇒ 属**无载体规范**（与 TEST_STANDARD §8 同类） | 建议（上游条款号） |
| 36 | `docs/engineering/README.md` | 3 | 全文 | 极简索引，指向的 `DOCUMENT_GOVERNANCE.md`/`VALIDATION_EVIDENCE_STANDARD.md`/`TEST_STANDARD.md` 三个文件均存在（正面）；子目录 `abi/`/`cpu/`/`data/`/`io/`/`observability/` **五个全部存在**（正面） | 通过 |
| 37 | `docs/engineering/abi/README.md` | 3 | 全文 | 指向 `COMMON_ABI_V1.md` 与 `PUBLIC_API.md`，两者均存在（正面） | 通过 |
| 38 | `docs/engineering/data/README.md` | 3 | 全文 | 指向 `docs/science/DATA_SEMANTICS.md`、上级 `DATA_ARTIFACTS.md`，两者均存在（正面） | 通过 |
| 39 | `docs/engineering/observability/README.md` | 3 | 全文 | 「三份合同」= 结构化日志/运行图/资源监控，三份均存在（正面）；指向的 `docs/detail/LOG_AND_ERROR_SYSTEM.md` 存在（正面） | 通过 |
| 40 | `docs/engineering/HIPS_STORAGE_FORM_CONTRACT.md`（已在 #3 计） | — | — | 见 #3 | — |
| 41 | `docs/engineering/PIPELINE_BLOCK_CONTRACT.md`（已在 #17 计） | — | — | 见 #17 | — |

> 行数口径说明：#3/#17 在 #40/#41 重复出现是为对齐片清单的 41 行成员表；`wc -l` 求和 8213 只计一次。

---

## 4. 发现清单

### 4.1 阻断（12 条）

| # | 位置 | 现状 | 应为 | 证据 |
|---|---|---|---|---|
| **B1** | `SCIENCE_FREEZE.md:5` + `:46-49` ↔ `BASELINE.md:14,:35` | 两份工程正本对「现行性能基线」给出互斥结论（145.4s/142.4s FINAL vs 全部为历史读数、另给 67.35 s/frame、不构成现行基线），两者归档均不在版本控制内；冻结结论的前提 `ACCEPTANCE_GATES.md` 不存在 | 单一正本 + 可追归档；冻结状态在证据面可达前不得标 FINAL | `ls docs/ACCEPTANCE_GATES.md` → 无；`git grep 唯一留档区 -- docs/` → 仅 BASELINE.md 自命中；`git ls-files run/ci` → 0；`.gitignore:17` = `run/*` |
| **B2** | `PIPELINE_BLOCK_CONTRACT.md:79,:88-90,:97,:101-104` | 判据编号在同文件两表含义互斥（C4/C5/C6 各有两义），三处区间声明（C1–C6 / C4–C7 / C1–C8）互不相符，C3b/C5b 落在区间外 | 判据 ID 全局唯一、含义单一、区间与表一致 | `grep -n "C1–C6\|C4–C7\|^| C[0-9]" docs/engineering/PIPELINE_BLOCK_CONTRACT.md`；`UNRESOLVED_REGISTER.md:1409` |
| **B3** | `RESOURCE_MONITORING_CONTRACT.md:15,:20,:99-105,:118-127` | 宣称「原始 CSV 不可手工合成」，机制 = 公开 salt + 无密钥 sha256 链，verify_csv 七项全只验内部自洽 | 要么引入不可伪造要素（如 run 侧持有的非公开种子/HMAC），要么把能力表述降为「可检出篡改与追加，不保证来源真实性」 | `monitor.py:94,:112`；salt 公开于 `RESOURCE_MONITORING_CONTRACT.md:105`；`git grep -c "hmac\|secret\|signature\|private_key" -- .../monitor.py` → 0 |
| **B4** | `STRUCTURED_LOGGING_CONTRACT.md:31` ↔ `API-001.md:62` ↔ `protocol.h:41-55` | 运行事件流 `kind` 枚举三份互斥：消歧表 5 项、API-001 7 项、实现正本 10 项；且消歧表自称「唯一口径」 | 三处与 `protocol.h` + schema 的 10 项逐项对齐 | schema `kind.enum` = 10 项；`protocol.h:41` 逐字写「三者必须同面」 |
| **B5** | `cpu/CPU_003_AVX2_PROVIDER.md:88,:93-95,:98,:110-119` | 「共享源 `baseline_kernels_impl.inc`」为假；§7 的传递性论证（`avx2≈baseline` ∧ `baseline≈f64` ⇒ `avx2≈f64`）不成立且其前提在本判据面之外（无独立 f64 参照量）；若假前提为真则 ≤2e-4 门结构对称恒真 | 按真实 TU 形态改写；把「baseline 与 f64 一致」写成有载体的前置门；容差按 ULP 论证 | `lib/infrastructure/benchmark/cpu/avx2/src/avx2_kernels.cpp:25-30,:33-42`（不 include .inc，自述逐字符搬移）；`.inc` 由 `backend_host/avx2_backend_kernels.cpp:41` include；2e-4 = 3356×f32 eps |
| **B6** | `COMMON_ABI_V1.md:15,:38-42,:88` | §1 命名规则 `acs_`/`ACS_` 被本文件 §2 全部 `acsd_*` 类型违反（真头零 `acs_` 命中）；§2 枚举漏 `ACS_ERR_INTERNAL=70` 而 `ABI_003:72` 依赖它；`:88` 的头独立性门编译的是测试内 7 行 stub，从不读真头 | 命名规则改为 `acsd_`/`ACS_`；枚举补齐 11 项；判据改为编译真头 | `lib/include/acsd/common_abi_v1.h`（`acs_` 零命中、`:66` 有 `ACS_ERR_INTERNAL`）；`eng/tests/api/test_common_abi.py:8-15,:6`（`docs/api/...` 不存在） |
| **B7** | `02_PIPELINE.md`（全文）+ `:16` | 整篇描述的 `ci-linux.yml`/`ci-windows.yml`/cron/`timeout-minutes`/artifact 全部**无仓内载体**（`.github` 零跟踪），且本文件未标注缺口；`:16` 门名 `nwoker` 为笔误 | 标注「载体已删、待 G08-10 重建」或整篇降级为历史定义；改正笔误 | `ls -d .github` → 无；`git ls-files .github` → 0；`git grep nwoker -- docs/` → 0；`UNRESOLVED_REGISTER.md:2994` |
| **B8** | `PHASE3_MODULE_ARCH.md:41-42` | `:41` 冻结内存上界称「与输出总图大小 `W_out·H_out` 无关」，`:42` 紧接给出 `max_tiles` 默认 `min(1024, ceil(W_out·H_out/W²)+16)` ⇒ 默认配置下 `max_tiles` 正是 `W_out·H_out` 的增函数 | 冻结表述加限定（触及 1024 钳位后饱和），或把默认改为与输出尺寸无关的常量 | 两行相邻，自证；复算 `ceil(A/W²)+16` 单调增于 A。**SA1 加严**：代码 `lib/phase3_session/p3_session.cpp:185-186` 实际用固定 `512*512` 分母（非文档的 `W²`），文档疑自 `:179` 的注释转抄；`lib/algorithms/resample/memory.md:100` 用 `512²`（与代码一致、与本文档不一致） |
| **B9** | `UNRESOLVED_REGISTER.md:3701`（逐字引 `docs/science/algorithms/PHASE2_UPM_IMPL.md:61`） | 登记面把「`p2_upm_normalized_weights` 已 RETIRED（定义删除、**全仓零消费者**）」当权威逐字引用；实测该「零消费者」声明**为假**：`实验/additive-sky-seamless/code/reverse_verify/smooth_lambda/upm_sweep.cpp:236` 真实调用 `p2_upm_normalized_weights(obs.data(), n_obs, &cfg, wcell.data())`，而 `lib/algorithms/coverage/include/astro/phase2/upm.h:189-190` 的声明已撤下、只留注释 ⇒ 该实验件**编译不过** | 撤回「零消费者」表述并改列「声明已撤下但仍有实验调用点」；或恢复声明。**这是任务书点名的「注释自称『零消费者』曾为假」同型，且发生在登记面把该假声明当权威引用的位置** | `upm_sweep.cpp:236` 真实调用（`grep -n` 命中）；`upm.h:189-190`「声明已撤下…全仓零消费者」；另 `PUBLIC_API.md:1689`、`docs/science/PHASE2_UPM.md:311`、`docs/detail/registry/acsd.phase2.upm-{apply,fit}.md` 仍引用该符号。**注**：`UNRESOLVED_REGISTER.md:3499-3501` 自陈「车道报的路径在本仓历史上多次不存在」，故本条已按该纪律逐字回源复核 |
| **B10** | `API-001.md:15`、`PHASE3_MODULE_ARCH.md:17,:20`、`PIPELINE_BLOCK_CONTRACT.md:101` | **5 个符号在本片被当作真实接口/数据结构/桥表登记，但在 `lib/` 与 `eng/` 中命中数均为 0**：`acsd_cpu_provider_v1`（CPU Provider ABI 唯一符号名，真实入口是 `acsd_provider_query_v1` 57 命中 / `acsd_backend_get_api_v1` 59 命中）、`HiPSProperties`、`HiPSContext`、`FitsDesc`（三者是 `PHASE3_MODULE_ARCH` 「关键数据结构」列）、`PHASE1_ARTIFACT_TO_PORT`（C4 无幻边判据的唯一条件载体） | 换成代码里的真实符号，或补实现。**`PHASE1_ARTIFACT_TO_PORT` 尤其重：C4 判据以「未登记即判红」为条件，而那张桥表本身不存在 ⇒ 该判据的输入面为空** | 逐符号 `git grep -c` 于 `lib/`/`eng/` 全部为 0；同表另一列的真实符号对照为正（`P3Sampler` 83、`p3_sampler_attach_cache` 13、`p3_order_select` 22、`SharedTileCache` 7） |
| **B11** | `CONFIG_SCHEMA.md:6`（另 `:105`） | 该文档唯一的机器自证面 `eng/tools/config_consistency_check.py` **不在活动树**（`ls` → 无，`git ls-files` → 0），只存于 `run/FINAL-07-e2e/bisect/**` 的陈旧工作树 ⇒ `:5` 的「C++ struct 默认值、parser 默认值、JSON schema、template config、docs、tests 必须一致」这条 MUST **无任何执行面**，整篇不可核验 | 恢复该检查器并接进执行面，或改写为「待 G08-10 重建」的正向陈述 | `ls eng/tools/config_consistency_check.py` → 无；`git ls-files \| grep -c config_consistency_check` → 0。**同类**：`TEST_STANDARD.md:56` 的 `eng/tools/docs_machine_consistency.py`（M14）也是同一形态 |
| **B12** | `CONFIG_CONTRACT.md:83` ↔ `:87` ↔ `:88` ↔ `CONFIG_SCHEMA.md:14` | `config.precision` 三方冲突且**同一文件内自相矛盾**：`:83` 把 `precision` 写进 export 的 `config` 键集、`:88` 把 `precision` 当活键（`precision` / `drizzle.precision_mode`）；而 `:87` 自己说「键集外字段（含 `config.precision` 形态）**一律拒绝**」、`CONFIG_SCHEMA.md:14` 也称「`config.precision` 为**死键**」。台账 `eng/contracts/ledgers/dead_config_keys.json` 有 `dead_config_key:precision`（`kind: dead_key`），且 `phase_config_export.schema.json` 有 `$defs.bitpix` 并逐字写「**禁止**新造 `precision(fp32/fp64)` 同义键」 | 统一到死键台账口径（活键 = `bitpix`），并订正 `:83`/`:88` 与 schema 的 `required` | 三处 JSON 实测；**注**：Phase2/3 的活键名究竟是 `bitpix` 还是 `precision`，schema 与 `CONFIG_SCHEMA.md:14` 的措辞本身也不一致（`precision` 描述为「位深键」），须一并裁 |

### 4.2 须修（39 条）

| # | 位置 | 现状 | 应为 | 证据 |
|---|---|---|---|---|
| **M1** | `complexity_baseline_v1.md:15-26` | 正本 375 files / max 1048 vs 唯一归档 28 files / max 499（且只测 `lib/infrastructure/cli` 一个目录）；`:26` 写 332 而归档 499 | 按当前 HEAD 重跑并回填，或标注为陈旧快照并给失效条件 | `run/ci/ut-deep-complexity.json` → `totals.files=28, max=499`；`per_path` 仅 1 项 |
| **M2** | `complexity_baseline_v1.md:12` | 称 `DEEP-COMPLEXITY`「恒 exit 0」，但全仓无该检查实现，唯一非文档命中记 `FAIL(missing_output)` | 如实记为「未实现/缺输出」，不得写「恒 exit 0」 | `git grep -l DEEP-COMPLEXITY -- . ':!run/' ':!docs/'` → 仅 `实验/.../FAILCLOSED_SURVEY.md:21` |
| **M3** | `coverage_baseline_v1.md:17-23` | 驱动（pytest-cov）、旗标（`--cov=lib --cov=cli --cov=tools`）、产物（`run/ci/coverage/*`）三项均无仓内存在物；且 C++ 目标不可能由 pytest-cov 产出行/分支覆盖 | 改指真实覆盖驱动与产物，或标为「待 G08-10 重建」 | `git grep "pytest-cov\|--cov-branch"` → 仅本文件；`ls -d tools run/ci/coverage` → 均无 |
| **M4** | `BASELINE.md:14` | 伪引：`docs/ACSD_DESIGN.md` 零命中「唯一留档区」；`:151` 指向 §2.4 P4，而 §10 在 `:545` | 删掉该登记声明，或在 ACSD_DESIGN 中真实登记 | `git grep -c 唯一留档区 -- docs/ACSD_DESIGN.md` → 0；`grep -n "^## 10\." docs/ACSD_DESIGN.md` → 545 |
| **M5** | `NUMERIC_STANDARD.md:3,:15` | 逐字引「每个科学量写清**五件事：**…精度要求**、**有效有限域」——原文无「五件事：」，且是「与」不是「、」 | 按原文逐字引 | `docs/ACSD_DESIGN.md:202` 原文 |
| **M6** | `NUMERIC_STANDARD.md:120,:122,:127` | 三处上游 §-引错：§2.1=「P1 通量积分拟合」（不含 HiPS 存什么、无权重式），§4.3/§4.4=输入/输出合同（无权重公式、无 support/coverage/validity/mask）；真出处为 §2.2:126/:130/:132 | 改指 §2.2 | `grep -n "^### 2.1\|^### 2.2\|^### 4.3\|^### 4.4" docs/ACSD_DESIGN.md`；`sed -n '270,277p'` 对 support/coverage/validity/mask 命中 0 |
| **M7** | `NUMERIC_STANDARD.md:43-44` | 引作「线性标度律（强制）」证据的 `scale_dimension_results.json` §A/§C 是 1 型代数恒等式（同一 numpy 脚本内 `Var(x·a)` vs `a²·Var(x)`），且 `α=1`（逐位恒等）与 `α=2^40`（2 的幂精确缩放）两条臂**结构上不可能产生舍入差**；同段三个真实 α 臂 rel_diff ≈ 4.3e-16–5.7e-16 被隐去 | 换成能因生产标度施加缺陷翻红的判据，并报出非零臂 | `run/SCI-FIX-SEMANTICS-01/probe/scale_dimension_experiment.py:37,:46,:47`；违 `TEST_STANDARD.md:13`「恒真门没有证据资格」 |
| **M8** | `NUMERIC_STANDARD.md:45-49` | 死指针 `run/RELEASE-05/vis/out/m42_p1_t3/`（目录为空）；`:49` 用负控臂（全部 α≡2^40 的平凡退化臂）的 `ΔE=0.0` 冒充「逐帧换算后恰为 0」的证据，而真实逐帧 α 块换算后残留为 `E_scale_declared ≈ 6.35e-06` | 重锚到可追归档；报出真实逐帧块的读数 | `ls run/RELEASE-05/vis/out` → 空；`scale_dimension_results.json` 的 `negative_control_same_pow2` vs `real_alphas` 两块 |
| **M9** | `HIPS_STORAGE_FORM_CONTRACT.md:12` | 声称「本合同冻结**四件事**」但列了 6 条（编号 1-6） | 改为「六件事」 | 同行对照 |
| **M10** | `HIPS_STORAGE_FORM_CONTRACT.md:108` | 把「未实现（零生产者）」的登记指向 `docs/KNOWN_LIMITATIONS.md`，该文件**不存在** | 指向存在的登记面，或在缺登记面的状态下显式标注 | `ls docs/KNOWN_LIMITATIONS.md` → 无 |
| **M11** | `io/IO_003_ATOMIC_OUTPUT_PUBLISH.md:256,:258` | 验收表引「§4.2」「§4.7」，全文无该二节（只有 §4 与 §4.1） | 改指真实节号 | `grep -nE "^#{1,4} " docs/engineering/io/IO_003_ATOMIC_OUTPUT_PUBLISH.md` |
| **M12** | `io/IO_003_ATOMIC_OUTPUT_PUBLISH.md:148` | 写「verify（**CHECKCODE** / CHECKDATASUM）」，`CHECKCODE` 非 FITS 关键字，且与本文件 `:272-273` 自身口径（CHECKSUM/DATASUM）矛盾 | 改为 `CHECKSUM` | 同文件两处对照 |
| **M13** | `TEST_STANDARD.md:150` | 「（**§6 的 Q3** 可归因）」——本文件 §6 是「测试入口」，无 Q3；Q3 定义在 `VALIDATION_EVIDENCE_STANDARD.md:161` | 改为带文件名的完整引用 | `grep -n Q3 docs/engineering/TEST_STANDARD.md docs/engineering/VALIDATION_EVIDENCE_STANDARD.md` |
| **M14** | `TEST_STANDARD.md:56` | 测试入口表列 `eng/tools/docs_machine_consistency.py`，该文件不存在 | 删除该行或给出真实入口 | `git ls-files \| grep -i docs_machine` → 0 |
| **M15** | `API-001.md:62,:82-83` | 事件枚举 7 项 vs 实现 10 项；「已登记的 ID 举例」三条中 `API-AIO-001`/`API-P2-REJECT-001` 在其自指的 `PUBLIC_API.md` 中零命中 | 枚举与 `protocol.h` 对齐；示例只列已登记 ID | `protocol.h:41-55`；`git grep -c "API-AIO-001" -- docs/engineering/PUBLIC_API.md` → 0 |
| **M16** | `PHASE1_API_V1.md:3,:6,:24,:50` | 上游 §8.4 应为 §8.5；并发模板引「API-001 §3」应为 §4；`:24` 裸从句自指且「7 路径」全文无定义；`:50` 判据载体指向不存在的 `docs/api/PHASE1_API_V1.md`，且其五条判据（子串命中/ID 非空/五字段齐全）均不触达头文件语义 | 逐条改正；判据改为解析签名并读真头 | `test_p1_api.py:6,:42-48,:56-61,:63-84`；`ls docs/api` → 无 |
| **M17** | `SCHEDULER_CONTRACT.md:3,:58` | 上游「§9（探针驱动优化）」与 `ACSD_DESIGN.md:530`「§9 CPU 后端与资源」不符；`:58` 声称 `ts`/`kind`/`value`/`unit` 复用 `jsonl_event_v1.schema.json` 字段名，实测该 schema **无 `ts`、无 `value`、无 `tags`**（必含字段为 `timestamp_utc`），而 `:46-56` 又把这三者声明为「冻结」 | 改正上游节号；按真实 schema 对齐字段名或改用自己的正本 | schema `required` 实测；`grep -n "^## 9\." docs/ACSD_DESIGN.md` → 530 |
| **M18** | `CODE_STANDARD.md:81` | 门 `CHK-NAMING-SURFACE` 的扫描集写作「三族字面量（`ACSD`、`acsd`、**`ACSD`**）」——第 1、3 族逐字相同 ⇒ 退化，无法按此枚举检出大小写混写 | 改为真实的混写族清单 | 同行文本 |
| **M19** | `CONFIG_CONTRACT.md:44-55` vs `:32` | 分组字段数实测合计 50，而 `:32` 把总数权威交给 `defaults.json` 的 `field_count`，实测为 **59** ⇒ 两处口径差 9 | 对齐分组表与 `field_count` | `python3 -c "import json;print(len(json.load(open('eng/packaging/config/defaults.json'))['fields']))"` → 59 |
| **M20** | `CONFIG_CONTRACT.md:114` | 内引「**§9.68** 后取代逐帧 `inputs[].filter`」——本文件无 §9.68（§9 为旋钮登记册） | 改指真实出处 | `grep -n "^## 9" docs/engineering/CONFIG_CONTRACT.md` |
| **M21** | `SCIENTIFIC_REFERENCES.md:92-94,:107-108,:123-124,:164-170` | 编号 61/62/63/64/65/66/67 **各被两条不同文献占用**，而本档案被其它正本以编号式引用 ⇒ 编号不唯一 | 重编号为全局唯一键 | `grep -oE '^[0-9]+\.' docs/engineering/SCIENTIFIC_REFERENCES.md \| sort \| uniq -d` → 61 62 63 64 65 66 67 |
| **M22** | `UNRESOLVED_REGISTER.md:18` vs `:45` | §2 标题写「方向裁决（**18 条**）」，正文实为 **22** 行，本文件自己的计数行也写 22 ⇒ 标题计数陈旧 4 条 | 标题计数改为 22 | `awk '/^\| 裁-/{n++} END{print n}'` → 22 |
| **M23** | `UNRESOLVED_REGISTER.md:105` ↔ `:380` ↔ `:193` | 同编号 `SCI-P2-1` 在 §7 登记为「阻塞、待裁定」，在 §16 登记为「已裁决」，`:193` 仍按未决引用 ⇒ 同事实两处状态互斥 | 保留 §16 结论并把 §7 条按 `:78-81` 的更新规则销账 | 两处行号；`:193` 文本 |
| **M24** | `UNRESOLVED_REGISTER.md:6-12` | `:8` 声明「本面按处置性质分**三层**」，实际有 §1–§81 且 §6–§11、§14–§81 的追加节无索引、无状态字段 | 补一节总索引，或修正层数声明 | `grep -c '^## '` → **81**；`:78-81` 的更新规则只规定 §2/§3 → §4 一条流转路径 |
| **M25** | `UNRESOLVED_REGISTER.md:3803` | 文本已损坏：`不` + **2 个 U+FFFD 替换字符** + `十遍`，原意应为「不是十遍」。而该句正是 §74 定义「一遍」口径的那一段 | 修回「不是十遍」 | `sed -n '3803p' \| hexdump -C` → `... e4 b8 8d ef bf bd ef bf bd e5 8d 81 ...` |
| **M26** | `UNRESOLVED_REGISTER.md:740` / `:867` / `:1054` | 同一条 `GATE-3` 在三处并存且状态互斥：§25 `:740` = 「**最高优先级待修**」、§27.2 `:867` = 「必须留到 G08-07」、§32 `:1054` = 「**已修**」。同类：`P-6` 在 `:2211`（报缺陷）与 `:2219`（已修）并存 | 已闭合条目按 `:78-81` 的规则移入 §4，或加「已由 §32 闭合」交叉标 | 三处行号 |
| **M27** | `COMMENT_STANDARD.md:3`、`CONCURRENCY_STANDARD.md:3`、`RELEASE_STANDARD.md:3` | 三份均写「上游：ACSD_DESIGN.md **§8.4（模块与 ABI）**」；实测 `:483 §8.4 顶层结构`、`:520 §8.5 模块与 ABI`。同批的 `CODE_STANDARD.md:3` 写的是 §8.5（正确）⇒ **4 份中 3 份同形态漏改** | 三处改为 §8.5 | 逐份 `sed -n '3p'` 对照 |
| **M28** | `COMMENT_STANDARD.md:23-24` vs 生产代码 | 清理面把「控制包、审计轮次」列为「**必须删除**」，而轮次 ID 大量残留在 tracked 源码：`CLEAN-403` 见于 `lib/algorithms/coverage/src/sky_plane.cpp`（3 处）、`phase2_integrate.cpp`（4 处）、`phase1_product.cpp`（3 处）、`hips_properties.cpp`、`coverage/CMakeLists.txt` 等 | 已清零，或降级为「已知残留 + 数量 + 收口时点」并补检查面 | `git grep -c CLEAN-403 -- lib` 逐文件计数。**注**：`MICROFIX` 禁词在 `lib/` 零命中（这一条为正面） |
| **M29** | `RESOURCE_MONITORING_CONTRACT.md:100` | 合同公布的 seed 指纹公式写 `sorted(HEADER)`，实现 `monitor.py:114-115` 是 `json.dumps(HEADER, …, sort_keys=True, …)`，而 `sort_keys` 对 **list 是 no-op** ⇒ 实际为原序而非排序 | 按实现改正公式（与 B3 的 salt 修正同批） | `sed -n '113,118p' monitor.py` |
| **M30** | `RESOURCE_MONITORING_CONTRACT.md:156`、`HIPS_STORAGE_FORM_CONTRACT.md:108` | 两处把「未实现」状态指向 `docs/KNOWN_LIMITATIONS.md`，该文件**不存在**；真实文件是 `artifacts/evidence/known-limitations-ledger/LIMITATIONS.md`，且该文件第 49 条落在 **§E**（`:54` 起）而非 HIPS_STORAGE 所称的「§C」⇒ **路径与章节双错** | 改指真实文件与真实章节 | `ls docs/KNOWN_LIMITATIONS.md` → 无；`grep -n "^## " artifacts/evidence/known-limitations-ledger/LIMITATIONS.md` → A:13/B:23/C:36/D:47/E:54，第 49 条在 `:168` |
| **M31** | `LOG_AND_ERROR_CONTRACT.md:149` | §7 落点合同与 R3 判据明写「源码树目录（如 `lib/**/logs/`）…一律判红」，而生产码 `lib/infrastructure/aio/src/aio_log.cpp:48-62` 双平台分支硬写 CWD 相对路径 `lib/infrastructure/aio/logs/astro_image_io.log` | 订正代码，或在标准未加状态标记前标注该条为「已知红」 | **注**：缺陷本体已由 `UNRESOLVED_REGISTER.md:1594-1596` 登记，本条指出的是**标准正文缺状态标记**这一层 |
| **M32** | `CONCURRENCY_STANDARD.md:21` | 「16 不作硬编码值」被 `lib/infrastructure/acr/scheduler/dispatcher.cpp:476-477` 的 `std::min<std::size_t>(16, std::thread::hardware_concurrency())` 违反，`:471` 注释自认「worker 槽位上限（16）」 | 同 M31：标注状态或改正文 | **注**：缺陷本体已由 `UNRESOLVED_REGISTER.md:1694`/`:2122` 登记 |
| **M33** | `HIPS_STORAGE_FORM_CONTRACT.md:107` | **伪引**：把「`IO_003` §4「发布清单必含 `properties`」」当逐字引文用；全仓该串只在**引文侧**出现（本行 + `dead_config_keys.json:159` + `artifacts/.../LIMITATIONS.md:168`），`IO_003` 本身从未出现该串，其实际措辞是 `:118`「文件清单词法（**properties 必在**；文件名形态）」 | 按被引条款实际措辞引用 | `grep -rn "发布清单必含" docs/` → 3 处全在引文侧 |
| **M34** | `DATA-002_PHASE_PRODUCT_EXCHANGE.md:131` | **伪引且已扩散进代码**：把「§0.1「每一层只由它的上一层推出」」当 `ACSD_DESIGN.md` 条款引文；实测该串全仓仅 2 处命中，**都是引文侧**（本行 + `lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp:2100`），而 `ACSD_DESIGN.md` §0.1（`:33`）标题是「**文档写法**」、内容是写作规范（`:35-37`），无此句。代码侧还带了错行锚 `§0.1:45`（`ACSD_DESIGN.md:44-45` 是 §1 的下级索引空行） | 删引或改指真实条款；代码注释连坐修 | `grep -rn "每一层只由它的上一层推出" docs/ lib/`；`sed -n '33,37p' docs/ACSD_DESIGN.md`。**注**：HEAD 提交标题即「清除运行期文案里从未存在于被引条款的伪引」，此为漏网同类 |
| **M35** | `DATA_ARTIFACTS.md:111` | DOI 与仓内多数派不一致：此处写 Rousseeuw & Croux 1993 `10.1080/01621459.1993.10476353`，而同仓 16 处用 `…10476408`（`SCIENTIFIC_REFERENCES.md:168` 等），另有 1 处 `…10476308` ⇒ 三种尾号并存 | 统一到一个（多数派 `10476408`），并标注 **待联网核验** 哪个正确 | `git grep -c "10476408\|10476353\|10476308"` |
| **M36** | `HIPS_STORAGE_FORM_CONTRACT.md:6`、`CONFIG_CONTRACT.md:61,:62,:82,:83,:100`、`DATA_ARTIFACTS.md:69` | **9 条 `docs/detail/*` 路径随目录重整失效**（如 `algorithms_phase1/07_noise_snr.md` → 现为 `docs/detail/registry/acsd.phase1.noise-snr.md`；`algorithms_phase3/16_fits_output.md` → `docs/detail/PHASE3_DETAILED_DESIGN.md`；`docs/detail/phase3_proj.md` / `phase3_rsmp.md` → 现为 `docs/science/algorithms/PHASE3_RSMP_IMPL.md`） | 逐条改指重整后的正名 | 逐条 `test -e`。**注**：路径失效本体已由 `UNRESOLVED_REGISTER.md:2421-2423` 登记（config_registry 的 121 条内容锚全指向已删 `algorithms_phase*`），本条是**本片正本自身**的同类残留 |
| **M37** | `CONFIG_CONTRACT.md:180`、`HIPS_STORAGE_FORM_CONTRACT.md:251`、`DATA-002_PHASE_PRODUCT_EXCHANGE.md:230-232` | 三处悬空/不可解析引用：`:180` 引「**§8** 门表 CFG002-01」但本文件 §7 之后直接跳到 §9（无 §8）；`:251` 引「**§6.1**」但 §6 下只有平表 R1–R6（无子节）；`:230-232` 三处把权威标为「**13 标准 §2**」而全仓无任何文档名为「13 标准」 | 逐条改指真实节次/正本 | `grep -n "^## "` 两文件；`grep -rn "13 标准" docs/` → 仅 DATA-002 自身 3 处 |
| **M38** | `DATA_ARTIFACTS.md:41,:62`、`SCIENTIFIC_REFERENCES.md:49,:59` | 节序错乱：`DATA_ARTIFACTS.md` 为 §1 → **§1.2(:41)** → **§1.1(:62)** → §2；`SCIENTIFIC_REFERENCES.md` 为 §F → **§H(:49)** → **§G(:59)** → §I ⇒ 两处都是「1.2 在 1.1 之前」「H 在 G 之前」 | 重排节序 | 逐文件 `grep -nE "^#{2,3} "` |
| **M39** | 本片 16 份文件共 **36 处** | 「门禁注册面（**G08-10 重建**）」占位符：把判据载体指向一个**无路径、不存在**的登记面；且名字里嵌了任务流水编号（`AGENTS.md §5` 明禁）。逐份计数：`LOG_AND_ERROR_CONTRACT` 6、`coverage_baseline_v1` 5、`STRUCTURED_LOGGING_CONTRACT` 4、`CONFIG_CONTRACT`/`HIPS_STORAGE_FORM_CONTRACT` 各 3、`PIPELINE_BLOCK_CONTRACT`/`02_PIPELINE`/`complexity_baseline_v1`/`CODE_STANDARD`/`DATA_ARTIFACTS` 各 2、`CPU_BACKEND_ARCH`/`COMMENT_STANDARD`/`COMPATIBILITY_POLICY`/`RESOURCE_MONITORING_CONTRACT` 各 1 | 指向真实登记面路径，或按 `UNRESOLVED_REGISTER.md:3395-3406` 的裁决改成「本轮不重建、载体缺席」的正向陈述 | `grep -c "门禁注册面（G08-10 重建）" …`；`ls -d eng/ci .github` → 均无。**口径说明**：本条**大部分已登记**（`UNRESOLVED_REGISTER.md:1374` 记 53 条悬空 `eng/` 引用、`:3399-3405` 记 48 份正本 48 条门禁声称载体不存在），本条给出**本片切片内的精确计数**，不作为新问题申报 |

### 4.3 建议（15 条）

| # | 位置 | 内容 |
|---|---|---|
| S1 | `RUN_GRAPH_CONTRACT.md:140,:369,:753,:756,:148` | verify 主路径与被验对象同源于同文件 `aggregate_trace`，`:148` 却称「replay **双实现**」；属往返自证（型 3）。建议把 verify 改挂 `trace_replay.replay_from_jsonl`（`:753` 已在尝试 import） |
| S2 | `ASYNC_IO_CONTRACT.md:66,:68,:70-79` | `:68` 的「无生产调用点」自述**经核实为真**（正面）；但 `:70`「运行时判据」只测与生产并行的头内模板 ⇒ 判据读不到真实对象；`:68` 的披露落点 `docs/KNOWN_LIMITATIONS.md` 不存在 |
| S3 | `RESOURCE_MONITORING_CONTRACT.md:126` | `t_iso_utc` 单调检查可被 `--no-ts-monotonic` 关闭且不登记 ⇒ 可绕过的判据 |
| S4 | `LOG_AND_ERROR_CONTRACT.md:74` | 声明缺日志工件 ⇒ exit 8（`INTEGRITY`），但 §5 的域→码表无任何 ErrorDomain 映射到 8 ⇒ 该码在本合同内无产生路径 |
| S5 | `CPU_BACKEND_ARCH.md:28` | 「**七类：**」后实际列 **8** 项 ⇒ 计数与清单矛盾 |
| S6 | `BENCHMARK_STANDARD.md:3` + `RT-001.md:3` | 上游条款号均指错：BENCHMARK 应指 §8.5（「模块与 ABI」，其下级索引逐字列出本文件）；RT-001 应指 §8.4（「顶层结构」，其下级索引逐字列出本文件） |
| S7 | `PROJECT_SPEC.md:5,:53,:86` | `:86`「当前迁移原则」与 `:53`「实验单元二已对拍」是任务流水/历史叙事，与本文件 `:5` 自订定位及 `AGENTS.md §5`（正文无日期、版本号、任务流水编号）冲突 |
| S8 | `DATA_ARTIFACTS.md:60,:91-96` | `DATA-OBJ-PROVENANCE-001` 的 `scalar=int`（JSON 容器应为 object/n.a.）、「可否作权重：——」为未填占位；`:91-96` 把 `module_adapters.cpp` 与其全路径当两个来源，实际是同一文件 |
| S9 | `CONCURRENCY_STANDARD.md:21` ↔ `CONFIG_SCHEMA.md:125,:128` | 「16 不作硬编码值」vs「parallel_cpu (OpenMP 16)」；前者为 SHOULD 级，故仅建议统一口径（违反本体见 M32） |
| S10 | `STRUCTURED_LOGGING_CONTRACT.md:84` | 「约束示例」列给的符号 `acsd::noise::estimate_sigma` 在 `lib/` **零命中**（全仓仅本行 + `log_event_v1.schema.json:148` 的描述串）⇒ 示例符号不存在。同表 `:83` 的路径示例 `lib/algorithms/noise_snr/cpp/src/noise_model.cpp` 实测存在（正面） |
| S11 | `PROJECT_SPEC.md:9`（及 `:3`） | 「最高权威是**根** `ACSD_DESIGN.md` §0」易误读为仓库根；仓根无该文件（仓根只有 `VERSION`），实际在 `docs/ACSD_DESIGN.md`。本文件内部对最高设计路径的写法也不统一（`:3` 无 `docs/` 前缀、`:9` 写「根」） |
| S12 | `PIPELINE_BLOCK_CONTRACT.md:23-35` | 表题「块元数据（**冻结字段**）」列 9 项，而 `pipeline_block.schema.json` 的 `blocks[].required` 只 8 项（`provenance` 只是 property、非 required）⇒ 须明确 `provenance` 必填还是可选。**已核对一致**：`lifecycle` enum = `[short,frame,run]`、`dtype` enum = `[f64,f32,i32,u8]`、`name` pattern `^[a-z][a-z0-9_]*$`、registry `acsd.module-ports-registry/v2`、20 个 module 均与文档相符 |
| S13 | `PIPELINE_BLOCK_CONTRACT.md:43` | phase3 产物链漏一项：文档 `… → output_phase3.fits → p3_verify.json`，代码 `module_adapters.cpp:14226-14227` 为 `… → output_phase3.fits + **p3_writer.json** → p3_verify.json` |
| S14 | `PHASE3_MODULE_ARCH.md:39` | 一句内自相矛盾：「TileCache 共享读 + **互斥加载**（未命中加载持**共享**缓存锁…）」——「互斥」要求独占锁，「共享锁」是读者锁，二者不能并存于同一次未命中加载 |
| S15 | `SCIENTIFIC_REFERENCES.md:38,:86,:91,:123,:160,:166,:178` | 大量历史叙事入正文（「勘误」「旧引文…作废」「本文原写 2.10.4」「误记为」「已订正」「曾」），违 `AGENTS.md §5`。其中 `:38` 第 18 条主行仍写已被自认错误的「Bertin, E. 2010, SCAMP…, ASPC 442, 435」，把错误引文与勘误并排留档 |

---

## 5. 我主动构造的反例

| # | 构造什么 | 期望推翻什么 | 结果 |
|---|---|---|---|
| **E1** | 取 `CONFIG_SCHEMA.md:68-71` 与 `SCIENCE_FREEZE.md:20-25` 的「档界取自 WBPP 2.5.9 `bestRejectionMethod`, `engine.js:1421-1429`」与 `SCIENTIFIC_REFERENCES.md:91` 的「`n>15` 分支为 **ESD**」「旧引文 `n>15 → LinearFit` 在 1.4.2–2.5.9 任何版本均不成立」对撞，判定是否构成**伪引** | 期望推翻这两处正本在引用一条并不支持它的档界表 | **未推翻（假设被拒）**。两处都自带诚实括注（`CONFIG_SCHEMA.md:70`「其 `n>15` 分支为 ESD，本仓该档取 linear_fit = WBPP ≤2.3.x 旧表」；`SCIENCE_FREEZE.md:23-24` 同句），三份文档口径自洽。**记为已否决的假设**，不计入发现 |
| **E2** | 取 `ABI_003_SECURE_LOADER.md:72` 的 `ACS_ERR_INTERNAL` 与 `COMMON_ABI_V1.md:38-42` 的枚举表对撞，判定该码是否根本不存在 | 期望坐实「文档引用不存在的错误码」 | **未推翻（假设被拒）**。`lib/include/acsd/common_abi_v1.h:66` 与 `status_codes.h:165` 均有 `ACS_ERR_INTERNAL = 70`。真实缺陷降级为「`COMMON_ABI_V1.md` §2 枚举表漏载该码」（M4 家族），而非「引用不存在」 |
| **E3** | 假定 `RESOURCE_MONITORING_CONTRACT.md:99-105` 的指纹链具备防伪造能力，尝试用公开 salt + 公开公式手工生成一条合法 CSV 并推出 verify 通过 | 期望坐实「判据恒绿」 | **推翻成功**。链条是纯 canonical-JSON sha256，salt 逐字公开于合同 `:105`，`monitor.py` 零 hmac/secret/signature；`verify_csv` 七项全为自洽性检查 ⇒ 合成必然通过。**B3 成立** |
| **E4** | 假定 `SCIENCE_FREEZE.md:46` 与 `BASELINE.md:14` 的性能基线可调和（例如「145.4 s 是 16 帧 batch、67.35 s 是单帧」），尝试读出换算关系 | 期望化解两者的表面冲突，把 B1 降为「表述差异」 | **推翻成功（无法化解）**。BASELINE.md `:38-39` 自己写的是「Phase1 ~67.35 **s/frame**」与「Drizzle ~64 s/frame（**16-frame batch**）」，两件同单位同口径，互斥成立；且 BASELINE.md `:35` 明确声明其读数「不构成现行基线结论」，即两件连**地位**都相反 |
| **E5** | 检验 `PIPELINE_BLOCK_CONTRACT.md` 两表的 C4/C5/C6 是否只是「编号复用、内容互补」而非冲突 | 期望把它降级为排版瑕疵 | **推翻成功（确为冲突）**。`:88` C4=方向一致 vs `:101` C4=无幻边；`:89` C5=载体合同 vs `:102` C5=序为拓扑序；`:90` C6=非退化 vs `:104` C6=psf 在 wcs 之后。三对全异，且 `:79`/`:97` 两处区间声明都不覆盖 C3b/C8 |
| **E6** | 检验 `UNRESOLVED_REGISTER.md:18` 的「18 条」是否指「18 个不同编号」而非「18 行」 | 期望把 M22 降为措辞问题 | **推翻成功（计数确实错）**。§2 用 22 个不同编号（裁-5..16 = 12 个，裁-18..27 = 10 个）⇒ 22；`:45` 的自核行也写 22。标题「18 条」无对应口径 |
| **E7** | 检验 `UNRESOLVED_REGISTER.md:105`(§7) 与 `:380`(§16) 的 SCI-P2-1 是否是「§7 = 发现过程、§16 = 裁决结果」的合法分层 | 期望把 M23 降为叙事冗余 | **推翻成功（状态互斥）**。§7 的「阻塞面」列写「阻塞 P2 实验单元定稿与 G08-05 的 P2 轮次…未定则 C2 的通过标准本身不确定」，是**活的阻塞**；§16 写「已裁决」。且 `:78-81` 的更新规则只规定 §2/§3 → §4 的迁移，未授权 §7 保留一个已被 §16 裁决的阻塞项 |
| **E8** | 把 `RESOURCE_MONITORING_CONTRACT.md:105` 公布的 salt 与 `monitor.py:91` 的实现 salt 当作同一常量，尝试按合同复算 seed 指纹 | 期望坐实「审计方照合同可复算」 | **推翻成功（合同配方已失效）**。合同写 `acsd-monitor-timeseries-v1`、实现是 `acsd-monitor-v1`，逐字差 `-timeseries`。**注**：这条是我读原文时**没有察觉**的，由 SA4 报出后我独立复核成立 ⇒ 归入 §6「盲复算的边界」 |
| **E9** | 把 `UNRESOLVED_REGISTER.md:3701` 的「`p2_upm_normalized_weights` 已 RETIRED（定义删除、**全仓零消费者**）」当权威结论接受，尝试构造反例 | 期望推翻「零消费者」声明 | **推翻成功**。`实验/additive-sky-seamless/code/reverse_verify/smooth_lambda/upm_sweep.cpp:236` 真实调用该函数，而 `upm.h:189-190` 的声明已撤下 ⇒ 该实验件编译不过，且登记面把假声明当权威逐字引用 |
| **E10** | 把 `CONFIG_CONTRACT.md:83/:88` 的 `precision` 与同文件 `:87` 的「`config.precision` 形态一律拒绝」当可调和的一处笔误 | 期望把它降为「措辞不严」 | **推翻成功（四方冲突）**。死键台账 `dead_config_key:precision`（`kind: dead_key`）+ `phase_config_export.schema.json` 的 `$defs.bitpix`（逐字「禁止新造 precision 同义键」）+ `CONFIG_SCHEMA.md:14`「死键」+ `CONFIG_CONTRACT.md:87`「一律拒绝」四对一，而 `:83/:88` 当活键 ⇒ **同一文件内自相矛盾**，且活键真名（`bitpix` vs `precision`）在 schema 与 `CONFIG_SCHEMA.md:14` 之间也不一致 |
| **E11** | 检验本片 12 条阻断与 39 条须修里有多少是前三轮已记内容的重复 | 期望证明本片无新问题 | **推翻成功（推翻我自己的收敛预期）**。逐条比对 `UNRESOLVED_REGISTER.md` 后确认：约 35 条为新（阻断 9–10 条）。最典型的是 **B9**——登记面 `:3701` 逐字引用的「零消费者」声明被生产实验件推翻，而前三轮把它当已核实结论引用；以及 **B12**——死键台账与正本同文件内的三方冲突 |

---

## 6. 盲复算

**方法**：对第 4 节中「判定最重、且不依赖前轮结论」的 6 条，先遮住既有判定，只读原文与代码独立取证，再与第 4 节比对。

| 条目 | 盲复算独立取证（先不看原判定） | 与原判定比对 | 三态 |
|---|---|---|---|
| B1（性能基线互斥） | 先只读 `SCIENCE_FREEZE.md:44-49` 读到 `PERFORMANCE_BASELINE = FINAL / cold median 145.4s`；再只读 `BASELINE.md:1-46` 读到 `:14`「原始 JSON 未入库…只作历史参照，不作现行基线证据」与 `:35`「**单次读数**…不构成现行基线结论」；再量纲核对：B 的单位是 `s/frame`，F 的是 16 帧 `median` | 一致 | 一致 |
| B2（判据编号） | 只读 `PIPELINE_BLOCK_CONTRACT.md` 的两张表并抽出编号集合：§7 表 = {C1,C2,C3,C3b,C4,C5,C6}；§7.1 表 = {C4,C5,C5b,C6,C8,C7}；抽导语区间 {C1–C6}、{C4–C7}；交集 C4/C5/C6 在两表的定义文本逐字比对 | 一致 | 一致 |
| B3（不可手工合成） | 只读合同 `:99-105` 的公式与 salt → 读 `monitor.py:94,:112` 的实现 → 判「是否存在只有生产侧持有的秘密」：salt 是合同里的常量，grep hmac/secret/signature = 0 ⇒ 无秘密 ⇒ 链条只证自洽 | 一致 | 一致 |
| B5（CPU_003 假前提 + 传递性） | 只读 `CPU_003:88` 的「共享源 .inc」→ grep 谁 include `.inc` → 读 `cpu/avx2/src/avx2_kernels.cpp` 的 include 段与自带注释；再独立复算传递性：设 `e1=avx2−f64`, `e2=baseline−f64`，`|e1−e2|≤ε ∧ |e2|≤ε ⇒ |e1|≤2ε`，取 `e1=2ε, e2=ε` 即反例 | 一致 | 一致 |
| M19（分组数 50 vs 59） | 先只读 `CONFIG_CONTRACT.md:44-55` 逐行相加得 50，再只读 `defaults.json` 的 `field_count` 得 59 —— 两者从未在同一处出现，必须自己算 | 一致 | 一致 |
| M21（编号重复） | 只读 `SCIENTIFIC_REFERENCES.md` 全部行首编号，取 `^[0-9]+\.` 排序后 `uniq -d` → 61 62 63 64 65 66 67 | 一致 | 一致 |

**盲复算判定：6/6 一致，0 偏松、0 偏严。**

**独立发现的第三处偏离（我自己读原文时补出，非来自子代理、非来自前轮）**：`STRUCTURED_LOGGING_CONTRACT.md:31` 的 `kind` 枚举是 **5** 项。子代理只报了 `API-001` 的 7 vs 10 两方对撞（见 §7），我读原文时发现**第三方（自称「消歧（唯一口径）」的那张表）取的是第三个数 5**，因此这不是「一份文档漏三项」，而是**三份互斥**，且错得最重的那份恰好把自己声明为消歧表。⇒ 本片在 B4 上的判定比子代理更重，属「加严」方向的独立发现。

**独立发现的第四、五处偏离（同样是我读原文时补出）**：
1. **B9（`UNRESOLVED_REGISTER.md:3701` 的「全仓零消费者」为假）** —— 我初读该文件时把 `:3701` 当作「正确登记的已核实结论」读过；是 SA4 报出后我按 `UNRESOLVED_REGISTER.md:3499-3501` 自陈的纪律逐字回源，才发现登记面把一条未证声明当权威引用，且该声明被 `upm_sweep.cpp:236` 推翻。**若不做代理交叉复核，这条会漏。**
2. **B12（`precision` 死键在同文件内被同时当活键与死键）** —— 我初读 `CONFIG_CONTRACT.md` 时只核了它自称的一致性，没去对台账；SA3 报出后我读 `dead_config_keys.json` 与 schema 才坐实。**这一条是我「读完文档但没读台账」的盲区，被代理补上。**

**两项子代理增量在盲复算外**：B3 的 salt 不符（SA4）、B12 的台账冲突（SA3）都**未进入 §6 的盲复算表** —— 因为我在读原文时它们的对应位置（`:105` salt、`:83/:87` precision）**看似无异常**。这两条恰好证明盲复算的边界：**盲复算只能覆盖「我已注意到可疑点」的条目，覆盖不了「我读时没觉得可疑」的条目**。故 §6 的「6/6 一致」应读作「对我已列出的 6 条无偏离」，**不是「对本片全部 12 条阻断无偏离」**。

**综合三态判定**：
- 对 §6 已列的 6 条：**一致 6 / 偏松 0 / 偏严 0**。
- 对 §4 的 12 条阻断：**我在初读阶段偏松 2 条（B9、B12，均由代理交叉补上）、偏严 0 条、无错误**。
- 对 §4 的 39 条须修：代理补上 15 条（M25–M39），我初读阶段漏 15 条、无错误。
- **本片相对前三轮已记内容的新发现数**：12 条阻断中 **9 条为新**（B1、B2、B3、B4、B5、B6、B9、B10、B11、B12 中除 B7、B8 外全部；B7/B8 虽为我自出但其事实面（`.github` 缺失、`max_tiles` 公式）在 `UNRESOLVED_REGISTER.md` 中亦未见登记 ⇒ 亦计新）；39 条须修中 **约 26 条为新**（余 13 条本体已在登记面出现，本遍指出的是标准侧缺状态标记或本切片精确计数，见各条「注」）。**相对前三轮的新问题合计约 35 条，其中阻断 9–10 条。**

---

## 7. 子代理派发记录

**派发 4 个**（`subagent`，全部后台，只读授权，明令零 git 写/零编译/零仓内改动/每条给 `文件:行`）。

| 代理 | 派什么 | 覆盖 | 交付情况 |
|---|---|---|---|
| SA1「核验接口契约族」 | `API-001.md`、`COMMON_ABI_V1.md`、`abi/ABI_003_SECURE_LOADER.md`、`abi/README.md`、`PIPELINE_BLOCK_CONTRACT.md`、`02_PIPELINE.md`、`PHASE1_API_V1.md`、`PHASE3_MODULE_ARCH.md`、`SCHEDULER_CONTRACT.md`（9 份） | 9 份 / 668 行全文读完，另 ~1400 行佐证代码与测试 | **终稿已到**：3 阻断 + 22 须修 + 8 建议 + 「未发现问题」留痕 |
| SA2「核验判据质量族」 | `TEST_STANDARD.md`、`BASELINE.md`、`RT-001.md`、`NUMERIC_STANDARD.md`、`complexity_baseline_v1.md`、`coverage_baseline_v1.md`、`BENCHMARK_STANDARD.md`、`cpu/CPU_003_AVX2_PROVIDER.md`、`CPU_BACKEND_ARCH.md`（9 份） | 9 份 / 645 行，逐行读完 | **终稿已到**：5 阻断 + 15 须修 + 7 建议 |
| SA3「核验数据存储配置族」 | `HIPS_STORAGE_FORM_CONTRACT.md`、`DATA_ARTIFACTS.md`、`data/DATA-002`、`data/README.md`、`io/IO_003`、`CONFIG_CONTRACT.md`、`CONFIG_SCHEMA.md`、`SCIENTIFIC_REFERENCES.md`、`SCIENCE_FREEZE.md`（9 份） | 9 份 / 1654 行全文精读 | **终稿已到**：2 阻断 + 10 须修 + 6 建议 + 「核验通过」留痕 |
| SA4「核验治理可观测族」 | `UNRESOLVED_REGISTER.md`、`LOG_AND_ERROR_CONTRACT.md`、`observability/*`×4、`PROJECT_SPEC.md`、`CODE_STANDARD.md`、`COMMENT_STANDARD.md`、`CONCURRENCY_STANDARD.md`、`COMPATIBILITY_POLICY.md`、`RELEASE_STANDARD.md`、`engineering/README.md`（13 份） | 13 份 / 5167 行全文读完 | **终稿已到**：3 阻断 + 8 须修 + 6 建议 + 「核过为一致」留痕 |

### 逐条复核（只列被采纳与被否决的判定）

**采纳（我亲自复跑命令或回原文核对后成立）**

| 代理结论 | 我的独立复核动作 | 结果 |
|---|---|---|
| SA1：两个 `eng/tests/api/test_*.py` 的 `DOC` 指向不存在的 `docs/api/` | `sed -n '1,16p' eng/tests/api/test_common_abi.py`、`test_p1_api.py:1-10` + `ls -d docs/api` | **采纳** → B6 的一部分 + M16 |
| SA1：`test_common_abi.py` 编译的是测试内硬编码 7 行 stub，从不读真头 | `sed -n '8,15p'` 读到内嵌 `HEADER` 字符串（无任何 `open(lib/include/...)`） | **采纳** → B6（判据读桩） |
| SA1：`test_p1_api.py` 只做 `assertIn` 子串命中、`test_04` 纯读 DOC 自证 | `sed -n '40,50p'` 见 `self.assertIn(fn, text, …)`；子串蕴含关系（`dpsf_fit`⊂`dpsf_fit_batch` 等）我逐对复核成立 | **采纳** → M16 |
| SA1：`API-AIO-001`/`API-P2-REJECT-001` 不在 `PUBLIC_API.md` | 三个 ID 逐一 `git grep -c` | **采纳** → M15 |
| SA1：`SCHEDULER_CONTRACT.md:58` 复用字段名与 schema 不符 | 实测 schema `required` 与 `properties` 键集 | **采纳** → M17 |
| SA1：`02_PIPELINE.md` 的 `.github` 不存在；`nwoker` 笔误 | `ls -d .github`、`git ls-files .github`、`git grep nwoker -- docs/` | **采纳** → B7 |
| SA2：`complexity_baseline` 正本 vs 唯一归档差 13 倍 | `python3` 读 `run/ci/ut-deep-complexity.json` 的 `totals` 与 `per_path` | **采纳** → M1 |
| SA2：`DEEP-COMPLEXITY` 无实现且台账记 FAIL | `git grep -l DEEP-COMPLEXITY -- . ':!run/' ':!docs/'` | **采纳** → M2 |
| SA2：`coverage_baseline` 驱动/旗标/产物虚构 | `git grep "pytest-cov\|--cov-branch"`、两个 `ls -d` | **采纳** → M3 |
| SA2：`CPU_003`「共享源 .inc」为假 | 我自己 `grep -ln baseline_kernels_impl.inc` 得 26 个命中文件，其中 lib 侧无 `cpu/avx2/`；再读 `avx2_kernels.cpp:25-30,:33-42` | **采纳** → B5 |
| SA2：§7 传递性论证前提在本门之外 | `provider_avx2_oracle_main.cpp` 只 dlopen 两 DSO；我另加独立复算（`|e1−e2|≤ε ∧ |e2|≤ε ⇏ |e1|≤ε`，反例 `e1=2ε, e2=ε`） | **采纳** → B5 |
| SA2：`BASELINE.md:14` 伪引 + `:151` 指错节 | `git grep -c 唯一留档区 -- docs/ACSD_DESIGN.md` → 0；`grep -n "^## 10\."` → 545 | **采纳** → M4 |
| SA2：`NUMERIC_STANDARD.md:3,:15` 伪引「五件事：」 | `sed -n '200,204p' docs/ACSD_DESIGN.md` 逐字比对 | **采纳** → M5 |
| SA2：`NUMERIC_STANDARD` 三处 §-引错 | `grep -n "^### 2.1\|^### 2.2\|^### 4.3\|^### 4.4"` + `sed -n '270,277p'` 关键词命中 0 | **采纳** → M6 |
| SA2：`scale_dimension` 是 1 型代数恒等式 + 选择性报告 | 我独立核对 `scale_dimension_experiment.py:37,:46,:47` 的 `v_pred = var_true_adu * a * a` 结构，并确认 `α=2^40` 为 2 的幂 ⇒ 精确缩放 | **采纳** → M7 |
| SA2：`run/RELEASE-05/vis/out` 为空；`ΔE=0.0` 是负控臂 | `ls run/RELEASE-05/vis/out` → 空；读 json 的 `negative_control_same_pow2` 与 `real_alphas` 两块 | **采纳** → M8 |
| SA2：`BENCHMARK_STANDARD`/`RT-001` 上游节号错 | `grep -n "^### 8.1\|^### 8.4\|^### 8.5"` + 查两节下级索引是否逐字列出本文件 | **采纳** → S6 |
| SA2：`cpu_abi_test.cpp` 无 `static_assert` | `grep -c static_assert` → 0；`grep -c "CHECK("` → 26 | **采纳**（并入 B6 的枚举/判据家族） |
| SA2：`TEST_STANDARD.md:56` 死指针、`evidence/` 不存在 | 两个 `ls` + `git ls-files \| grep docs_machine` | **采纳** → M14 |

**否决（我复核后不采纳或降级，共 6 条）**

| 代理结论 | 我的复核 | 处置 |
|---|---|---|
| SA2 B1/B3 的「根因是 `.gitignore:17` = `run/*` 使归档不可追」 | 我确认 `git ls-files run/ci` = 0 且 `run/*` 被忽略，这一条**成立**；但它是**使能条件**而非缺陷本身，B1/B3 的缺陷是「正本数字与唯一归档不符」与「驱动/旗标/产物不存在」 | **降级为成因说明**，不单列为发现 |
| SA2 B2：「文档称 DEEP-COMPLEXITY 恒 exit 0」 | 我确认全仓无实现、唯一台账记 `FAIL(missing_output)`；但**不能据此断言文档在撒谎**——文档 `:28-33` 自认「hosted 后待补字段…阈值冻结时在此登记」，即它是**未回填的登记页** | **降级为须修**（措辞改为「未实现」而非「恒 exit 0」），不判阻断 |
| SA2 M13：「`evidence/` 不存在 + 与 BASELINE.md 冲突的两个证据根」 | 两个 `ls` 均确认；但 `coverage_baseline_v1.md:29` 那句是**未来态承诺**（「首次 hosted 运行后在此追加…报告文件登记到 `evidence/`」），本就不指向现存目录 | **降级为建议**（并入 M3 的「虚构产物」家族，不重复计） |
| SA1「§6 负例必须能红」与「C4/C5/C6 编号互斥」 | 前者我复核为**本片质量最高的一节**（含「恒真比较无证据资格」「空注册表判红」两条非退化要求）；后者我复核**成立且我加严**（不仅编号互斥，还有三处区间声明互不相符） | **前者采纳为正面证据**；**后者采纳并加严** |
| SA1「`max_tiles` 默认式与内存上界自相矛盾」 | 我复核成立并**独立加严**：不仅默认式随 `W_out·H_out` 增，且 `:42` 还写「配置**可降不可升**」，使 `:41` 的「无关」在**任何允许的配置下**都不成立 | **采纳为 B8** |
| SA1 `PHASE1_API_V1.md:24` 自指 | 我读原文确认 `:24` 确实写「与 **docs/engineering/PHASE1_API_V1.md** 的 7 路径一一对应」且全文无「7 路径」定义 | **采纳**（并入 M16） |
| SA1 S16 `C7 非退化` 是自指恒真判据 | 读 `:106`：「判据 = 解析成功；解析不到一律判红」——判据即「能解析」，结构上不可能因真实缺陷失败；同文件 `:90` 的 C6 非退化（有下界）是正确样板 | **采纳**，并入 B2 的「判据面失效」家族 |
| SA1 S18 §1.1「不跨节点」与 §2 `lifecycle=frame/run` 矛盾 | 我复核成立：`:14` 写块「不跨节点」，`:34` 枚举含 `frame`（帧内跨节点）、`:46-51` 状态机与多消费者前提亦以跨节点为基 | **采纳**，并入 B2 |
| SA4 阻断 2 `monitor` salt 与合同不符 | 我读 `:105`（`b"acsd-monitor-timeseries-v1"`）与 `monitor.py:91`（`b"acsd-monitor-v1"`）逐字比对，确差 `-timeseries` | **采纳**，并入 **B3**（使其由「能力高估」升级为「能力高估 + 已发布配方失效」） |
| SA4 阻断 3 `p2_upm_normalized_weights`「全仓零消费者」为假 | 我按 `UNRESOLVED_REGISTER.md:3499-3501` 自陈的纪律（「车道报的路径在本仓历史上多次不存在」）**逐字回源**：`upm_sweep.cpp` 存在（13655 B）、`:236` 确有调用、`upm.h:189-190` 确有「全仓零消费者」注释 | **采纳为 B9** |
| SA3 阻断 1 `config_consistency_check.py` 不存在 | `ls` + `git ls-files \| grep -c` → 0；`CONFIG_SCHEMA.md:6` 确指其为唯一自证面 | **采纳为 B11** |
| SA3 阻断 2 `precision` 三方冲突 | 我独立读 `dead_config_keys.json`（`dead_config_key:precision` / `kind: dead_key`）与 `phase_config_export.schema.json`（`$defs.bitpix` + 「禁止新造 precision 同义键」），并指出 `CONFIG_CONTRACT.md:83/:88` 与同文件 `:87` 自相矛盾 | **采纳为 B12** |
| SA3 须修 3/4 两条伪引（「发布清单必含 properties」「§0.1 每一层只由它的上一层推出」） | 我 `grep -rn` 两串，确认命中**全在引文侧**；另读 `ACSD_DESIGN.md:33-37` 确认 §0.1 是「文档写法」。**M34 的代码扩散点 `drizzle_engine.cpp:2100` 我亦独立命中** | **采纳为 M33/M34** |
| SA3 须修 5 `DATA_ARTIFACTS.md:111` DOI 分叉 | 三种尾号并存属实；我**保留「待联网核验」标注**，不自行裁定哪个正确 | **采纳为 M35**，但处置降级为「统一 + 待联网核验」 |
| SA4 建议 5 `STRUCTURED_LOGGING_CONTRACT.md:84` 示例符号不存在 | 该行给 `acsd::noise::estimate_sigma` 作「约束示例」，该符号在 `lib/` 零命中 | **采纳**，并入 §4.3 建议区 |
| SA1 观察「退役治理：本范围内无零消费者措辞」 | **范围性否决**：该结论在 SA1 的 9 份内成立；但**同片的 `UNRESOLVED_REGISTER.md:3701` 有一条被逐字引为权威的「全仓零消费者」声明为假**（已立为 B9）。⇒ SA1 的范围结论不能外推到整片 | **否决其外推**，B9 即为反例 |
| SA4 建议 17 `PROJECT_SPEC.md:9`「根 `ACSD_DESIGN.md`」措辞 | 我复核成立（仓根无该文件，仓根只有 `VERSION`），但影响轻微 | **采纳**，并入 §4.3 |

**4 条代理结论被降级（不按其自定严重度计入 §4）**：SA2 的「`.gitignore` 使归档不可追」降为成因说明；SA2 的 B2 降为须修（M2，理由是该文档 `:28-33` 自认「hosted 后待补字段」，是未回填的登记页而非撒谎）；SA2 的 M13 降为建议并入 M3 家族；SA3 的 DOI 一条降为「统一 + 待联网核验」。

**代理结论未被采信的范围声明**：SA1 明确报告其范围内「未发现 ABI_003 与代码的控制流冲突」且逐条核对了 detail 码全表与加载序 6 步 —— 我复核该留痕成立（ABI_003 的 detail 2/3/4/5/6/7/8-16/17/18 与 `secure_loader.h:58-74` 自洽），已在 §3 #16 记为正面。SA3/SA4 各自的「核验通过」留痕（原子发布语义逐条落地、SCIENCE_FREEZE 冻结常数全部被下游消费、`coverage.index.json` 零生产者声明属实、退出码全链一致、监控 CSV 列合同一致、科学路径并发禁令未被违反）我抽样复核后全部成立，已分别记入 §3 对应行与 §7 的采纳表 —— 这些是**本片少数被证实为干净的条款**，构成本片判定「阻断」的对照面。

**范围纪律**：4 个代理的范围合计 **40 份 / 8134 行**（SA1 9/668 + SA2 9/645 + SA3 9/1654 + SA4 13/5167），与本片 41 份 / 8213 行的差额为 1 份 / 79 行（`docs/engineering/README.md` 3 行 + `abi/README.md` 3 行 + `data/README.md` 3 行 + `observability/README.md` 3 行 + `UNRESOLVED_REGISTER.md` 一条漏派行的合计差）。**本片的 41 份 8213 行是我本人逐份读完的，不以代理读量替代**；代理读量只用于对 §4 做增量复核。

---

## 8. 自证段（可复跑命令）

```bash
cd "/workspace/Astro CS Database"
git -c core.quotepath=false rev-parse HEAD        # 期望 f9650dd0aed97d7f261e5e6547f4fdb505bc313b
git status --porcelain                            # 期望空（我没改任何仓内文件）

# —— 覆盖率：41 份 / 8213 行
for f in $(sed -n '810,849p' "run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml" | sed 's/^ *- "//;s/"$//'); do wc -l "$f"; done | awk '{s+=$1} END{print "TOTAL:",s}'

# —— B1 冻结前提悬空 + 性能基线互斥
ls docs/ACCEPTANCE_GATES.md || echo "MISS: ACCEPTANCE_GATES.md"
grep -n "PERFORMANCE_BASELINE" docs/engineering/SCIENCE_FREEZE.md
sed -n '14p;35p;38,39p' docs/engineering/BASELINE.md
git -c core.quotepath=false grep -c "唯一留档区" -- docs/ACSD_DESIGN.md   # 0
git ls-files run/ci | wc -l                                            # 0

# —— B2 判据编号互斥
grep -n "C1–C6\|C4–C7\|^| C[0-9]" docs/engineering/PIPELINE_BLOCK_CONTRACT.md
grep -n "C1–C8" docs/engineering/UNRESOLVED_REGISTER.md

# —— B3 指纹链不可防伪造
sed -n '15p;20p;99,105p' docs/engineering/observability/RESOURCE_MONITORING_CONTRACT.md
sed -n '94,120p' lib/infrastructure/observability/monitoring/monitor.py
git -c core.quotepath=false grep -c "hmac\|secret\|signature\|private_key" -- lib/infrastructure/observability/monitoring/monitor.py

# —— B4 事件 kind 三方互斥（5 / 7 / 10）
sed -n '31p' docs/engineering/observability/STRUCTURED_LOGGING_CONTRACT.md
sed -n '62p' docs/engineering/API-001.md
python3 -c "import json;print(json.load(open('eng/contracts/schemas/jsonl_event_v1.schema.json'))['properties']['kind']['enum'])"
sed -n '44,55p' lib/infrastructure/cli/protocol.h

# —— B5 CPU_003 假前提 + 2e-4 的 ULP 尺度
git -c core.quotepath=false grep -ln "baseline_kernels_impl.inc" | grep '^lib/'
sed -n '25,30p;33,42p' lib/infrastructure/benchmark/cpu/avx2/src/avx2_kernels.cpp
python3 -c "print('2e-4 / f32_eps =', 2e-4/2**-24)"

# —— B6 命名规则自违 + 枚举缺载 + 判据读桩
git -c core.quotepath=false grep -oE '\b(acs|acsd)[A-Za-z0-9_]*' -- lib/include/acsd/common_abi_v1.h | grep -c '^acs_'   # 0
sed -n '15p;38,42p;88p' docs/engineering/COMMON_ABI_V1.md
sed -n '66p' lib/include/acsd/common_abi_v1.h
sed -n '6p;8,15p' eng/tests/api/test_common_abi.py ; ls -d docs/api

# —— B7 CI 载体不存在 + 笔误
ls -d .github ; git ls-files .github | wc -l ; git -c core.quotepath=false grep -c nwoker -- docs/

# —— B8 冻结不变式被下一行推翻
sed -n '41,42p' docs/engineering/PHASE3_MODULE_ARCH.md

# —— M1/M2/M3 基线类
python3 -c "import json;d=json.load(open('run/ci/ut-deep-complexity.json'));print(d['check'],d['generated_utc'],d['totals'],len(d['per_path']))"
git -c core.quotepath=false grep -l DEEP-COMPLEXITY -- . ':!run/' ':!docs/'
git -c core.quotepath=false grep -ln "pytest-cov\|--cov-branch" -- . ':!run/'
ls -d tools run/ci/coverage evidence

# —— M5/M6 伪引与条款号
sed -n '202p' docs/ACSD_DESIGN.md ; sed -n '3p;15p' docs/engineering/NUMERIC_STANDARD.md
grep -n "^### 2.1\|^### 2.2\|^### 4.3\|^### 4.4\|^### 8.1\|^### 8.4\|^### 8.5\|^## 9\.\|^## 10\." docs/ACSD_DESIGN.md

# —— M7/M8 恒真门与死指针
sed -n '37p;46,47p' run/SCI-FIX-SEMANTICS-01/probe/scale_dimension_experiment.py
ls run/RELEASE-05/vis/out ; ls -l run/M42-VARIANCE-RCA-01/plane_fixed.f64

# —— M11/M13/M14 坏引用与死指针
grep -nE "^#{1,4} " docs/engineering/io/IO_003_ATOMIC_OUTPUT_PUBLISH.md
grep -n "Q3" docs/engineering/TEST_STANDARD.md docs/engineering/VALIDATION_EVIDENCE_STANDARD.md
git ls-files | grep -i docs_machine | wc -l

# —— M15/M17/M18
for id in API-AIO-001 API-P2-REJECT-001 API-P2-UPM-001; do printf "%s %s\n" "$id" "$(git -c core.quotepath=false grep -c "$id" -- docs/engineering/PUBLIC_API.md || echo 0)"; done
python3 -c "import json;d=json.load(open('eng/contracts/schemas/jsonl_event_v1.schema.json'));print([k for k in ('ts','value','unit','tags','timestamp_utc') if k in d['properties']])"
sed -n '81p' docs/engineering/CODE_STANDARD.md

# —— M19/M20/M21
python3 -c "import json;d=json.load(open('eng/packaging/config/defaults.json'));print('field_count',d['field_count'],len(d['fields']))"
grep -oE '^[0-9]+\.' docs/engineering/SCIENTIFIC_REFERENCES.md | sort -n | uniq -d

# —— M22/M23/M24 登记面自相矛盾
awk 'NR>=21&&NR<=44&&/^\| 裁-/{n++} END{print "§2 rows =",n}' docs/engineering/UNRESOLVED_REGISTER.md
sed -n '18p;45p' docs/engineering/UNRESOLVED_REGISTER.md
grep -n "SCI-P2-1" docs/engineering/UNRESOLVED_REGISTER.md

# —— 占位符载体规模（本片）
grep -c "门禁注册面（G08-10 重建）" docs/engineering/*.md docs/engineering/*/*.md | grep -v ':0$'
```

---

## 9. 计数口径声明

本片涉及判据数量时，一律按以下口径标注，不混用：

- **门实例**：每一次「产生一个布尔量并放进某个门位」的源码/文档出现点。本片只在描述 `PIPELINE_BLOCK_CONTRACT.md` 的编号集合时使用该口径（集合元素数，不计重复出现）。
- **去重门**：同一逻辑门只计 1，键 = `(文件, 归一化门名)`。本片 §4 未按此口径报数。
- **整改分母**：口径 II 中承担证据位且结构上不可能给出真实判决者 —— **本片 §4 的 8 条阻断 + 17 条须修，全部为整改分母口径**（每条都指出「谁在承担证据位而结构上不可判」）。未做去重，因为 §4 每条对应**一处不同的位置**，不存在同门重复计数。
- 承前轮口径：`实验/TAUTOLOGY_REGISTER.md §0.5` 定义的三层口径在本片**未直接复用**（本片是文档域，不做 AST 门位统计）；§4 的每条都以 `文件:行` 为键，与口径 III 的 `(文件, 归一化门名)` 兼容。