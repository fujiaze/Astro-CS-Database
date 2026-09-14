# STAGE-06 V6 重构、V6.1 返工与 V7 合同-实现波

## 时间窗与提交数
- seq 1261–1520（共 260 条；merge 0，L 级 15 条）。日期 2026-08-30 至 2026-09-05。V6→V6.1 硬分界空窗 575 分钟（seq 1360 02:33:36→1361 12:08:31，H-S25）。
- 对应分片：H-S23（尾）、H-S24、H-S25、H-S26、H-S27、H-S28（前半）。全段单亲线性链。

## 该阶段要解决的问题（引自提交消息/台账，标注自述）
1. V6 重构控制包（88 任务）"合同真相→分层施工→门登记"：G0 基线冻结（seq 1261 | 4b1b948e）→ G1 合同真相层 8/8（seq 1274 | aa4298a7，DOC/SCI/DATA-001、API_CONTRACTS 自述 423 行）→ G2 RUNTIME 9/9 + core 八件套（seq 1276–1284）→ G3 I/O+CPU 8/8 且 TSan 移交 Windows（seq 1297 | f65fcc98）→ G4/G5 Phase1/Phase2 门禁（seq 1307/1316）→ G6/G7/G8…G11 登记链（H-S24/S25：1323/1331/1336/1343/1346/1353）。
2. V6.1 返工：V6 收尾"零源码改动纯核验 + REVIEW_PENDING"（seq 1340/1356–1360，RELEASE_STATUS 措辞链、AWAITING_EXTERNAL_RELEASE_REVIEW）→ seq 1361 | a3d16d85 起身份冻结/源码清单/台账状态机/已知问题基线（F-001..F-040）/检查器体系，落点 evidence/refactor→evidence/v6_1_rework、tools/→tools/quality/ 同时翻转（H-S25）。
3. CLI/运行时结构重做与可达性治理：seq 1379 | 84191084 main.cpp 削为入口壳、拆 parser/commands/runtime_client + 新库 astrocs_cli_runtime（+2479/-1612，本片最大结构改动）；seq 1380 G2 CHECKLIST"唯一 Runtime 与类型化数据链"；PROD_REACHABILITY.dot/json 重算（H-S26）。
4. V7 合同/ABI 冻结波转实现铺设波：seq 1474→1475 三重切换（435 分钟断点；消息改 conventional 前缀 + requirements/tests/scientific_change 结构化正文；编号族切 GOV/LOG/ARC/BLD/DATA/ABI/RT；落点转 contracts/、runtime/、include/astrocs/abi、tools/doccheck，H-S27）→ seq 1486 起 IO/ABI/RT/CPU/DATA 各族"头+C 实现+selftest+契约 pytest"同构连排（H-S28）。

