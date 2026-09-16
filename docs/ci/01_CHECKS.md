# CI 检查项目录（CI Checks Registry）

## 1. 注册表原则

- `ci/checks.json` 是唯一检查注册表；`ci/` 提供确定性执行器；
- 每项检查必须可"能绿能红"（有正例与负例）；
- 豁免显式登记 `ci/exemptions.json`，只减不增，需负责人批准。

## 2. 检查项清单

| ID | 类别 | 名称 | 命令/入口 | 门禁 |
|---|---|---|---|---|
| CHK-BUILD-LINUX | 构建 | Linux Release 构建 | `cmake -S . -B build && ninja -C build` | P0 |
| CHK-BUILD-WIN | 构建 | Windows Release 构建 | VS 工具链 + CMake preset | P0 |
| CHK-FMT | 静态 | 格式检查 | `clang-format --dry-run --Werror` | P0 |
| CHK-WARN | 静态 | 编译警告 | MSVC /W4 /WX · GCC -Wall -Werror | P0 |
| CHK-STATIC | 静态 | 静态分析 | 选定分析器（clang-tidy 等） | P1 |
| CHK-MODULE-MANIFEST | 文档一致性 | 模块 manifest/注册表/构建 target/产品清单一致 | ci 检查器 | P0 |
| CHK-CONTRACT-REF | 文档一致性 | 端口引用有效 DATA 合同 | ci 检查器 | P0 |
| CHK-SCI-REF | 文档一致性 | 算法引用有效 SCI/ALG | ci 检查器 | P0 |
| CHK-CONTRACT-TEST | 文档一致性 | 核心合同有独立测试 | ci 检查器 | P0 |
| CHK-AGENT-HARD-RULES | 文档一致性 | AGENTS.md 硬禁令存在 | ci 检查器 | P0 |
| CHK-DANGLING | 文档一致性 | 删除/重命名无悬空引用 | ci 检查器 | P1 |
| CHK-STALE-DOC | 文档一致性 | 活动文档无陈旧版本号/历史状态冒充 | ci 检查器 | P1 |
| API-DOCS | 文档一致性 | doc↔code 命令树/签名/退出码/schema 一致（命令树：CLI 产物候选缺失即 fail-closed） | `tools/check_api_docs.py` | P0 |
| CHK-ROOT-CLEAN | 目录规范 | 仓库根目录整洁（§7 白名单 / 运行产物落根 / 必需条目缺失） | `tools/quality/check_root_cleanliness.py` | P0 |
| CHK-UNIT | 单元 | 每模块单测 | `ctest --test-dir build`（模块级） | P0 |
| CHK-ORACLE | 模块数值 | SCI/ALG Oracle 测试 | 模块测试 target | P0 |
| CHK-INVARIANT | 模块数值 | 科学不变量/性质测试 | 模块测试 target | P0 |
| CHK-ABI | 合同/ABI | C ABI 兼容性 | ABI 检查器 | P0 |
| CHK-SCHEMA | 合同/ABI | schema 校验 | schema validator | P0 |
| CHK-DUAL-TOL | 合同/ABI | 双平台允许误差 | 双平台数值对比 | P1 |
| CHK-SYNTH-P1 | 科学 | normalize 合成全链 | 合成全链测试 | P0 |
| CHK-SYNTH-P2 | 科学 | mosaic 合成全链 | 合成全链测试 | P0 |
| CHK-SYNTH-P3 | 科学 | export 合成全链 | 合成全链测试 | P0 |
| CHK-ISA-EQ | 科学 | baseline/AVX2/AVX-512 等价 | ISA 等价测试 | P1 |
| CHK-NWORKER | 科学 | 1 vs N worker 数值一致 | 并行一致性测试 | P0 |
| CHK-SANITIZER | 资源 | ASan/UBSan | sanitizer 构建测试 | P1 |
| CHK-COVERAGE | 资源 | 覆盖率报告 | 覆盖率工具 | P2（报告） |
| CHK-RESOURCE | 资源 | 内存/线程/利用率门禁 | 资源监控测试 | P0 |
| CHK-PACKAGE | 打包 | 发布候选打包/白名单/哈希/版本/provenance | 打包脚本 | P0 |

### 2.1 退役记录（只减不增；每条必须写依据与日期）

| 退役项 | 日期 | 依据 | 处置 | 可复跑性 |
|---|---|---|---|---|
| `TRACEABILITY-CODE`（`python3 tools/check_traceability.py`，原三 profile / `waivable=false`） | 2026-09-16 | 负责人裁决。① 本规范不含任何追溯要求（TRACEABILITY/追溯 零命中）；② `ASTROCS_DESIGN.md` 与 `ENGINEERING_SPEC.md` 同样零命中「追溯」；③ 该门**唯一默认输入** `artifacts/prerelease_v5/tables/TRACEABILITY.csv` 位于构建产物目录（从来不是权威落位），已随 `artifacts/` 按负责人裁决删除（commit `b1290525`「不归档、不保留」）；④ 表内容锚在 `docs/VERSIONING.md` 的版本串匹配上，而新设计 §12 明令版本信息下线 ⇒ 口径被新世代废止；⑤ 内容未丢：`git show b1290525^:artifacts/prerelease_v5/tables/TRACEABILITY.csv` | `ci/checks.json` 移除该注册项（147→146）；`ci/impact_map.json` 清 26 处悬空引用；`ci/tests/test_impact_map.py` 的「SCI→TEST 追踪」必含类保留 `TRACEABILITY-MATRIX`/`CON-TRACEABILITY`；`tools/check_traceability.py` 加退役抬头，**文件不删** | 有：`python3 tools/check_traceability.py <claims.csv>` 仍按 R1–R7 全量校验（夹具示例 `tests/quality/fixtures/docchk002_claims_fixture.csv`，11 claim PASS）；无参调用打印 `TRACEABILITY_RETIRED` 并 **exit 2**（不再回退被删快照，不伪装绿） |

