# 工程控制

本目录是控制包工作区：按控制包规范组织的任务卡、验收、交接与未决问题文档按包存放在各自的子目录中，包内以 00_README.md 为启动入口。

## 职责边界

- 放：一个控制包一个子目录，包内含启动说明、任务总览、任务书、验收、交接、差距与未决问题等固定件。
- 不放：任务的执行产物与日志（run/）、验收证据（artifacts/）、科学实验单元（实验/）。
- 控制包收口后的包内文件按 CONTROL_PACK_SPEC.md 的清理条款处理。

## 内容

- RELEASE-05/ —— 控制包子目录，包内固定件如下。
- TASK_LIST.md —— 任务总览清单。
- tasks/ —— 逐项任务书，一任务一文件。
- ACCEPTANCE.md —— 本包验收要求。
- GAP_AUDIT.md —— 差距清单。
- HANDOFF.md —— 交接说明。
- OPEN_QUESTIONS.md —— 未决问题登记。
- PROMPT.md —— 执行入口提示。
- SUMMARY.md —— 汇总件。

## 上游

上游：CONTROL_PACK_SPEC.md（控制包规范：任务卡、回执、收口与清理）；ENGINEERING_SPEC.md §7（工程控制/ 目录登记）。