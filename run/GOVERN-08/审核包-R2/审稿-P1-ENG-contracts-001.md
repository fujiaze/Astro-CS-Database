# 审稿-P1 · ENG-contracts-001 · G08-05 对抗审稿第 1 遍

- 审稿人：SubAgent（片 ENG-contracts-001）
- 基线：仓库 `/workspace/Astro CS Database`，HEAD = `850a9ede`
- 日期口径：第 1 遍 = 对本片 29 个成员文件的一次完整重读
- 纪律自证：`git -c core.quotepath=false status --porcelain -- eng/contracts/` 输出为空 ⇒ 本人未修改任何仓内文件。未执行编译 / ctest / pytest / 任何仓内测试脚本。⛔ 未读 `/tmp/acsd_g08/`。

---

## 1. 读完了吗

| 项 | 值 |
|---|---|
| 成员份数（权威清单） | **29** |
| 实际读完份数 | **29** |
| 成员总行数 | **8175** |
| 实际读了多少行 | **8175** |
| 覆盖率 | **29/29 份 = 100%；8175/8175 行 = 100%** |
| 未读完的 | **无** |

行数与 `片清单-权威版.yaml:971-973`（成员份数 29 / 实际行数 8175）逐项吻合，无缺失、无多余成员。

唯一需要说明的口径：`clause_registry.json` 单文件 4159 行（占全片 50.8%）由本人**逐行读完**，未抽样。

---

## 2. 本片判定

### **判定：需修（倾向阻断）**

**最重的 3 条**

| # | 结论 | 定位 |
|---|---|---|
| **1** | **条款注册表的 `source_binding` 溯源大面积伪引：46 条带绑定条款中 23 条 `contains`、42 条 `locator` 在被引文档里逐字不存在。**`PSFSW-T-*` 的 17 个阈值 ID 与其数值（如 `PSFSW-T-DEPTH` / `0.05`）在全仓任何文档中都不存在 —— 注册表是这些数值的**唯一持有者**，却同时宣称它们「值与文本唯一确定」。这是本轮**最有价值的产出**：自证式断言 + 悬空引用叠加。 | `eng/contracts/data/clause_registry.json:953-960`（`source_binding`）、`:2100`；被引 `docs/science/PSF_SIGNAL_WEIGHT.md` 全文 150 行 |
| **2** | **守锚门的名字与函数体不符：`test_clause_anchors_reanchored_to_living_docs` 文档串写「49 条待签条款的锚不得悬空」，实际只检查 `source_binding.file` 且只对 `docs/` 前缀。** `anchor` 字段（20 条待签条款引的是注册表自己 `reanchor_map` 声明「已出库」的文档）**完全不被检查** ⇒ 该门对锚点悬空**结构上不可能转红**，却因文件存在而常绿，掩护了 20 条真悬空锚。 | 门：`eng/tests/contracts/product_family/test_field_constraints_integration.py:176-186`；被漏检项：`clause_registry.json:1361,1386,1411,1436,1461,1486,1511,1536,1561,1586,1611,1636,1661,1686,1711,1736,1761,1786,1811,1836` |
| **3** | **`examples_index` 10 条路径全部悬空（且零消费者），`migration_map.file_migrations` 20 条路径全部悬空，而唯一读 `migration_map` 的门只数条目个数、从不打开路径。** 门 `O23-migration-map` 断言 `n_schema_mig == 10`，对 20 条死路径完全无感；配套的 mutation 用例（删掉 signal 条目）恰好只改计数 → 制造「该门能抓迁移表完整性」的错觉。 | `clause_registry.json:3586-3597`（examples_index）、`:3870-3933`（file_migrations）；门：`eng/tests/contracts/product_family/field_constraints_oracle.py:630-648`；错位 mutation：`eng/tests/contracts/product_family/test_field_constraints_mutations.py:139-140` |

---

## 3. 逐文件清单（29 份全读）

> 「读到什么」= 本人实际读到的内容与结构；「看到什么」= 可复现的观察。

