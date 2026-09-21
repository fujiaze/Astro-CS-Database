# reverse_verify 工作区迁移登记（ROOT-CONSOLIDATION，2026-09-21）

> 上游：`ENGINEERING_SPEC.md §7`（外部只读数据集与根目录规范）、`ENGINEERING_SPEC.md §9`（仓库整洁与治理工件清理）
> 依据：负责人 2026-09-21 直接指令（根目录整合，选项 D：`reverse_verify/` 按主题拆入 `实验/SCI-A/B/C` 对应单元，根条目删除）

`reverse_verify/`（146 tracked 文件 / 4.4 MB）原为**逆向验收工作区**：主线从设计权威正向推导，
该工作区独立地从第一性原理与公开文献/开源实现逆向验证主线声称的能力。整合后其内容按主题
归入三个 SCI 实验单元，**根条目删除**；工作原则与 provenance 保留在本文件。

## 1. 迁移映射（旧路径 → 新路径）

| 旧路径 | 新路径 | 依据 |
|---|---|---|
| `reverse_verify/experiments/frame_snr/` | `实验/SCI-B/code/reverse_verify/frame_snr/` | 帧级 SNR 定案 ↔ SCI-B 跨帧绝对 SNR 传递链 |
| `reverse_verify/experiments/p7_noise/` | `实验/SCI-B/code/reverse_verify/p7_noise/` | 噪声项/SNR 标定 ↔ SCI-B |
| `reverse_verify/experiments/snr_design/` | `实验/SCI-B/code/reverse_verify/snr_design/` | SNR 传播设计数值估算 ↔ SCI-B |
| `reverse_verify/experiments/f_instr/` | `实验/SCI-B/code/reverse_verify/f_instr/` | 星点通量口径（进 SNR 分子）↔ SCI-B |
| `reverse_verify/experiments/p1_spatial_gain/` | `实验/SCI-A/code/reverse_verify/p1_spatial_gain/` | 低阶空间乘法增益 ↔ SCI-A 测光星等坐标系 |
| `reverse_verify/experiments/smooth_lambda/` | `实验/SCI-C/code/reverse_verify/smooth_lambda/` | UPM 控制点平滑 λs ↔ SCI-C 加性天光与无接缝 |
| `reverse_verify/experiments/m16_sampling/` | `实验/SCI-C/code/reverse_verify/m16_sampling/` | M16 采样/重建 ↔ SCI-C |
| `reverse_verify/experiments/m16_scene/` | `实验/SCI-C/code/reverse_verify/m16_scene/` | M16 场景前向渲染判据 ↔ SCI-C |
| `reverse_verify/experiments/data_matrix/` | `实验/SCI-C/code/reverse_verify/data_matrix/` | 数据类型矩阵判据 ↔ SCI-C |
| `reverse_verify/synthetic/` | `实验/shared/synthetic/` | 合成数据生成代码，三单元共用 ⇒ 新建 `实验/shared/` |
| `reverse_verify/data/` | `实验/shared/data/` | 真值场景 JSON / 真实实例索引，被共用代码引用 |
| `reverse_verify/docs/frame-snr-canon.md` | `实验/SCI-B/docs/frame-snr-canon.md` | 论文雏形原文（不搬进 docs/ 正式文档） |
| `reverse_verify/docs/f-instr-canon.md` | `实验/SCI-B/docs/f-instr-canon.md` | 同上 |
| `reverse_verify/docs/snr-propagation-design.md` | `实验/SCI-B/docs/snr-propagation-design.md` | 同上 |
| `reverse_verify/docs/p1-spatial-gain.md` | `实验/SCI-A/docs/p1-spatial-gain.md` | 同上 |
| `reverse_verify/docs/smooth-lambda.md` | `实验/SCI-C/docs/smooth-lambda.md` | 同上 |
| `reverse_verify/references/frame-snr-survey.md` | `实验/SCI-B/docs/surveys/frame-snr-survey.md` | 调研记录（见 §4 落位说明） |
| `reverse_verify/references/f-instr-survey.md` | `实验/SCI-B/docs/surveys/f-instr-survey.md` | 同上 |
| `reverse_verify/references/bibliography.md` | `实验/shared/references/REVERSE_VERIFY_BIBLIOGRAPHY.md` | 参考文献（见 §4.3 落位偏离说明） |
| `reverse_verify/references/bibliography.bib` | `实验/shared/references/reverse_verify_bibliography.bib` | 同上（机器可读版） |
| `reverse_verify/references/README.md` | 并入 `实验/shared/references/REVERSE_VERIFY_BIBLIOGRAPHY.md §0`（逐字保留） | 记录规则，避免新建第 3 份 formal doc |
| `reverse_verify/README.md` | 本文件（§2/§3 保留定位与工作原则） | provenance |
| `reverse_verify/CMakeLists.txt` | **退役**（见 §3） | `ENGINEERING_SPEC.md §1`「唯一根 CMake；不引入第二套构建入口」 |
| `reverse_verify/docs/.gitkeep`、`experiments/.gitkeep`、`synthetic/.gitkeep` | **退役** | 空目录占位，随目录解散失效 |
| `reverse_verify/experiments/figs/`（空目录，0 tracked） | **退役** | 空目录 |

