# CPRun V4 控制包模板（CP）

配套「控制包执行模式（CP）」preset 使用：前台 = 控制面，只操作 `cp` 工具；控制器 = 唯一调度权威；Worker SubAgent = 干活。

复制本目录 → 改 `id`/`workspace` → 按任务改 `tasks/` → 注册。

## 使用流程

```text
cp { action: "register", path: "<本目录>" }        # 全量校验+冻结图，返回首张任务卡
cp { action: "dispatch", ids: ["AUDIT-001", …] }   # 提交全部 READY id；控制器原子预留并生成 token
                                                   # 插件为每个 reservation 启动独立 SubAgent（一节点一 SubAgent）
cp { action: "review", reviews: [{ id: "AUDIT-001", decision: "pass" }] }
                                                   # 简审；证据缺失/无效会被控制器拒绝 PASS，只能 retry
cp { action: "status" }                            # 紧凑汇总；pause / resume 同理
```

前台规则：控制包运行中仅负责派发和简审；不得亲自执行任务。
每次 revision 变化且需要动作时，插件自动注入一次任务卡（READY/REVIEW 列表），照着卡上的 ID 操作即可。

## 字段速查

| 字段 | 必需 | 说明 |
|---|---:|---|
| schema | ✓ | 固定 `cprun/v4` |
| id / title | ✓ | 稳定标识 / ≤80 字符标题 |
| workspace | ✓ | 工程工作区，相对路径按包目录解析 |
| max_parallel | ✓ | 全局 Worker 并发上限 |
| lanes | ✓ | 资源组与容量；写同一仓库 → `repo-write` capacity 1 |
| gates | – | 命名门禁：只有 `all`/`any` + task/gate/evidence/external 四种原子 |
| externals | – | 外部有限状态机，用 review 的 payload.externals 翻转 |
| tasks[].spec | ✓ | 包目录内的文件（禁止 `..` 逃逸），全文只进对应 Worker |
| tasks[].depends_on | – | 硬依赖：必须全部 `passed` |
| tasks[].requires | – | 命名门禁：必须全部 `open` |
| tasks[].review | ✓ | `foreground`（前台简审）/ `machine`（证据有效即自动 PASS） |
| tasks[].retry | ✓ | `max_attempts`；`on_exhausted`: `failed` 或 `hold`（停在 retry_wait 等人工） |
| tasks[].write_scope | – | 允许修改的路径；报告 changed_paths 越界 → 证据无效 |
| tasks[].evidence | – | `checks`（报告里必须 status=pass）+ `artifacts`（必须存在，可校验 sha256） |

## 门禁原子

```json
{"task":"ID","state":"passed|failed|finished"}
{"gate":"G","state":"open|closed"}
{"evidence":"TASK.checkId","state":"valid|invalid|present"}
{"external":"name","state":"<声明的枚举值>"}
```

只允许 `all`/`any` 组合；环（任务环/门禁环/混合环）注册时直接拒绝并给出完整环路径。
只有 `passed` 满足下游 depends_on；PASS 后新 READY 节点在同一 revision 全部出现。

## 文件清单

- `control-pack.json` — 可直接注册的示例包
- `TEMPLATE.control-pack.jsonc` — 带注释的全字段模板（注册前去掉注释）
- `tasks/_TEMPLATE.md` — 任务规格模板
- `evidence-template.json` — 证据报告结构（回填值由 Worker prompt 提供）
