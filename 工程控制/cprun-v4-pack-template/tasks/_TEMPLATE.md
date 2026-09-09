# 任务规格模板（tasks/<TASK-ID>.md）

控制器会把本文件全文逐字放进该节点 Worker 的 prompt，前台永远看不到正文。
写成“换成任何一个新 Agent 也能照做”的程度即可。

## 目标
一句话说清本任务要达成什么（可验证的结果，不是过程）。

## 输入
- 权威路径 / 参考资料（相对 workspace 的路径或 URL）
- 上游节点产出的衔接说明（如依赖 AUDIT-001 的结论，写明去哪里读）

## 要做的事
1. 步骤一
2. 步骤二
（只描述本任务边界内的工作；越界的整合/提交工作建成独立节点）

## 产出
- 修改/新建的文件与位置
- 需要提交的 artifact（如 `ci-report.md`），与 control-pack.json 里 evidence.artifacts 对应

## 验收条件（对应 evidence.checks，逐条给出可自测的判据）
- build：`<命令>` 退出码 0
- unit-tests：`<命令>` 全部通过

## 证据报告（完成后必须写）
控制器 prompt 会给出报告的绝对路径（`…/evidence/<nodeId>/<attempt>.json`）与
run_id / node_id / attempt / dispatch_token 四个回填值，照抄即可。结构：

```json
{
  "schema": "cprun/evidence-v1",
  "run_id": "<照抄 prompt 给的值>",
  "node_id": "<照抄>",
  "attempt": 0,
  "dispatch_token": "<照抄>",
  "result": "completed",
  "summary": "一段话结论",
  "checks": [
    { "id": "build", "status": "pass", "evidence": "build/build.log" },
    { "id": "unit-tests", "status": "pass", "evidence": "12/12 passed" }
  ],
  "artifacts": [
    { "path": "ci-report.md", "sha256": "<可选，填了会校验>" }
  ],
  "changed_paths": ["src/a.ts", "tests/a.test.ts"]
}
```

## 禁止事项
- 不修改 write_scope 之外的文件；不碰 CPRun 状态目录
- 不伪造证据：checks 里没有跑过的项不得标 pass
- 完成即正常结束输出总结，不要等待 further instructions
