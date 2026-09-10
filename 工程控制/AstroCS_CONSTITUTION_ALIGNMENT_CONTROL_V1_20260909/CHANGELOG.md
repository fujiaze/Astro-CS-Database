# 控制包 CHANGELOG｜ASTROCS-CONSTITUTION-ALIGNMENT-V1

## rev3（2026-09-10）

负责人真实数据验收指令落地（`04_OWNER_DECISIONS_20260910.md`）：

- 新增 `REAL-000`：真实数据审计、`testdata/index.json` v1.1→v1.2（补 M42 数据集、
  multi-telescope mosaic schema、pixel_size_um 待素材文件确认）、确定性匹配工具与测试、
  逐文件 sha256 清单、缺口量化（T2/T3 300s dark 缩放 53+94 帧、T2 Lum 平场缺失 15 帧、
  T1 显式空集）。
- 扩写 `REAL-001`：三层验收——A 全量校准+解算（553 帧）、B M42+银心完整 Phase1（353 帧
  单帧 HiPS）与 Phase2（4+5 滤镜马赛克，schema/matrix 校验，GaiaDR3+GaiaDR3SP 双分支）、
  C 机器证据与资源门；断点续跑；timeout 提至 259200s。
- 扩写 `VIS-001`：固定拉伸参数预览（马赛克全图+面板接触表）、初审清单逐项勾选。
- 增补 `WIN-001`：同候选真实数据复验子集（manifest 绑定、不 checkout/编译）。
- `TASK_LEDGER.csv` 扩至 30 任务并同步状态快照（PASSED×9、PASSED_FINDING_OPEN×1、
  IN_FLIGHT×1，以 ACTIVITY_STATE.md 为真相源）。
- `control-pack.json` 与台账重新对齐（REAL-001 依赖 CI-002+REAL-000；VIS-001 转 repo-write）。
- `validators/validate_control.py` CRIT 集扩至 30。

## rev2（2026-09-10，执行侧）

- 接入负责人三项裁决与 P1 架构抽验：`WCS-003`（AP/BP 扩展+迭代反演，选择 B）、
  `BASE-UTIL-001`（utilization 归因，选择 A）、`P1-HIPS-DIGEST-001`（多 HDU digest，选择 A）、
  `ARCH-AUDIT-P1`（独立架构抽验）；G-SCI 增加 WCS-003，RT-001 增加 BASE-UTIL-001/WCS-003，
  AUD-001 增加 ARCH-AUDIT-P1/P1-HIPS-DIGEST-001。

## rev1（2026-09-09）

- 初版 25 任务、6 门禁、cprun/v4 图、evidence schema、双校验器。