| # | 成员文件 | 读了什么 | 看到什么（含 `文件:行`） | 判定 |
|---|---|---|---|---|
| 1 | `eng/contracts/data/clause_registry.json` (4159) | 全读 | `counts.clauses_total=96` 与实际 `clauses[]` 长度**一致**（本人独立数出 96，状态 39/49/8 全部吻合）⇒ **计数无误**。但 `:3586-3597` examples_index 10 条路径全错（真实位置少一层 `v6/`）；`:3870-3933` file_migrations 20 条路径全不存在；`:953-2099` source_binding 23 `contains`/42 `locator` 伪引；`:1361-1836` 20 条 `anchor` 引已出库文档 | **需修** |
| 2 | `eng/contracts/schemas/unified/signal.schema.json` (572) | 全读 | 4 份统一 schema 中**唯一** `declared_via_provenance` 分支真正可满足者（`:451-490` 强制 `bunit="ADU"` + `:90-100` 自由 enum）；但 `:90-100` 的 `dimensionless`/`count`/`probability`/`magnitude` 4 个值因 `product_family` 必填且被 allOf 覆盖而**结构不可达** | 建议 |
| 3 | `eng/contracts/schemas/jsonl_event_v1.schema.json` (491) | 全读 | `:490` `additionalProperties:true`（全片最开放的根）；`:388-390` 三个 `required` 字段未在 `properties` 声明 ⇒ 类型不受约束；`:39` `sequence minimum:0` 恒真 | 须修 |
| 4 | `eng/contracts/schemas/unified/sparse_snr_layer.schema.json` (399) | 全读 | `:301` `const "absolute_flux_type_snr"` 是 n6 唯一被触发的约束；`:30-52` 禁止键守卫（regex + not.enum 双层）**真实有效**、非冗余 | 通过（n6 目标 schema 本身） |
| 5 | `eng/contracts/schemas/phase_config_mosaic.schema.json` (341) | 全读 | 三分支 `if/then/else` 闭合良好（`:274/:294/:326` `propertyNames` 白名单）；`:210-217` `$defs.upm`/`$defs.reject` 是裸 `{"type":"object"}` ⇒ 硬件键禁令漏这两个口子 | 须修 |
| 6 | `eng/contracts/schemas/unified/rejection.schema.json` (335) | 全读 | 本人复核确认：`bunit const "1"` + `pixel_semantics const "probability"` 使 `declared_via_provenance` **恒红**、`target_pixel_area` 门**恒绿**（与 validity 同构）。**该缺陷项目已自行登记** `eng/contracts/UNRESOLVED_REGISTER.md:3992-4002`（78.1 条，点名「同块逐字复制进 coverage/rejection/support/validity 四份 ⇒ 一处错、四处同错」） | 阻断（已登记未修） |
| 7 | `eng/contracts/schemas/unified/support.schema.json` (324) | 全读 | 同 6 的恒红/恒绿结构；`:286-287` `[0,1]` 区间是全片**少数真正可达**的数值门；但「support=0 ⟺ 无覆盖」的关系无任何约束 | 阻断（同 6） |
| 8 | `eng/contracts/schemas/unified/validity.schema.json` (314) | 全读 | **本人逐行复核**：`units.bunit const "1"`(`:78`) ⇒ `allOf[0]`(`:102-125`) 的 `pattern "/sr"` **恒不触发**；`allOf[1]`(`:127-149`) 强制 `pixel_semantics const "surface_brightness"`(`:143`) 与基础层 `const "count"`(`:89`) **互斥** ⇒ `declared_via_provenance` **永不可满足**，`:94-98` `target_pixel_area.exclusiveMinimum:0` **永不可达=恒绿**。唯一合法值 `written_px_power` 的自述(`:82`)是「BUNIT 显式含立体角幂次（canonical "sr"）」——而 BUNIT 被钉死为 `"1"`，**语义为假** | 阻断 |
| 9 | `eng/contracts/schemas/hardware_inspect.schema.json` (150) | 全读 | `:3` `$id` 用 RFC-2606 保留 TLD `acsd.invalid`（永不可解析）；`:145-147` 必填 `quota_signature` 是裸 string ⇒ `""` 过；`:80-83` `affinity_count` 与 `:73-79` `affinity` 长度**无任何绑定** | 须修 |
| 10 | `eng/contracts/data/phase_product_exchange_matrix.json` (141) | 全读 | `:44`/`:54` phase2/phase3 的 `min_planes` 均缺 `variance`/`ivar`（phase1 `:34` 有）；`:62,71,80,89,98,107` 的 `to_role`/`from_role` 端点（`phase2_input`/`phase3_input`/`external_consumer`/`external_fixture`）在 `:26-57` 的 `roles[]` 中**一个都不存在** ⇒ 每条边的端点都悬空 | 须修 |
| 11 | `eng/contracts/schemas/run_manifest.schema.json` (124) | 全读 | `:34` `additionalProperties:false` **真实生效**（`:20` 的禁止名 isa/workers/block/sci_algorithm_id/sci_weight_mode 因未声明而被拒）——这是全片少数做对的门；但 `:119-122` `storage` 是裸 `{"type":"object"}`，描述声称「字段级校验由该 $defs 承载」却**无任何 `$ref`**，且写明「缺失 ⇒ 不判红」 | 须修 |
| 12 | `eng/contracts/resource_gate_v1.json` (97) | 全读 | `:6` 自称「唯一数值源…实现侧不得再出现字面量阈值」，但 `:8-9` 的 readers 路径写作 `cli/resource_gate.h` / `cli/memory_report.h` —— **该路径不存在**（真实为 `lib/infrastructure/cli/…`）。`:44,46,49` 三条判据 enforcement = `record_and_justify`，`:58` 明写「不改变退出码…未标定前不得硬失败」，而 `docs/engineering/PERF_GATE_CONTRACT.md:5,13` 称「四条**全部为真判红**」 | 阻断（口径三处互斥） |
| 13 | `eng/contracts/schemas/pipeline_block.schema.json` (88) | 全读 | **无 `additionalProperties` 键**（draft-07 默认 true）⇒ 根与 items 全开放；`:15` `blocks` 无 `minItems` ⇒ `[]` 绿。与 `PIPELINE_BLOCK_CONTRACT.md:24` 称字段集「冻结字段」矛盾 | 须修 |
| 14 | `eng/contracts/schemas/projection_registry.schema.json` (70) | 全读 | `:25-31` `frozen_set` 有 `minItems:8`/`maxItems:8` 但**无 `uniqueItems`** ⇒ `["TAN"×8]` 合法，7/8 冻结投影可被静默丢弃；`:27` 声称「顺序冻结」在 JSON Schema 中**根本不可表达**；`:7` 权威 `docs/detail/algorithms_phase3/14_projection.md` **不存在** | 阻断 |
| 15 | `eng/contracts/schemas/monitor_field_semantics.schema.json` (56) | 全读 | 全文**无 `additionalProperties`**（开放）；实质约束全是 `const`：`:24` `semantics="enforced"`、`:27` `on_missing_evidence="red"`、`:45` `on_unclosed_change="red"`。**这是纯自证文件**——用「JSON 字符串写着 red」来证明「缺证据会判红」，schema 根本观测不到证据是否缺失 | 阻断（恒真门） |
| 16 | `eng/contracts/schemas/contract_index.schema.json` (53) | 全读 | `:5` description 宣称「唯一 ID…**无悬空引用**」，实际 `contracts` 无 `uniqueItems`、`upstream`/`downstream`(`:25-26`) 是裸字符串数组且**无任何交叉引用检查**、`path` **存在性从不校验** ⇒ 两条承诺**均不可判定** | 须修 |
| 17 | `…/negative/n6_sparse_snr_relative_semantics…json` (46) | 全读 | `:31` `"relative_to_frame_snr_median"` 与 `sparse_snr_layer.schema.json:301` `const "absolute_flux_type_snr"` 冲突 ⇒ **是真负例**（唯一被触发约束 1 条）。但缺陷只能靠**自报 token**发现：写 `absolute_flux_type_snr` 而存相对数值可全绿通过 | 通过（负例有效） |
| 18 | `…/data/examples/phase3_planar_fits_v1.example.json` (43) | 全读 | `:37` signal 面 `units:"ADU"`（无 `/sr`、无 pixel_semantics 声明）——量纲不可判，而 `phase_product_exchange_matrix.json:54` 又未要求 variance 面 | 须修 |
| 19 | `eng/contracts/data/artifact_types.registry.json` (42) | 全读 | `:6` `doc_ref` 指向 `docs/interfaces/data/DATA-001_ARTIFACT_CONTRACT.md` —— **`docs/interfaces/` 整个目录已不存在**（`ls docs/` 只有 ACSD_DESIGN.md / detail / DOCUMENT_INDEX.yaml / engineering / GLOSSARY.md / README.md / science）。本表自称「单一真源」，其语义权威文档已随目录蒸发 | 须修 |
| 20 | `…/unified/examples/variance.example.json` (42) | 全读 | `:6-10` bunit=`ADU^2/sr^2` + `written_px_power` + `target_pixel_area` **自洽**（`ADU^2/sr^2` 真含 sr）；`:32` `variance_value:0.25`；`:38` `oracle_ref` 指向仓内测试符号 | 通过 |
| 21 | `…/data/examples/weight-mode.example.json` (41) | 全读 | `:4` `weight_mode:"point_information"` + `:5` `mode_class:"production"` 与注册表 A44 作废口径一致；`:10-14` `legacy_superseded` 以**登记面**形态保留（合规）；`:8` 把已冻结 `psf_snr_power` 列进 `deferred_modes_documented` 而非生产集 | 通过 |
| 22 | `…/unified/examples/point_information.example.json` (37) | 全读 | `:6` bunit=`ADU^-2` + `:8` `pixel_semantics:"dimensionless"` + `:9` `pixel_area_power:0` **自洽**；`:31` `W_info:0.0125` 与 `:32` `variance_of_flux:80.0` —— **Var=1/W 成立**（1/0.0125=80.0，精确）⇒ 该例通过了契约最核心的不变量 | 通过 |
| 23 | `…/negative/n2_source_snr_as_variance…json` (36) | 全读 | `:2-3` 自报 `source_snr`；**真正的目标 schema 是 `variance`**（绑定写在 `negative/EXPECTED.json:10`）⇒ 对 `variance.schema.json` 触发 const/required/additionalProperties 多条。是真负例 | 通过 |
| 24 | `…/unified/examples/frame_snr.example.json` (35) | 全读 | `:6` bunit=`"1"` + `:7` `written_px_power` —— 与 validity 同类**语义为假**声明（BUNIT `"1"` 不含 sr）；`:31` `frame_snr_value:118.4`，`:34` `role:"unique_frame_reference"` | 须修 |
| 25 | `…/negative/n3_coverage_as_rejection…json` (35) | 全读 | `:2-3` 自报 `coverage`；真正目标是 `rejection`（`EXPECTED.json:18`）⇒ 触发 `rejection.schema.json` 的 `required pollution_inference` 等。是真负例 | 通过 |
| 26 | `…/data/examples/provenance.example.json` (32) | 全读 | `:9` units 与 `:11` `pixel_semantics` **一致**（surface_brightness / -2）；`:21` `flux_conservation_factor:1.0`（与 FZ-COND-FLUX-CONSERV「恒 1」一致）；`:26` 标定脚本 `run/quality/upm/kcorr_calib.py` 为仓内路径引用 | 通过 |
| 27 | `…/data/examples/phase3.example.json` (31) | 全读 | `:4` `output_mode:"point_source_flux"` + `:5` `measurement_capable:true` 组合合法（非 visualization）；`:19` 显式写出 `Sum c_k^2 != 1`（bilinear 方差非简单传播）——**科学上正确且诚实** | 通过 |
| 28 | `eng/contracts/README.md` (22) | 全读 | `:3` 自称「机器校验的合同 schema 唯一事实源：门禁与检查器**直接读取**这里的 JSON Schema 与登记表」；`:13-18` 目录清单未列 `anchors/`、`ledgers/`、`data/examples/` | 建议 |
| 29 | `eng/contracts/schemas/version.schema.json` (20) | 全读 | `:13` `prerelease const "alpha"` 与 `:11` pattern 强制 `-alpha.N` ⇒ **发布 1.0.0 当天该 schema 恒红**（latent，非今日缺陷）；`:17-18` `abi_version`/`cli_schema_version` 裸 string（`""` 过）且与 `cpu_profile.schema.json:465`（integer, minimum 1）**三处口径不一** | 须修 |

