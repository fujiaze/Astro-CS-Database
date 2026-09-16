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
