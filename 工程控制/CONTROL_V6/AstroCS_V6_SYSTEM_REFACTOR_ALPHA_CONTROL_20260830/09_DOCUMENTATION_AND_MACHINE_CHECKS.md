# 文档分层、代码注释与机器一致性

## 1. 单一真相表

| 内容 | 唯一权威 | 机器派生物 | 禁止重复维护 |
|---|---|---|---|
| 软件版本 | 根 `VERSION` | header、CLI、manifest、release docs | 源码字面量版本 |
| 科学公式 | `docs/contracts/science/SCI-*.md` | 公式索引/测试关联 | README 再写一套不同公式 |
| 离散算法 | `ALG-*` | kernel/test matrix | 代码注释中藏算法变体 |
| 数据语义 | `DATA-*` + schema | port/artifact index | `weight/value` 靠上下文猜 |
| Pipeline | canonical Pipeline IR | static graph、owner overview | CLI/session 手写另一顺序 |
| 模块集合 | source ModuleRegistry | module index | 手工模块清单 |
| 真实 API | public header + AST | API reference/diff | 文档虚构函数名 |
| 现场结果 | immutable L3 evidence | L0 status | 历史 PASS 复制 |

## 2. L0：负责人阅读层

根 `REVIEW.md` 是唯一入口，链接：

1. `SCIENCE_OVERVIEW.md`：只写核心科学定义、关键公式、假设、已知限制；
2. `PIPELINE_OVERVIEW.md`：从 IR 自动图派生并加人类解释；
3. `ARCHITECTURE_OVERVIEW.md`：职责、接口边界、CPU/ACR 决策；
4. `RELEASE_STATUS.md`：每个硬门 PASS/FAIL/NOT VERIFIED；
5. `CHANGE_REVIEW.md`：本轮变化、科学影响、原子 commit、证据、需决策。

L0 不列数百函数，不粘贴日志，不要求负责人维护；每个结论必须链接到 L1/L3。

## 3. L1：合同层

每份合同 front matter 固定：

```yaml
id: SCI-P2-UPM-001
version: 1.0.0
status: ACTIVE
owner: AstroCS
upstream: []
downstream: [ALG-P2-UPM-001]
source_commit: <generated-at-release>
```

SCI：对象、物理意义、数学、单位、假设、适用边界、禁止解释。  
ALG：从 SCI 推导、离散、复杂度、精度、并行可分性、误差。  
DATA：字段、类型、单位、坐标、shape、invalid、ownership、version。  
ARCH：职责、依赖、生命周期、串并行/异步、错误/恢复。  
API：真实符号解释、pre/postcondition、ownership、thread-safety。  
MOD：模块 ID、端口、config、execution/memory model、contracts。  
TEST：fixture、oracle、properties、tolerance、platform/backend matrix。

## 4. L2：模块层

每个 production module 必须有 `README.md`，按模板回答：

- 做什么/不做什么；
- 输入输出 DATA ID 与单位；
- 对应 SCI/ALG/API/MOD/TEST；
- public headers 与核心 source symbols；
- execution class、并行轴、ThreadLease、determinism；
- 内存/缓存/Artifact ownership；
- 错误、取消、checkpoint；
- 如何独立运行 synthetic test；
- 已知限制。

README 只解释和链接，不复制可漂移的 header 声明或完整公式。

## 5. 代码注释规则

必须注释：数学选择原因、单位、坐标约定、所有权、thread-safety、数值边界、非显然优化及与 ALG 的关系。

禁止注释：逐行复述、任务历史、审核轮次、旧版本故事、已删除代码、虚假的“thread-safe”、与签名重复的参数描述。

核心实现附近使用稳定 ID，例如：

```cpp
// ALG-P2-UPM-001 §4: gauge-fixed additive background model.
// DATA-P2-UPM-002: coefficients are additive in input image units.
```

注释中的 ID 必须由机器验证存在。

## 6. 机器检查工具

### `check_contract_graph`

- 解析所有合同 front matter；
- ID 唯一、路径存在、upstream/downstream 双向一致；
- production MOD 必须有 SCI/ALG/DATA/API/TEST；
- ACTIVE 不得依赖 OBSOLETE/CONFLICT；
- 输出 `CONTRACT_GRAPH.json/.dot`。

### `extract_api_ast`

- 使用 compile_commands + Clang AST；
- 提取 public class/function/enum、参数/返回、cv/ref/noexcept、visibility；
- C ABI 提取 struct layout/version/export；
- 与 API machine index 对比；
- 文档只解释真实符号，删除/重命名后自动报悬空。

### `check_module_registry`

- 比较 registry、CMake targets、link symbols、module README、MOD contracts；
- duplicate/missing/unused module 失败；
- CPU-heavy+serial descriptor 失败；
- ACR 在 production registry 出现失败。

### `check_pipeline_trace`

- 从 IR 生成预期 node/edge/port/artifact；
- 与运行 JSONL 的 module version、producer/consumer、状态序列对比；
- Phase `all` 检查 P1→P2→P3 Artifact ID/hash 连续；
- 文档图由同一 IR 生成，不手画第二拓扑。

### `check_serial_heavy`

- 编译 AST/call graph + allowlist；
- 查生产可达路径的固定 workers=1、裸 thread pools、direct `omp_set_num_threads`、serial pixel/sample loops、全局热点 mutex；
- 仅 grep 命中不是最终结论，但所有命中必须分类；
- allowlist 仅短 I/O/metadata/small threshold，含 reason/owner/expiry/test。

### `impact_analysis`

- 从 git diff 的 header/source/schema/contracts/IR/CMake 计算受影响模块与 downstream；
- 输出必须运行的 tests；
- 科学/单位/topology/backend/platform 变更自动扩大；
- Agent 不可手工缩小集合，只可增加。

## 7. 机器检查的负面测试

工具自身必须有 fixtures，故意制造：

- 不存在的 SCI ID；
- API 函数改名但文档未改；
- module 声明端口单位与 DATA 不同；
- pipeline 环/重复 producer；
- runtime trace 少节点/换 Artifact；
- CPU-heavy 声明 serial；
- production link 含 ACR；
- L0 PASS 无证据 hash；
- summary counts 与 ledger 不一致。

每类错误必须让 CI 非零退出。没有 negative fixture 的 checker 不算可信门禁。

## 8. 运行图作为代码文档

每个正式 preset 生成两种图：

- static architecture graph：模块、端口、DATA ID、execution class；
- observed runtime graph：实际节点、Artifact、时延、backend/workers、status。

图生成命令、IR hash、source commit写入旁车 JSON。发布 L0 只嵌入简化图，L3 保留完整 graph JSON/DOT/SVG。运行图不能包含真实数据绝对路径或凭据。

## 9. 文档发布门

- 所有 ACTIVE 合同 graph pass；
- AST/API zero drift；
- Registry/CMake/link/module docs zero drift；
- Static IR/runtime trace zero drift；
- L0 状态由 Ledger/L3 自动汇总；
- 扫描不到旧 production executable、旧版本或 Phase3 假实现；
- 负责人只需从 REVIEW.md 可判断科学、架构、风险、验证和发布限制。