---

## 4. 发现清单

### 4.1 阻断（5 条）

**B1 · 条款注册表的溯源绑定大面积伪引，且数值在文档层无第二来源**（本人原创发现）
- 46 条带 `source_binding` 的条款中，**23 条 `contains` 字符串在被引文档中逐字不存在，42 条 `locator` 不存在**。
- 复核命令：
  ```bash
  grep -c "PSFSW-T-DEPTH" docs/science/PSF_SIGNAL_WEIGHT.md   # → 0
  grep -c "0\.05"            docs/science/PSF_SIGNAL_WEIGHT.md   # → 0
  grep -c "alpha"            docs/science/PSF_SIGNAL_WEIGHT.md   # → 0
  grep -c "FZ-AP2S-KAPPA-MAX\|1e6" docs/science/algorithms/UPM_SOLVER.md  # → 0
  grep -c "max(f_p, 0.1)"    docs/science/DATA_SEMANTICS.md      # → 0
  ```
- `PSFSW-T-*` 的 17 个阈值 ID 在注册表声明的语义权威 `docs/science/DATA_SEMANTICS.md:3370-3374` 中**只有 ID 列表、没有数值**；其原值文档 `PSFSW_FROZEN_THRESHOLDS.md` 按注册表**自己**的 `reanchor_map:4110-4112` 已「retired」。`eng/tests/unit/p1_psfw/p1psfw_tests_gates.cpp` 里也没有这些数（grep `0.05|depth_scan_max_rel_dev|kRho` → 0 命中）。
  ⇒ **`PSFSW-T-*` 的 17 个阈值数值全仓只存在于注册表自身。**
