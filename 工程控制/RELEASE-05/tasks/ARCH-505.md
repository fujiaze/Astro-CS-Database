# 任务：ARCH-505 生产节点改写与旧架构退役（A 线）

## 目标
全部生产节点改为从命名块读写、由三阶段调度器编排；在数值等价验证通过后退役整阶段 Session 与未采纳的 orchestrator 实现；架构文档随代码同提交重写。

## 权威依据
最高设计 §8（全文）、§10.3；ENGINEERING_SPEC §2（退役实现二选一处置）。

## 改动范围
- 允许改：`lib/phase1_session/**`、`lib/phase2_session/**`、`lib/phase3_session/**`（含 p3_v6_export）、`lib/infrastructure/pipeline/orchestrator/**`、20 个生产节点装配、`docs/owner/PIPELINE_OVERVIEW.md`、`docs/owner/ARCHITECTURE_OVERVIEW.md`、`docs/architecture/**`
- 禁止改：科学算法内部（除非对应 SCI 任务已授权）、eng/

## 步骤
1. **冻结等价基线**：改写前跑全量现有测试 + 三命令合成全链，归档输出哈希与关键数值（容差表写入任务回执）；
2. 逐阶段把 Session 中的节点装配迁移到新调度器+命名块（normalize/mosaic/export 分别依赖 ARCH-502/503/504）；每迁一个阶段做一次全链等价比对；
3. 每节点声明块读写集合与生命周期，长生命周期块（WCS/PSF/SNR/星表匹配）随帧到导出，短生命周期块即时消耗；
4. 等价验证：合成全链三命令输出与基线逐块比对（数值容差按 Oracle 冻结，不放宽）；testdata 小样本冒烟；
5. 退役：lib/phase*_session、orchestrator 中未采纳部分按 ENGINEERING §2 删除或统一注释块（含原因+替代指针）；CHK-RETIRED-CODE 绿；MODULE_MAP fake=0；
6. 文档同提交：PIPELINE_OVERVIEW/ARCHITECTURE_OVERVIEW/architecture 全部改为三调度器+命名块描述，无 Session 架构残留；
7. 与 SCI-506 协调：若 p1001 红在迁移中自然消除，登记关联。

## 验收门
- [ ] 生产路径无 Session 直接装配（grep + 注册表双证）；
- [ ] 合成全链三阶段等价（哈希/容差表）；1/N worker 一致；
- [ ] 退役处置完整（删除或统一注释块），CHK-RETIRED-CODE 绿；
- [ ] 架构文档与代码一致（独立子代理抽查 5 个节点的块流图）；
- [ ] ctest、ci fast 全绿。

## 禁止
- 不先删后验；不等价就保留并登记阻塞原因，不硬退役。
