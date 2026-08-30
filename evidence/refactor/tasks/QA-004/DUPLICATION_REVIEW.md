# QA-004: 重复实现与所有权审查报告

状态: PASS — 逐区域判定 KEEP/MIGRATE/DELETE

## 1. WCS (TAN 投影)
- `lib/phase1/wcs/wcs_tan.{h,cpp}` (P1-004): 单帧像素↔天球, 天体测光/星表。
- `lib/phase3_session/p3_wcs.cpp` (P3-002): HiPS 球面→平面重投影, 尺寸溢出检查。
- **判定: KEEP (层分离)** — 不同数据域(帧 vs HiPS), 独立合同(P1-004/P3-002), 数学共用标准 TAN 公式但实现独立; 合并会耦合跨层合同。

## 2. weight/ivar 语义
- `lib/phase1/noise/noise_model.cpp` (P1-005): variance/ivar 定义 (1/variance, floor=1e-12)。
- `lib/phase2/src/integrate.cpp` (P2-006): ivar 加权积分 (type=ivar 非模糊 weight)。
- **判定: KEEP (合同分层)** — P1 定义, P2 消费; 显式 ivar 类型传递, 无重复算法。

## 3. config 解析
- `cli/main.cpp` parse_args + validate_config_full: CLI 层参数校验。
- `lib/phase2/src/stage2_common.cpp` p2_stage2_parse_config: phase2 配置。
- `lib/phase3_session/p3_session.cpp`: phase3 配置。
- **判定: KEEP (按层)** — CLI 只做命令层; 各 session 解析自身配置 (adapter 语义, P1-001); 无跨层重复。

## 4. scheduler/调度
- 生产: CLI run preset (CLI-002) → 3 session facade; 无第二调度器。
- 旧: orchestrator.cpp (5405 行) → **LEG-002 已退出**; aio_pipeline_engine → **LEG-003 退出调度**。
- **判定: DELETE (已完成)** — 调度职责已删除, 底层函数保留。

## 5. I/O
- `lib/astro_image_io` (aio_*): 唯一 FITS 读写层 (cfitsio 封装)。
- phase1/2/3 均经 aio 或 session 内 adapter, 无第二 I/O 层。
- **判定: KEEP (单例)** — 唯一 I/O 实现。

## 结论
重复调度器: 2 个已 DELETE (LEG-002/003)。重复实现: 0 个新发现。
所有权: 每模块 owner = 控制包任务 (P1/P2/P3/CLI/IO/CPU); 无无主代码。