- 为何致命：这些条款正是 `PENDING_OWNER_SIGNOFF`（非 `OPEN`）的唯一理由。按 `DATA_SEMANTICS.md:3357`，该状态定义为「**值与文本唯一确定**，但…签字后生效」。若「值」在文档层无出处，「唯一确定」不可证。**整条状态分类的地基失据。**
- 位置：`eng/contracts/data/clause_registry.json:953-960`（首个实例）、`:1490-1509`、`:1553-1588`；`docs/science/DATA_SEMANTICS.md:3357`。
- 诚实交代一处**我差点误判**：`PSFSW-COMPOSITE-*` 5 条的 `contains`（`alpha=2` 等）不在文中，但 `PSF_SIGNAL_WEIGHT.md:54` 确实写了 `α=2, β=1, γ=2, δ=1, C_norm=1.0` —— **数值正确，只是希腊字母 vs ASCII 的转写不匹配**。这 5 条降级为「绑定不可逐字核验」，**不是数值错误**。

**B2 · 「锚不得悬空」门的函数体只检查了一半的锚**（本人原创发现）
- `eng/tests/contracts/product_family/test_field_constraints_integration.py:176-186`，文档串：「49 条待签条款的锚不得悬空：source_binding.file 必须存在」。
- 实际只做 `sb["file"]` 且只对 `f.startswith("docs/")`。**`anchor` 字段从不检查。**
- 结果：**20 条 `PENDING_OWNER_SIGNOFF` 条款的 `anchor` 指向注册表自己声明「已出库」的文档**（`PSFSW_FROZEN_THRESHOLDS §3/§5/§5.2/§5.3`、`PSFSW_BASELINE_COMPARISON §5`、`ALG-P2-SURF-PIXIVAR-GATE §3`、`ALG-P2-SURF-UPM §3/§4/§6`、`ALG-P2-SURF-GLS §2`、`ALG-P2-SURF-COVARIANCE-EPSF §5`、`ALG-P2-SURF-REJECTION §4`）。复核命令见 §8。
- 为何是阻断：门**因文件存在而常绿**，把 20 条真悬空锚藏在绿灯里 —— 正是「局部绿掩护真红灯」形态。
- 附带：`clause_registry.json:14` 特意声明「上列四件已出库；原 docs/ 前缀与目录一并省略，**避免在扫描面内产生新的悬空 docs/ 指针**」，而同一文件随后就在 `anchor` 里留下 20 条悬空指针。**该自我约束被自身违反。**

**B3 · `examples_index` 全错且零消费者；`file_migrations` 全错而门只数个数**
- `examples_index` 10 条全指向 `eng/contracts/data/examples/v6/`，该目录不存在；**真实文件就在少一层的 `eng/contracts/data/examples/`**（`covariance/effective-psf/phase3/point-information/provenance/psf/psfsw/signal/units/weight-mode.example.json` 逐个在位）。这是**路径前缀陈旧一层**，不是文件丢失 —— 更隐蔽，因为它指向的名字全都「像是存在」。
- 且 `examples_index` 在全仓**零消费者**（grep 仅命中注册表自身第 3586 行）。
- `migration_map.file_migrations` 的 10 条 `from`（`eng/contracts/proposals/v6/data/…`）+ 10 条 `to`（`eng/contracts/schemas/v6/…`）+ `v6_data_dictionary_v1.json` **共 21 条路径不存在**（两个目录都不存在）。
- 唯一读它的门 `field_constraints_oracle.py:630-648` 断言 `n_schema_mig == 10` —— **只数条目，从不打开路径**。配套 mutation `test_field_constraints_mutations.py:139-140` 删掉 signal 条目（10→9）被抓，**给人「该门能抓迁移表完整性」的错觉**，实则它只能抓计数漂移。