## 具体工作内容（按模块）
- 科学算法：phase1 新实现模块（seq 1300–1302）、phase2 存量整改（seq 1311）、Phase3 施工转换点 P3-001（seq 1317 文档状态统一 + check_p3_status.py 扫描门）；P3-005/006 FITS 重开验证与全链组装测试（seq 1321/1322，H-S25）。
- 管线与 CLI：RT-005..009 运行时合同与实现（seq 1372–1375：pipeline.cpp +233/-176 删手写 JSON 解析改 nlohmann——本片最大单文件生产改动）；CPU-001..004 资源面（1381 起）；P1-001..004 模块迁移（1393–1399，注意 P1 编号序≠实现序 001→003→002→004，H-S26）；P2-001..007（1401–1413，P2-001 依赖列不含 P1-004，Phase 隔离的依赖面证据）；P3-001..006（1415–1426）。
- 构建与 CI：根 CMakeLists +39 严格 flags + 42 文件全仓警告修复（seq 1439，DOC→QA/LNX 族切换点，H-S27）；BLD-002 唯一根构建图（seq 1485）；COMMITS.csv 绑定验证器治理（seq 1383 | 065aa340 一次补录 22 任务 + 放开双任务合并提交，此后绑定计数 23→45 连续，H-S26）。
- 控制包与治理：门登记节拍（9/55 提交为"代码面 0 + 单文件 CHECKLIST/STATUS"，H-S25）；等待登记形态化（seq 1449–1452 首次 WAITING_WINDOWS / NOT_RUN_EXTERNAL_OFFLINE 字面量 + 六份同构 TASK_RESULT 批量建账）；版本字面量全库替换 0.10.0-alpha.1→alpha.2（seq 1367，28 文件）；GOV-004 L0 审查入口五件套（REVIEW.md+docs/owner，seq 1489）+ GOV-005 清陈旧（memory.md -2256，seq 1491）；seq 1480 881 次改名归档搬家。
- 文档与报告：V6.1 文档重建族（DOC 系列，1431–1438）；docs/governance 首现（seq 1482 | 39e77317）。
- 测试：契约 pytest 族随 V7 实现波（1486+）；MSVC 兼容 15 连击（seq 1460–1474 一小时内，全部生产/构建/测试面、零台账面，H-S27）；seq 1341/1342 生产码改动零证据挂账、仅由 1343 门登记短码追认（H-S25）。

## 交付与验证留痕
- V6 审核包记录（seq 1360，HEAD b16d422、568 files 自述；b16d422 成 V6.1 祖先锚 1361/1364）；gates/G1（1371）、G2（1380）CHECKLIST 落盘——G3..G7 无落盘文件，各门实际宣布通过未证实（H-S26）；LNX-002/003 现场结果登记 + MACHINE_SUMMARY.json（seq 1445）；V6.1 审核包 ZIP 绑定 faad602d 状态 NOT_READY（seq 1459，dist/audit/ 为唯一噪声区文件）。

## 结构与规范变化
- 目录谱系：evidence/refactor（seq 1261 引入）→ evidence/v6_1_rework（1361）→ contracts/、runtime/、include/astrocs/abi、tools/doccheck（1475 起）；工程控制/CONTROL_V6 + evidence/refactor 双落点，消息转五段式（V5/V6 分界，H-S23 信号 E）。
- 编号族：G0..G11 + DOC/SCI/DATA/RT/CPU/MON/P1..P3/QA/LNX/WIN/REL/VER/CHK → GOV/LOG/ARC/BLD/ABI/RT（V7）。
- "移除类断言以检查器+文档订正出现，不以删除形态出现"成为可重复模式（seq 1317/1328/1329/1330/1338 删除文件数均为 0，H-S24/S25）。

## 与下一阶段的衔接
- seq 1521 | a4fdee3f V8.1 控制包整包注册 + TASK_STATE/WRITE_LEASE 控制面初始化开启 STAGE-07；自此消息形态从 requirements/tests 模板变为"无正文 + 文件内嵌要点"，evidence/v8_1_ci_control 台账面接管提交流（H-S28 信号⑤）。
- 带入项：1331 vs 1368 对"CLI 是否直连科学内部实现"的跨期对立判定未裁断；LNX-005 改判 PASS 缺数据证据；TSan 移交 Windows 的兑现。

## 证据缺口
- 全部测试结论系台账 PASS 字面 + log_sha256 挂账，卡片不含运行输出（H-S24/S25 通则）；1382 全卡"diff 读取截断"（12 kernel Oracle/3 provider DSO/AVX512<3% 仅自述）；1390 TEST 前缀无绑定行；1395/1425"移除"动作落点未观测；紧凑卡"消息中的编号"大量系 SCI-*/ALG-*/R-CPU-01 误拆致 BRIEF 编号族虚高（H-S24 警告：统计任务量勿用该字段）；1344/1354/1355 台账行误删-回补的恢复态字面量因卡片截断未证实；1356/1359 自述"终收敛/终"后仍继续追加。
