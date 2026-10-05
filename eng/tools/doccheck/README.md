# doccheck

文档面检查工具。工具是帮人**查**的，不是判据：退出码恒为 0，不进默认构建，不被任何门禁调用，不自动改任何文件。计数只作分母，判定由人阅读上下文做出。

| 工具 | 做什么 |
|---|---|
| `check_assert_dup.py` | 断言副本检测器。输入一条断言的特征（公式片段、常数、判据句、标识符），输出它在全仓哪些文件出现、每处所处文档层次、逐字是否一致，以及需要人裁决的分歧点。 |

## 断言副本检测器

五种查询模式，各自针对一种「同一断言散落多处」的形态：

| 模式 | 抓什么 | 典型用法 |
|---|---|---|
| `--text` | 逐字子串 | `--text "w = SNR²/F_ref²"` |
| `--regex` | 正则锚点（只作定位） | `--regex "row_sum_tol\|bunit_tol"` |
| `--ident` | 标识符折叠匹配，抓大小写混合的类型名 | `--ident frame_snr` |
| `--num` | 数值近邻，抓同一常数的旧值/截断值/近似值 | `--num 1.482602218505602` |
| `--strength` | 强度词 + 同段限定词共现 | `--strength 恒 --span 2` |

分层判定按项目权威链：顶点（`docs/ACSD_DESIGN.md`）→ 一级正本（`docs/science/`、`docs/engineering/`）→ 二级细节（`docs/detail/`）→ 登记册（`docs/DOCUMENT_INDEX.yaml`、`docs/GLOSSARY.md`）→ 合同面/配置面（`eng/contracts/`、`eng/packaging/config/`）→ 代码（`lib/`）→ 实验单元（`实验/`）。

输出含**口径块**（搜索根、文件过滤、行形态过滤、查询原文、大小写策略、计数单位），离开它计数不可比，禁止跨口径配 before/after。

## 已知限制（写在工具里，也写在这里）

- 逐字匹配看不见大小写混合的类型名，也看不见不含该子串的行——断言被改写过（换符号、换单位、换记法、拆句、只留结论不留公式）时完全查不到。用 `--ident`、`--num` 换检索形态，仍会漏。
- 「同名不同义」工具只把分歧的上下文摆出来，不判是不是同义。判成重复而误伤，本项目已实测发生过。
- 数值近邻尤其易误报：刻意的近似标注、有意的负例示范、历史记录都会命中。
- 恒真门/恒红门（判据与被检量同源）不在能力内，需要符号求值。
- 工具扫的是**工作树**。`docs/engineering/build/` 被 `.gitignore` 的 `build/` 规则排除，干净克隆上整目录不存在——只在工作树扫会把「已如实登记」误判为已闭合。
- 工具**会扫到自己的 README 与夹具文件**。若断言特征恰好写在 `eng/tools/doccheck/` 里（例如作为用法示例），命中数会虚增。排除法：`--exclude eng/tools/doccheck/`（A/B 两个口径的数字不同，**不可混用**）。
- `--strength` 与 `--ident --prefix` 是**候选流不是判据**：实测前者 2031 处（限定词表含「若」「当且仅当」等高频词，接近恒绿），后者 467 处（把 `bilinear_regular_grid_v1` 等一并吸进来）。把它们的命中数当判定依据，就是本项目已实测踩过的「恒真门」。
- `run/` 默认不扫（审核过程记录不是断言正本面），需要时加 `--include-run`。

## 复跑

```sh
python3 eng/tools/doccheck/check_assert_dup.py --selftest
python3 eng/tools/doccheck/check_assert_dup.py --text "w = SNR²/F_ref²"
python3 eng/tools/doccheck/check_assert_dup.py --num 211076.28514206142
python3 eng/tools/doccheck/check_assert_dup.py --ident bilinear --prefix
python3 eng/tools/doccheck/check_assert_dup.py --text "…" --format json
```

每条查询的输出末尾都会打印它自己的复跑命令。