**B4 · rejection / support / validity 三份 schema 的 `declared_via_provenance` 恒红、`target_pixel_area` 门恒绿**（本人对 validity 逐行复核确认）
- `validity.schema.json:78` `bunit const "1"` ⇒ `:102-125` 的 `pattern "/sr"` 恒不触发；`:127-149` 强制 `pixel_semantics const "surface_brightness"` 与 `:89` `const "count"` 互斥 ⇒ 该分支**永不可满足**；`:94-98` 的 `exclusiveMinimum:0` 因此**永不可达 = 恒绿**。
- 唯一合法值 `written_px_power` 的自述（`:82`）「BUNIT 显式含立体角幂次（canonical "sr"）」在 `bunit="1"` 下**语义为假**，而 schema 强制要求它为真。
- ⚠️ **项目已自行登记**：`eng/contracts/UNRESOLVED_REGISTER.md:3992-4002`（78.1 条，点名「同块逐字复制进 coverage / rejection / support / validity 四份 schema ⇒ 一处错、四处同错」），**HEAD 上仍未修**。本人确认该登记准确，且点名文件与本片成员重合度 3/4。
- `signal.schema.json` 是唯一幸免者（`:451-490` 强制 `bunit="ADU"` + 自由 enum）。

**B5 · `monitor_field_semantics.schema.json` 是纯自证文件（恒真门）**
- 全文无 `additionalProperties`；实质约束全为 `const`：`:24` `"enforced"`、`:27` `on_missing_evidence:"red"`、`:45` `on_unexplained_change:"red"`。
- **它用「JSON 字符串写着 red」来证明「缺证据会判红」。schema 无法观测证据是否缺失** —— 被检量与期望量同源，典型的自洽式断言。
- `:49-54` `declared_checks` 可选且无 `minItems` ⇒ **声明零个检查是绿的**。
- `:29-31` 的 `monitor_capable` 是孤儿值，且与权威文档 `docs/engineering/PERF_GATE_CONTRACT.md:45`「不改名为 `monitor_capable`」**直接矛盾**。

### 4.2 须修（10 条，摘要）

| # | 发现 | 定位 |
|---|---|---|
| M1 | `resource_gate_v1.json:8-9` 的 readers 路径写作 `cli/resource_gate.h`，**该路径不存在**（真实 `lib/infrastructure/cli/resource_gate.h`）；`:6` 自称唯一数值源，而消费侧确有硬编码字面量 | `eng/contracts/resource_gate_v1.json:6,8-9` |
| M2 | 资源门口径三处互斥：`PERF_GATE_CONTRACT.md:5,13`「四条全部为真判红」/ `perf_gate_criteria.schema.json:57` `on_violation const "red"` / `resource_gate_v1.json:44,46,49` 实为 `record_and_justify`，`:58`「不改变退出码」 | 同左 |
| M3 | 85/90/0.70 三阈值以**分数**（md:11-13）、**百分数**（json:43,45,47,48）、**枚举 token 名内嵌字面量**（`perf_gate_criteria.schema.json:39` `"frac_windows_ge_0.85"`）三形态并存，无机器绑定 ⇒ 漂移不可见 | 同左 |
| M4 | `hard_fail_criteria[0]` 的判定数 `1.2` 硬编码在消费侧，注册表无此键 —— 违反 `:6`「实现侧不得再出现字面量阈值」 | `resource_gate_v1.json:84`；`eng/tools/quality/resource_monitor.py:88` |
| M5 | `artifact_types.registry.json:6` `doc_ref` 指向 **`docs/interfaces/` 整目录已删**；`projection_registry.schema.json:7` 权威 `docs/detail/algorithms_phase3/14_projection.md` 不存在 | 两处 `:6` / `:7` |
| M6 | `projection_registry.schema.json:25-31` `frozen_set` 有 `minItems/maxItems:8` 但**无 `uniqueItems`** ⇒ `["TAN"×8]` 合法；`:27` 声称「顺序冻结」在 JSON Schema 中不可表达；`:32-34` `projections` 无 `minItems` ⇒ `[]` 绿 | `:25-34` |
| M7 | `contract_index.schema.json:5` 宣称「唯一 ID…无悬空引用」，两者**均未强制**（无 `uniqueItems`、`upstream/downstream` 无交叉引用、`path` 存在性不查） | `:5,12-13,25-26` |
| M8 | `run_manifest.schema.json:119-122` `storage` 声称校验「由该 $defs 承载」却**无 `$ref`**，且「缺失 ⇒ 不判红」—— fail-open 伪装成委派校验 | `:119-122` |
| M9 | `pipeline_block.schema.json` 全文件**无 `additionalProperties`**（draft-07 默认开放），与 `PIPELINE_BLOCK_CONTRACT.md:24`「冻结字段」矛盾；`:15` `blocks` 无 `minItems` | `:6,15,18` |
| M10 | `jsonl_event_v1.schema.json:490` `additionalProperties:true`；`:388-390` 三个 `required` 字段未在 `properties` 声明 ⇒ 只约束存在不约束取值 | `:490,388-390` |

### 4.3 建议（6 条）

- S1：`phase_product_exchange_matrix.json:62,71,80,89,98,107` 的 `to_role`/`from_role` 端点（`phase2_input`/`phase3_input`/`external_consumer`/`external_fixture`）在 `:26-57` `roles[]` 中**一个都不存在** ⇒ 每条边端点悬空。
- S2：`phase_product_exchange_matrix.json:44,54` phase2/phase3 的 `min_planes` 缺 `variance`/`ivar`（phase1 `:34` 有）⇒ P2/P4 的 SNR 产物无法跨阶段边界。
- S3：`phase3_planar_fits_v1.example.json:37` signal 面 `units:"ADU"` 无 `/sr`、无 pixel_semantics ⇒ 量纲不可判。
- S4：`signal.schema.json:90-100` 的 `dimensionless`/`count`/`probability`/`magnitude` 四值因 `product_family` 必填 + allOf 覆盖而**结构不可达**。
- S5：`version.schema.json:11,13` 强制 `-alpha.N` ⇒ 1.0.0 发布当天该 schema 恒红（latent）；`:17-18` `abi_version` 裸 string 与 `cpu_profile.schema.json:465`（integer, min 1）三处口径不一。
- S6：`hardware_inspect.schema.json:3` `$id` 用 RFC-2606 保留 TLD `acsd.invalid`；`:145-147` 必填 `quota_signature` 裸 string（`""` 过）；`:80-83` `affinity_count` 与 `affinity` 长度无绑定。

