# REAL-000｜真实数据审计、索引 v1.2 与确定性匹配计划

> 依据：`04_OWNER_DECISIONS_20260910.md` 真实数据验收指令。
> 本任务是 REAL-001 的前置：先把数据事实、匹配规则和缺口固定成机器可检验的计划，再跑链路。

## 目标

1. **数据盘点与对账**（只读数据）：
   - 枚举 `testdata/` 全部文件（预期 944：.fts 906 / .xisf 27 / 其他 11），按数据集×望远镜×面板×滤镜×曝光分组计数；
   - 与 `testdata/index.json` v1.1 对账（7 数据集/710 帧/27 母版），量化差异；
   - 盘点 `GaiaDR3/`（16 个 .xpsd）与 `GaiaDR3SP/`（20 个 .xpsd），按 DATA-GAIA-001
     记录 db_type/file_count/路径，产出数据库注册摘要。
2. **索引升级 v1.1→v1.2**（唯一允许写的数据面文件：`testdata/index.json`）：
   - 新增 `M42_T2T3_mosaic_Flying_dutchman` 数据集条目：multi-telescope（T2+T3）、
     面板 M1..M6×2、逐面板逐滤镜计数（磁盘实测 T2 300s Blue 19/Green 17/Red 16/H-alpha(300s) 1/
     H-alpha(600s) 20；T3 300s Blue 29/Green 32/Red 33/H-alpha(600s) 29）、
     滤镜 brand/model/bandwidth（从 `testdata/M42_T2T3_mosaic_Flying_dutchman/素材信息与版权约定.txt` 提取）；
   - schema 扩展向后兼容：`telescopes: [..]`（多望远镜）、可选 `pixel_size_um`；
   - T2/T3/T4 传感器像元尺寸：优先从各数据集素材信息文件提取；缺失 → 置 null 并登记
     `OWNER_CONFIRM` finding（**禁止凭相机型号臆造**）；
   - 同步 `statistics` 与 `version`；跑 `tests/monitoring/test_log_contract.py` 相关用例确认消费方不破。
3. **确定性匹配计划**（新工具 `tools/realdata/match_plan.py` + 测试 `tests/realdata/`）：
   - 匹配键（文档化并冻结于工具内）：望远镜目录 → sensor 尺寸/binning → 曝光 → 滤镜（大小写不敏感归一，
     OIII/Oiii 等价；**不改数据文件名**）；
   - dark 策略：优先精确曝光；缺失时按冻结策略
     `docs/science/CALIBRATION.md:57,90`（K=t_light/t_dark 线性缩放）与
     `docs/algorithms/CALIBRATION_ALGORITHMS.md:158,292`（`OPTIMAL` fallback `EXPOSURE_RATIO`，
     phase_config `dark_optimization` 显式开启）选择替代 dark 并记录 K 与策略码；
   - 每帧产出：`{bias, dark(file,K,strategy), flat}` 或 `UNMATCHED(reason_code)`；
   - 已知缺口须在计划中量化：T2 300s×53、T3 300s×94（M42）→ dark 缩放；T2 Lum×15（NGC247）→
     `UNAVAILABLE(NO_LUM_FLAT)`；
   - T1：显式空集用例——枚举 0 帧 0 母版，计划输出 `T1: empty(PASS)`，不是 skip。
   - 产出：`run/realdata/match_plan.json`（逐帧）+ `run/realdata/match_summary.md`（人读）+
     `run/realdata/file_manifest.csv`（逐文件 sha256，30G 预计分钟级）+ 各数据集 phase_config 模板
     （含 masters 路径、滤镜、曝光、gaia_data_dir、dark_optimization=true）。

## 写入白名单

- `testdata/index.json`（唯一数据面写入）
- `tools/realdata/`（新建匹配工具）
- `tests/realdata/`（新建测试）
- `run/realdata/**`（产物，gitignore）

## 非目标与禁令

- 不修改/移动/重命名 testdata、GaiaDR3、GaiaDR3SP、BASS DR3 下任何数据文件；
- 不在本任务跑 Phase1/Phase2 链路（那是 REAL-001）；
- 不发明冻结文档之外的校准策略；
- 不修改 `docs/science|algorithms` 冻结文本。

## 必须动作

1. 只读盘点（带 timeout 与日志）→ 对账表；
2. 写匹配工具 + 单元测试：正例（T4 精确匹配 180/300/600 + 5 滤镜）、
   策略例（T2 300s→600s dark K=0.5 OPTIMAL/EXPOSURE_RATIO）、
   负例（错滤镜 Lum↔Red、错曝光超策略、错传感器 4096↔4500、错望远镜目录）全部确定性拒绝；
3. 升级 index.json v1.2 并跑索引消费方测试；
4. 生成 match_plan 与 phase_config 模板；
5. 全部命令 timeout/日志/SHA 入证据。

## 验收

- `count_enumerated == count_classified`（906 亮场逐帧有记录；27+? 母版逐个有记录）；
- 缺口计数与指令表一致（53/94/15），reason_code 机器可读；
- T1 空集 PASS 用例存在且通过；
- 匹配器测试：正例全过、四类负例全拒；
- index 消费方测试不破（`tests/monitoring` 相关用例 PASS）；
- file_manifest 覆盖 100% 数据文件（行数=文件数），抽验 10 个 sha256 与 `sha256sum` 一致。

## 返回证据

scope/acceptance/provenance 三检查 + `run/realdata/` 产物清单 + 差异/缺口表 + OWNER_CONFIRM 清单。