## 2. 原工作区的定位与硬边界（逐字保留自旧 `README.md`）

> **主线做正向推导，本工作区做逆向验收。**
> 主线（`lib/` + `docs/` + `tests/`）从设计权威**正向推导**出实现；
> 本工作区**独立地**从第一性原理与公开文献/开源实现**逆向验证**主线声称的能力。

**硬边界（迁移后仍适用）**：

- 迁移后的 `实验/SCI-*/code/reverse_verify/**` **不得**被根 `CMakeLists.txt` 引用；
- 不产出生产代码，不改 `lib/`、`docs/`（`docs/references/` 的两份参考文献除外）、`tests/`、`ci/`；
- 中间产物一律落 `run/reverse_verify/`（gitignore）；仓库内只放代码、报告、参考文献。

## 3. 工作原则（迁移后仍适用）

1. **已知真值**：每个合成实验必须给出真值，并给**能红能绿的负例**（真值为「无效应」时度量必须归零）；
2. **独立复现**：不引用主线结论作为证据；需要时**自己算一遍**；
3. **判据先行**：先写判据与阈值，再看结果；不得事后放宽；
4. **诚实登记**：证据不足写「待定」并给判定方法，不臆断；
5. **可复跑**：脚本 + 数据生成 + 判据三件套齐备，任何人可复现。

### 独立构建（原 `CMakeLists.txt` 的替代）

旧独立构建入口 `cmake -S reverse_verify -B run/reverse_verify/build` 随根条目删除**退役**。
唯一被它登记的 target `rv_p1sg_oracle` 改由等价的最小构建脚本承担（**不新增第二套构建入口**）：

```bash
bash 实验/SCI-A/code/reverse_verify/p1_spatial_gain/build_oracle.sh
run/reverse_verify/p1_spatial_gain/p1sg_oracle        # 退出码 0 = 全部 PASS
```

## 4. 落位说明（对任务卡两处给法的偏离，逐条给理由）

1. **两份 survey 未落 `docs/research/`**：任务卡允许「对应 SCI 单元的 `results/` 或 `docs/research/`」。
   落 `docs/research/` 会把两份**逐条调研记录（点-in-time 文献综述）**升格为正式文档，须在
   `docs/DOCUMENT_INDEX.yaml` 登记 + 加「上游」抬头，且两份 survey 内含 `docs/frame-snr-canon.md`、
   `docs/f-instr-canon.md` 等**迁移后不再存在的 `docs/` 指针** ⇒ 直接判红 DOC-INDEX 门
   （台账 `tools/doccheck/dangling_ledger.json` 为**只减不增**，不得新增条目）。
   故按任务卡总则「论文雏形原文放 `实验/SCI-X/docs/`」落 `实验/SCI-B/docs/surveys/`。
2. **`reverse_verify/references/README.md` 未单独成文**：其内容是「参考文献记录规则」，
   并入 `实验/shared/references/REVERSE_VERIFY_BIBLIOGRAPHY.md §0`（逐字保留），避免为 17 行规则
   新建第 3 份需索引登记的正式文档。
3. **bibliography 未落 `docs/references/`，改落 `实验/shared/references/`**：任务卡要求 bibliography → `docs/references/`。
   实测该落位**无法在提交前通过 DOC-INDEX 门**：`check_doc_index.py` 的 `index_entry_paths_exist` 判据
   取自 `git ls-files`（跟踪集），新建但未 `git add` 的文件一律判「悬空条目」；
   而本任务**零 git 写权限**（禁 add/commit），前台提交前该门必红。同时 `docs/**/*.md` 必须 100% 登记
   （`docs_fully_covered`），故「放 docs/ 但不登记」也不可行。
   ⇒ 落 `实验/shared/references/`（与 canon 同层，provenance 完整）。**前台如需回到 `docs/references/`：
   `git mv 实验/shared/references/REVERSE_VERIFY_BIBLIOGRAPHY.md docs/references/` + `git mv .../reverse_verify_bibliography.bib docs/references/`，
   并在 `docs/DOCUMENT_INDEX.yaml` active 区段补两条（path/status/duty/upstream/downstream/notes）后随同一提交入库即可。**

## 5. 定案结论落位

各 canon/design 文档的**现行结论**已提炼为对应单元的 results 文档：

| 单元 | 定案结论落位 | 论文雏形原文 |
|---|---|---|
| SCI-A | `实验/SCI-A/results/REVERSE_VERIFY_CANON.md` | `实验/SCI-A/docs/p1-spatial-gain.md` |
| SCI-B | `实验/SCI-B/results/REVERSE_VERIFY_CANON.md` | `实验/SCI-B/docs/{frame-snr-canon,f-instr-canon,snr-propagation-design}.md` |
| SCI-C | `实验/SCI-C/results/REVERSE_VERIFY_CANON.md` | `实验/SCI-C/docs/smooth-lambda.md` |

## 6. 运行产物命名空间

迁移**未**改动运行产物路径：全部实验仍写 `run/reverse_verify/**`（gitignore）。
该目录是迁移后实验的**现行输出命名空间**（代码硬编码引用），故在 `run/` 清理中**保留**
（见 `run/ROOT-CONSOLIDATION/RUN_CLEANUP.md`）。