---

## 5. 我主动构造的反例

### 反例 1（**推翻成功 · 本片最强**）：把 20 条锚指向已删除文档，门仍绿
- **构造**：取 `PSFSW-COMPOSITE-ALPHA`（`clause_registry.json:1365-1388`）。其 `source_binding.file = docs/science/PSF_SIGNAL_WEIGHT.md`（**存在**）、`anchor = "PSFSW_FROZEN_THRESHOLDS §5"`（**该文档按注册表 `reanchor_map:4110` 自己声明已出库**）。
- **期望推翻什么**：推翻「`test_clause_anchors_reanchored_to_living_docs` 守着锚点不悬空」这一承诺。
- **是否推翻**：**推翻成功**。该门（`test_field_constraints_integration.py:176-186`）只查 `sb["file"]`，`anchor` 字段从头到尾没被读过 ⇒ 20 条待签条款的 `anchor` 全部悬空而门绿。全片 20 条，不是孤例。

### 反例 2（**推翻成功**）：把注册表数值源整个搬到无人认领的文档，绿
- **构造**：质疑「`PSFSW-T-DEPTH=0.05` 有文档出处」。
- **是否推翻**：**推翻成功**。`grep "PSFSW-T-DEPTH" docs/science/PSF_SIGNAL_WEIGHT.md` → 0；`grep "0.05" …` → 0；原文档 `PSFSW_FROZEN_THRESHOLDS.md` 已出库；语义权威 `DATA_SEMANTICS.md:3370-3374` 只有 ID 无值；C++ 门里也没有。**该数值全仓只存在于注册表自身**，而注册表同时是「被检量」与「期望量」。

### 反例 3（**推翻成功**）：给 `validity` 构造一个合法 `declared_via_provenance` 实例
- **构造**：`{"bunit":"1","bunit_semantics":"declared_via_provenance","pixel_semantics":"count","pixel_area_power":0,"target_pixel_area":1.0}`。
- **期望推翻什么**：推翻「`declared_via_provenance` 是可达分支」。
- **是否推翻**：**推翻成功**。`allOf[1]`（`:127-149`）强制 `pixel_semantics const "surface_brightness"`，与基础层 `:89` `const "count"` 直接冲突 ⇒ 无解。改 `pixel_semantics` 为 `surface_brightness` 又违反 `:89`。**该分支恒红，其 `target_pixel_area` 门恒绿。**

### 反例 4（**推翻失败 — 登记表是诚实的，我如实记录**）
- **构造**：核对 `counts.clauses_total=96` / `frozen:39` / `pending:49` / `open:8`。
- **是否推翻**：**未推翻**。本人独立数出 `clauses[]` 长度 **96**，状态分布 **39/49/8**，`clause_registry.counts_by_status` 全部吻合，`ids_by_status` 三组清单**逐一对应、无重复 ID、无遗漏**。
- **结论**：注册表的**自述计数是真话**。这一点必须如实写明 —— 它证明本片的缺陷**不是**「登记表整体不可信」，而是集中在**溯源指针层**（`source_binding` / `anchor` / `examples_index` / `file_migrations`）而**不在计数层**。

### 反例 5（**推翻失败 — 负例有效**）
- **构造**：质疑 n6 是不是「schema 其实收它」的假负例。
- **是否推翻**：**未推翻**。`n6:31` `"relative_to_frame_snr_median"` 与 `sparse_snr_layer.schema.json:301` `const "absolute_flux_type_snr"` 冲突，且是**唯一**被触发约束、无附带误差 ⇒ 干净的真负例。

---

## 6. 盲复算（遮住既有判定独立取证）

**方法**：对 4 处「可机械判定」的点，先不看任何既有审稿结论，独立重算，再与仓内既有判定比对。

| 复算项 | 我的独立结果 | 与仓内既有判定 | 判定 |
|---|---|---|---|
| `clauses[]` 条数与状态分布 | 96 / FROZEN 39 / PENDING 49 / OPEN 8，ID 无重复 | 仓内无相反记载 | **一致**（且我这一侧是新的独立佐证） |
| `source_binding.contains` 逐字存在性 | 46 条中 23 条不存在 | 无既有审稿记录此项 | **偏松**（既有审稿未发现此层） |
| `source_binding.locator` 逐字存在性 | 46 条中 42 条不存在 | 无既有审稿记录此项 | **偏松** |
| `examples_index` 路径存在性 | 10/10 不存在（真实位置少一层目录） | 无既有审稿记录此项 | **偏松** |
| `migration_map` 路径存在性 | 21 条不存在 | 无既有审稿记录此项 | **偏松** |
| validity `declared_via_provenance` 可满足性 | 恒红（本人逐行复核） | `UNRESOLVED_REGISTER.md:3992-4002` 已登记 | **一致** |
| `counts` 自述真伪 | **真** | — | 我比仓内更**正面**（无人质疑过它） |

**结论：既有判定对本片整体偏松。** 漏掉的是整个「溯源指针层」——`source_binding` 的 65 处伪引、`anchor` 的 20 条悬空、`examples_index`/`file_migrations` 的 31 条死路径。既有判定命中的只有 `validity/rejection/support` 的恒红恒绿一项（且那还是项目自己登记的，不算审稿新增）。

---

