# AstroCS 发布状态（Release Status）

> 状态词唯一口径：`ASTROCS_DESIGN.md` §11.3 —— `CONTRACT_READY` / `IMPLEMENTED` /
> `INSTALLED` / `VERIFIED`；负向 `NOT_IMPLEMENTED` / `NOT_VERIFIED` / `DEFERRED` /
> `DORMANT` / `FAIL`。**合成测试或历史可用节点不等于真实数据/Windows VERIFIED**（§11.3 末条）。

当前结论（DOC-001 复检，2026-09-16）：

```text
发布结论:            NOT_READY_FOR_RELEASE（未到 READY_FOR_OWNER_REVIEW）
真实数据面:          NOT_VERIFIED（FINAL_REAL_DATA_VALIDATION 未达成）
Windows x64 复验面:  NOT_VERIFIED（VERIFIED 要求正式平台 + 真实数据验收通过）
ACR（CPU/GPU 异构）: DORMANT（不进生产构建/加载/路由/发布）
psf_snr_power:       DEFERRED（生产拒绝）
```

- 逐面状态与证据锚以 `docs/owner/RELEASE_STATUS.md`（§0 词表 + 分面表）与
  `docs/modules/MODULE_MAP.yaml` 为准；本页只登记发布结论，不复制分面表。
- 最终发布决定只属项目负责人；Agent 至多声明 `READY_FOR_OWNER_REVIEW`
  （`ASTROCS_DESIGN.md` §12）。Alpha 前程序/代码/产物内不存在版本信息（§12）；
  根 `VERSION` 仅为内部助记符，不进入程序与发布产物（`ENGINEERING_SPEC.md` §7）。
- 历史轮次（V19R2/V19R8 等 quality-closure 节点、`PRE_RELEASE_ENGINEERING_FOUNDATION`
  一类自述门）**不是当前状态**，只作追溯；原始记录见 `docs/archive/**` 与 `git log`。
