# 包取证摘要：cprun-v4-pack-template

身份由脚本归一（去复原前缀与 .zip 后缀）。共 1 个实例。

## 【worktree】工程控制/cprun-v4-pack-template
- 文件 10 | 13.8 KB
- git 事件: {'first': ('4d3feaf45216e5109ee175585039682af2861324', '2026-09-09T21:27:22+08:00'), 'last': ('4d3feaf45216e5109ee175585039682af2861324', '2026-09-09T21:27:22+08:00'), 'dels': [], 'adds_n': 6, 'mod_n': 0, 'touch_last': None}
- tasks/ 共 6: AUDIT-001.md, CI-TEST.md, DOC-001.md, FIX-001.md, RELEASE-001.md, _TEMPLATE.md
- control-pack.json: {"schema": "cprun/v4", "id": "my-project-remediation", "title": "示例：修复与审核流水线", "workspace": "../my-project", "max_parallel": 4, "lanes": "[3 项] {'repo-write': {'capacity': 1}, 'read-only': {'capacity': 4}, 'windows': {'capacity': 1}}", "gates": "[2 项] {'SOURCE-READY': {'all': [{'task': 'AUDIT-001', 'state': 'passed'}, {'any': [{'task': 'CI-TEST', 'state': 'passed'}]}]}, 'CI-GATE': {'all': [{'externa", "externals": "[1 项] {'ci/windows': {'states': ['success', 'failure', 'pending'], 'initial': 'pending'}}", "tasks": "[5 项] [{'id': 'AUDIT-001', 'title': '核对实现与规范差异', 'spec': 'tasks/AUDIT-001.md', 'lane': 'read-only', 'priority': 100, 'review': 'foreground', 'retry': {'max_"}