## 7. 子代理派发记录

**派发 6 次，覆盖 4 个互不重叠的范围**（其中 2 次是我在同一消息块内误发的重复派发，已尝试 `job_kill` 但其已完成；如实记录，不掩饰）：

| # | 子代理 | 范围 | 覆盖 | 结论 |
|---|---|---|---|---|
| 1 | `bb44d8a0` | 3 份 negative 夹具（n2/n3/n6） | 117 行 100% | 三个都是**真负例**，0 阻断 |
| 2 | `a8a1a3b0` | 7 份正例夹具 | 未回收（见下） | — |
| 3 | `38aed810` | signal/rejection/support/validity + 2 份登记表 | 1728 行 100% | 4 阻断 + 7 须修 |
| 4 | `0d15ab69` | 10 份平台 schema + resource_gate | 1490 行 100% | 7 阻断 |
| 5 | `280172b3` | 同 #1（**误发重复**） | 117 行 100% | 与 #1 独立互证，并新增 F-3 |
| 6 | `b38049a0` | 同 #4（**误发重复**） | 1490 行 100% | 与 #4 独立互证，并新增 B1/B3/B5 |

**逐条复核与否决**

- **全部采纳（我亲自复核后确认）**
  - `38aed810` 的 **B1**（rejection/support/validity 恒红）—— 本人在 `validity.schema.json:78/89/94-98/102-149` 逐行复核，**确认成立**，并升级为本片 B4。
  - `bb44d8a0` 的 **F2 / `280172b3` 的 F-2**（n6 未进 `docs/engineering/UNIFIED_OBJECTS.md` 索引，而该文档 `:90` 却在讨论 n6 守的缺陷、`:105` 点名两道门）—— 两份独立报告互相印证，**采纳**。
  - `280172b3` 的 **F-1**（「relative 语义只能靠自报 token 发现」）—— 我核实 n6 全文：`sparse_snr_semantics` 是唯一判据，无任何数值交叉校验，**采纳**，写入 §5 反例 5 的补注。
  - `b38049a0` 的 **B4**（消费侧硬编码阈值违反 `resource_gate_v1.json:6` 的唯一数值源声明）—— 与 `0d15ab69` 的 H-3/H-4 独立同指，**采纳**为 M1/M4。

- **降级 / 部分否决**
  - `38aed810` 的 **B4（phase2/phase3 缺 variance 面）** —— 观察本身成立，但**严重性由我下调**：phase3 是否本就设计为不接收逐像素不确定度，属设计意图问题，仓内证据不足。降为 S2 建议。
  - `0d15ab69` 的 **F11**（`max_abs_crval_dec_deg` 描述称 85.0、schema 允许到 90）—— **部分否决**：子代理自承未打开 `p3_proj.cpp` 确认实现实际发出的值。若实现确实 ≤85.0，则仅为文档漂移而非活门漏洞。**降为不单列**（并入 M6 的附注）。
  - `0d15ab69` / `b38049a0` 的 **amd64 `const` / alpha-only pattern** —— 两位子代理**均自承这是 latent 而非活缺陷**（`AGENTS.md §11` 项目处于 alpha 前、amd64 平台面已声明封闭）。我**接受其自我降级**，列为 S5/S6 建议，不计阻断。

- **我否决的子代理结论**
  - `38aed810` 的 **M7**（`role:"probability"` 未强制 `rejection_probability` 必填）—— **否决**。子代理自承「最不确定」，其论证依赖把 `role` 读作语义断言而非描述性元数据；仓内无支持该读法的条款。按无罪推定降为不列。
  - `b38049a0` 的 **B3**（C++ 只实现队列饥饿 3 条合取中的 1 条）—— **否决为阻断**。子代理自己即已降级：`in_process_hard_fail` 为 `"retired"`、`resource_gate.h` 无条件返回 `RecordOnly`，故该分歧只改日志串、**不改裁决**。这是规格-实现漂移，不是红绿危害。降为不单列。

- **未回收**：`a8a1a3b0`（7 份正例夹具专项）在本轮结束前未返回报告。**该范围本人已自行读完 7 份中的 6 份**（weight-mode / provenance / phase3 / phase3_planar_fits_v1 / variance / point_information），第 7 份 `frame_snr.example.json` 亦已读完，结论见 §3 第 17–27 行。不因此留下覆盖缺口，但**该专项的独立交叉验证缺失**，如实登记。

---

## 8. 自证段（可复跑命令）

全部只读，均不写仓内文件、不跑仓内测试。基线 HEAD = `850a9ede`。