其余追溯类注册项**未退役**（2026-09-16 实测保留）：`TRACEABILITY` rc=0、`PIPELINE-TRACE` rc=0；
`CON-TRACEABILITY` rc=1（`TRACE-CORE-MISSING`：PSF/REJ 核心 SCI 未入表，P1）；
`TRACEABILITY-MATRIX` rc=1（矩阵 7 条 error：BOM / ID 格式 / 悬空测试路径，P1）；
`UT-TRACEABILITY` rc=1（2 failures：一为已退役门的旧消费者
`tests/traceability/test_traceability.py::test_01`，一为上述矩阵失败）。
后三项红灯属既有技术债，另行处置，不在本次退役范围。

### 2.2 工具层退役记录（非注册项，只减不增；每条必须写依据与日期）

RETIRE-001（2026-09-16）退役旧世代（V5 控制包）打包/审计工具。**全部只加退役抬头，文件本体不删**（保留可复跑性）。
统一判定口径：① 权威链 `ASTROCS_DESIGN.md §0`（旧世代控制包产物不构成判据）+ 负责人裁决「历史版本控制包全部作废；
`artifacts/` 不归档不保留」（commit `b1290525`）与 `ASTROCS_DESIGN.md §12`（版本信息下线）；② `ENGINEERING_SPEC.md §8`
（坏掉即红的门要退役或修好，**不允许静默坏掉**）。

| 退役项 | 日期 | 依据 | 处置 | 可复跑性 |
|---|---|---|---|---|
| `tools/assemble_audit.py` | 2026-09-16 | 唯一输入 `工程控制/RELEASE_V5/AstroCS_MAIN_RELEASE_CONTROL_V5_SINGLE_CLI_AMD64_20260828/` 与 `artifacts/prerelease_v5/tables/` 均不存在；实测未捕获 `FileNotFoundError` 且残留空目录 | 加退役抬头；入口改「显式失败」：打印 `ASSEMBLE_AUDIT_RETIRED` 并 exit 2；原实现保留为 `legacy_main()` | 有：`git show 01754fab8618:tools/assemble_audit.py` |
| `tools/make_capsule.py` | 2026-09-16 | 输出 `artifacts/prerelease_v5/capsules/` 已随 `artifacts/` 删除；无参 `IndexError`，带参则静默把 zip 写回已退役路径 | 加退役抬头；入口改「显式失败」：`MAKE_CAPSULE_RETIRED` + exit 2；原实现保留为 `legacy_main()` | 有：`git show 01754fab8618:tools/make_capsule.py` |
| `tools/make_rev2_capsule.py` | 2026-09-16 | 输入三表（`TRACEABILITY/COMMITS/REVIEW_CAPSULE_INDEX.csv`）被静默跳过，输出目录已删 → 实测未捕获 `FileNotFoundError` | 加退役抬头；入口改「显式失败」：`MAKE_REV2_CAPSULE_RETIRED` + exit 2；原实现保留为 `legacy_main()` | 有：`git show 01754fab8618:tools/make_rev2_capsule.py` |
| `tools/pack_audit_package.py` | 2026-09-16 | 打包入口产物落已退役 `artifacts/prerelease_v5/`；实测目录被旁路重建时**静默产出 11.2 MB / 2709 条目且超 10 MB 目标仍 exit 0**，目录不存在则 traceback | **仅退役打包入口 `main`**（`PACK_AUDIT_PACKAGE_RETIRED` + exit 2）；`allowed()/denied()/EXCLUDE_EXT` **保留为活动依赖**（`CHK-SECRET-HYGIENE` 的收录白名单与凭据排除真源），import 语义不变 | 有：`git show 01754fab8618:tools/pack_audit_package.py` |

实测复核（2026-09-16，RETIRE-001 自证）：四个退役项调用均为「明确退役文案 + exit 2，无未捕获 traceback」；
`python3 tools/quality/check_secret_hygiene.py --scope pack` rc=0、`tests/quality/test_secret_hygiene.py` 22/22 OK，
证明 `pack_audit_package.py` 保留函数的活动依赖未受影响。

**经实证不退役（保留原样，不计入退役数）**：`tools/quality/known_failures_baseline.py`（被 `KNOWN-FAILURES-BASELINE`、
`-VERIFY`、`-CHECK` 三个注册项消费，仍产出 `artifacts/KNOWN_FAILURES_BASELINE.json`）；`tools/check_traceability.py`
（已按 GAP-032 于 §2.1 登记）。逐条判定、消费者实测与复原坐标见
`reports/PROJECT-GOVERNANCE-01/retire/RETIREMENT_LEDGER.md`。

## 3. 门禁分级

| 级 | 含义 | 处理 |
|---|---|---|
| P0 | 硬门禁 | 红灯阻塞合并，无 waiver |
| P1 | 硬门禁（可负责人豁免） | 红灯阻塞；豁免须显式登记 |
| P2 | 报告项 | 不阻塞，但需留存结果 |

## 4. 新增检查项流程

1. 在 `ci/checks.json` 登记（ID/命令/门禁级/正例负例）；
2. 提供可"能绿能红"的正例与负例测试；
3. 本地复跑确认；
4. 合入 CI 流水线。

## 5. 运行方式（本地）

```bash
# 全量
python3 ci/run_checks.py --all
# 指定项
python3 ci/run_checks.py --check CHK-FMT CHK-UNIT
# 输出机器可读 JSON 供 CI 消费
python3 ci/run_checks.py --all --json-out ci_result.json
```
