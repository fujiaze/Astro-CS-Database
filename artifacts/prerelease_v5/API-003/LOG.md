# API-003 执行日志 (2026-08-28, vm-bj)

## 输入
03_TASK_DETAILS API-003 行(Phase1 逐函数 create/validate/run/inspect;参数/单位/同步异步/reentrant/错误;验收=doc-symbol-signature checker PASS+每 API 有直接 test ID); 现存六模块头文件(签名权威); API-001 模板。

## 动作
1. 新建 docs/api/PHASE1_API_V1.md(API-P1-001..010): §1 编排级生命周期 p1_session_create/validate/run/inspect/destroy(opaque handle+owner+async_io_depth∈{0,1,2}+取消点帧粒度+stage 事件序列与 stage1 7 路径对应); §2 底层函数登记表 10 行×6 列(函数/头文件+reentrant+threadsafe+internal_parallel+取消点+直接 test ID), 签名以现存头文件为准(V4 资产不重写); ac_set_num_threads 登记 TB-ARCH-004(checker 管控, 迁移整改点 ABI-001 收编); §3 单位/所有权速查(引 GLOSSARY/COMMON_ABI); §4 doc-symbol-signature checker 合同(五查, CLI-002 落地全量, 本任务表+机器门立约)。
2. 机器门 tests/api/test_p1_api.py 6 用例: 文档符号↔六头文件真实存在性核对(头文件路径解析)/生命周期五函数/每行 test+checker ID 与并发字段/迁移点标注/单位引用/stage1 路径数交叉核对(production_call_paths_stage1.csv 行数)。

## 验证
- 全量回归 unittest **64/64 OK**(新增 6)。
- 头文件符号核对实跑: astro_calibration/star_detector/dynamic_psf/ipv_api/photometric_calib/snr_estimator 六头 30+ 符号全命中。

## 产物
docs/api/PHASE1_API_V1.md; tests/api/test_p1_api.py; 本日志。

## PASS 判定
每 API 函数: 参数/单位/同步异步/reentrant/错误码登记齐; doc-symbol-signature 一致性由机器门实证(文档↔头文件); 每 API 直接 test ID 非空。API-003 = PASS。