```bash
cd "/workspace/Astro CS Database"

# 0. 本片成员与行数（应得 29 / 8175）
sed -n '969,1005p' run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml

# 1. B2 反例1：20 条 anchor 指向注册表自己声明已出库的文档
python3 -c "
import json,re
d=json.load(open('eng/contracts/data/clause_registry.json'))
retired={e['retired'] for e in d['reanchor_map']['entries']}
n=0
for c in d['clauses']:
    a=c.get('anchor','')
    for s in retired:
        t=s.rstrip('.md')
        if re.search(r'(?<![A-Za-z0-9_])'+re.escape(t)+r'(?![A-Za-z0-9_])',a):
            n+=1; print(c['id'],'|',a); break
print('TOTAL',n)"
# 预期：TOTAL 20，且 20 条 status 全为 PENDING_OWNER_SIGNOFF

# 2. B2 守门函数体（证明它只查 source_binding.file、且只对 docs/）
sed -n '176,186p' eng/tests/contracts/product_family/test_field_constraints_integration.py

# 3. B1 伪引：contains / locator 是否在被引文档中逐字存在
python3 -c "
import json,os
d=json.load(open('eng/contracts/data/clause_registry.json')); c={}
for x in d['clauses']:
    sb=x.get('source_binding')
    if not isinstance(sb,dict): continue
    f=sb['file']
    if f not in c: c[f]=open(f,encoding='utf-8').read() if os.path.exists(f) else None
    t=c[f]
    if t is None: continue
    if sb.get('contains') not in t: print('CONTAINS-MISS',x['id'],repr(sb.get('contains')),f)
    if sb.get('locator') not in t: print('LOCATOR-MISS',x['id'],repr(sb.get('locator')),f)
" | wc -l
# 预期 65 行（23 contains-miss + 42 locator-miss）

# 4. B1 决定性一击：被引文档里根本没有这些数
grep -c "PSFSW-T-DEPTH" docs/science/PSF_SIGNAL_WEIGHT.md          # 0
grep -c "0\.05"          docs/science/PSF_SIGNAL_WEIGHT.md          # 0
grep -c "alpha"          docs/science/PSF_SIGNAL_WEIGHT.md          # 0
grep -c "1e6"            docs/science/algorithms/UPM_SOLVER.md      # 0
grep -c "max(f_p, 0.1)"  docs/science/DATA_SEMANTICS.md             # 0
sed -n '3370,3374p' docs/science/DATA_SEMANTICS.md   # 只有 ID 列表，无数值

# 5. B3 examples_index 错一层目录（10 条全 MISS，真文件少一层 v6/）
python3 -c "
import json,os
d=json.load(open('eng/contracts/data/clause_registry.json'))
for p in d['examples_index']: print(('OK  ' if os.path.exists(p) else 'MISS'),p)"
ls eng/contracts/data/examples/     # 10 个同名文件都在这里

# 6. B3 examples_index 零消费者
grep -rn "examples_index" eng/ lib/ docs/   # 仅命中注册表自身 3586 行

# 7. B3 file_migrations 21 条死路径，而门只数个数
python3 -c "
import json,os
m=json.load(open('eng/contracts/data/clause_registry.json'))['migration_map']
n=0
for e in m['file_migrations']:
    for k in ('from','to'):
        if not os.path.exists(e[k]): n+=1; print('MISS',k,e[k])
print('TOTAL',n)"          # 预期 21
sed -n '630,648p' eng/tests/contracts/product_family/field_constraints_oracle.py   # 只断言 n==10
sed -n '139,140p' eng/tests/contracts/product_family/test_field_constraints_mutations.py

# 8. B4 validity 恒红/恒绿（本人逐行复核）
sed -n '77,98p'  eng/contracts/schemas/unified/validity.schema.json
sed -n '102,149p' eng/contracts/schemas/unified/validity.schema.json
grep -n '"pixel_semantics"' eng/contracts/schemas/unified/validity.schema.json
# 项目已自行登记：
sed -n '3992,4002p' eng/contracts/UNRESOLVED_REGISTER.md

# 9. M5 整目录已删 / M1 readers 路径错
ls docs/                        # 无 interfaces/
python3 -c "import os;print(os.path.exists('docs/interfaces/data/DATA-001_ARTIFACT_CONTRACT.md'))"
python3 -c "import os;print(os.path.exists('docs/detail/algorithms_phase3/14_projection.md'))"
python3 -c "import os;print(os.path.exists('cli/resource_gate.h'))"   # False；真实 lib/infrastructure/cli/resource_gate.h

# 10. B5 monitor_field_semantics 纯自证
sed -n '23,31p' eng/contracts/schemas/monitor_field_semantics.schema.json
grep -c "additionalProperties" eng/contracts/schemas/monitor_field_semantics.schema.json   # 0
grep -c "additionalProperties" eng/contracts/schemas/pipeline_block.schema.json            # 0

# 11. M6 frozen_set 无 uniqueItems
sed -n '25,34p' eng/contracts/schemas/projection_registry.schema.json

# 12. 计数自证（反例4：登记表的计数是真话）
python3 -c "
import json;from collections import Counter
d=json.load(open('eng/contracts/data/clause_registry.json'))
print('actual',len(d['clauses']),Counter(x['status'] for x in d['clauses']))
print('declared',d['counts'])"

# 13. 零修改自证
git -c core.quotepath=false status --porcelain -- eng/contracts/    # 空
```

---

## 9. 口径声明（按纪律第 9 条）

本片**不涉及**门实例 / 去重门 / 整改分母三层的计数 —— 本片 29 个成员全部是**合同 schema、登记表与示例夹具**，不含判据实现、不含归档实验结果，故无「门实例数」可报。

本片中出现的数量一律标明层级：

- **条款层**：`clauses[]` = 96 条（FROZEN 39 / PENDING_OWNER_SIGNOFF 49 / OPEN 8）—— 已独立复核，与注册表自述一致。
- **绑定层**：`source_binding` = 46 条（其中伪引 23 条 `contains` + 42 条 `locator`，两者有重叠，合计 65 处失配）。
- **悬空指针层**：锚 20 条 + examples_index 10 条 + file_migrations 21 条 = **51 条**。
- **schema 约束层**：恒真/恒绿判定按「可达性」逐条给，不做汇总去重。

---

**结论复述**：本片判定 **需修（倾向阻断）**。最强产出是**本人原创发现、既有审稿全部漏掉**的溯源指针层系统性失效：`source_binding` 65 处伪引 + `anchor` 20 条悬空 + 索引 31 条死路径，而唯一的相关判据（守锚门、迁移门）分别只查一半锚、只数条目个数，**结构上不可能因此转红**。这正是本轮要找的形态：**被判据"保护"的对象，其保护判据本身恒绿。**
