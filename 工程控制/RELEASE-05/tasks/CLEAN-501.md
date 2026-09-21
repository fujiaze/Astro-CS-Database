# 任务：CLEAN-501 死代码与注册表缺口收尾（A 线）

## 目标
RELEASE-04 被跨域注册表阻塞的删除项与路径/能力缺口全部处置完毕，生产树只留当前架构代码。

## 权威依据
ENGINEERING_SPEC §2；GAP_AUDIT G2-6、G3-1/G3-2；RELEASE-04 ACCEPTANCE D-2。

## 改动范围
- 允许改：lib/ 退役文件、eng/ci 注册表（仅删除已退役条目，判据改动归 GATE-501，前台登记）
- 依赖：ARCH-505（Session/orchestrator 退役后才能清关联文件）

## 步骤
1. B1 orchestrator 50 文件 18411 行：ARCH-505 裁决后，未采纳部分删除、采纳部分并入 pipeline；
2. B2 p3_v6_export.* + v6_p3 集成测试、B3 v1 p3_projection.*、B4 healpix_drizzle shim：ARCH-504/505 后删除（删除或统一注释块）；
3. B5 psfsw/weight_mode 残留：代码侧随 DOC-502 合同段落一并清；
4. CLEAN-402 的 12 组注册表锁定无引用文件：随注册表条目退役分批删除；
5. 66 路径缺口 + 72 能力缺口逐项三选一：实现（立任务）/登记 DEFERRED（写明非目标与理由）/声明作废（清注册表与文档）；出全表；
6. 每批删除跑全量测试 + CHK-RETIRED-CODE；MODULE_MAP fake=0。

## 验收门
- [ ] B1–B5 与 12 组候选逐项有处置结论（删除回执或保留理由）；
- [ ] 66/72 缺口全表闭环，无悬空注册表条目；
- [ ] 全量 ctest 与 fast 门绿；零死代码扫描告警。

## 禁止
- 不删 testdata/、实验/、docs/research 任何证据；拿不准的保留并登记。
