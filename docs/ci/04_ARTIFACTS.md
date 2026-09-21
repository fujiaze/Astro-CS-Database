# CI 产物与留存（Artifacts & Retention）

> 上游：ASTROCS_DESIGN.md §10（I/O 与原子产品）、§12.4（验证层级与四层验收）

## 1. 产物清单

每次 CI 运行留存：

| 产物 | 内容 | 用途 |
|---|---|---|
| `astrocs-linux-<sha>.tar.gz`（Alpha 前无版本号，见最高设计 §12） | Linux 安装目录（ELF+.so+schemas+manifest） | 复验/发布候选 |
| `astrocs-win-<sha>.zip`（Alpha 前无版本号，见最高设计 §12） | Windows 安装目录（exe+dll+schemas+manifest） | 复验/发布候选 |
| `test-results/` | ctest 结果、单元/模块/合成测试日志 | 验收证据 |
| `checks-report/` | `ci_result.json` + 各检查项证据 | 门禁报告 |
| `logs/` | 各 job 日志 | 失败诊断 |
| `gates-summary.md` | 门禁汇总 | 发布候选依据 |

## 2. 命名与哈希

- 产物名含 commit SHA 短 8（Alpha 前无版本号；可发布 Alpha 后按 §12 加版本）；
- 每产物附 `SHA256SUMS.txt`；
- 发布候选与普通构建分开存放（`artifacts/release-candidates/` vs `artifacts/builds/`）。

## 3. 留存策略

- 普通构建：保留最近 N 次（可配）；
- 发布候选：长期保留 + 关联验收记录；
- 真实数据/复验证据：长期保留，关联到发布记录。

## 4. 产物验证（CI 内）

- 安装目录可自检：`astrocs doctor` rc=0；
- 模块可独立加载/验证/卸载（CHK-MODULE-MANIFEST 关联）；
- 打包白名单/哈希/版本/provenance 校验通过（CHK-PACKAGE）。

## 5. 发布候选门槛（与最高设计 §12 一致）

发布候选至少满足：合同冻结无冲突、追踪无断链、模块可独立加载/验证/卸载、ACR 生产不可达、heavy 无硬编码线程/单线程长计算/持续低利用率/无界内存增长、双平台 CI 通过、Linux 真实数据终验 + Windows 复验、图像有量化证据+Agent 初审+Owner 终审、P0/P1=0、发布包白名单/哈希/版本/provenance 通过。

**产物只是材料；最终发布决定只属项目负责人。**
