# M7 · C_DOC_CODE_GAP · P0（第二层合并：L19/L20/L21）

> **档位声明**：本文件内全部条目的 **类别与优先级由本行标题承载**（协议 §3 的「一类别×优先级一档」）；条目正文只在**偏离本档级别**时显式标注改档及理由（如 M7-A-101 记 P0→P1、M7-A-201 记 P1→P2 并撤核心结论、M7-A-001 记 P1→P0、M7-I-202 记 P1→P2 且改类 A_SCI_DEF→I_DOC_HYGIENE）。逐条四态判定与编号映射见 `问题扫描/_merge/M7.md` §2；每条均含 位置/权威依据/证据或证据出处/问题说明/影响/建议处置/置信度/related/四态判定 九项。

## M7-C-001 采样器控制网格边长可配（1..64）而 UPM 把 grid=8/cell_side=64/tile_shift=9 编成常数，两侧无一致性门 ⇒ 校正场与采样点静默错位

- 类别: C_DOC_CODE_GAP · 建议优先级: **P0**（L20-002 原案维持；本代理已闭合其落地前提）
- **落地前提（父层必核事项 3，本代理读码闭环 = 成立）**：
  | 环节 | 锚（path::符号） | 复核时 | 事实 |
  |---|---|---|---|
  | 配置校验 | `lib/phase2/src/stage2_common.cpp::p2_stage2_config_from_json`（`control_grid_per_tile` 分支） | :40-45 | `m.value("control_grid_per_tile", 8)`，仅校验 `1..64`，**无 =8 约束** |
  | 原样透传 | `lib/phase2/tools/stage2.cpp::main`（W4 CONTROL SAMPLE 块） | :257 | `sccfg.control_grid_per_tile = cfg.control_grid_per_tile;` **未覆写、未 clamp** |
  | 采样侧生效 | `lib/phase2/src/sampler.cpp::p2_sample_controls_cached` | :499、:594-595 | 只把 `<1` 修补为 8；`cell_side = kTileWidth / grid`（kTileWidth=512，:75） |
  | UPM 侧常数 | `lib/phase2/src/upm.cpp::p2_upm_build_geo`、`::p2_upm_open`、`::p2_upm_calibrate_block` | :259-261、:1062-1063、:1254/:1281/:1405 | `m->grid = 8; m->cell_side = 512 / m->grid; const int tile_shift = 9;` **编译期常数** |
  | 键重算 | `lib/phase2/src/upm.cpp::p2_upm_build_geo`（`add_control` / `gx = x / cell_side`） | :298-304 | UPM 按 8×8 从 leaf 反推 (tile,gx,gy)，**不消费采样器 cell** |
  | 证据面自证 | `lib/phase2/src/upm.cpp::p2_upm_geometry_hash` | :1351-1352 | payload 只写 `grid=8;cell=64`（UPM 自家常数），**不含采样器实际 G** ⇒ 绑定/自锁门对错位恒绿 |
- 触发路径（写死给负责人）：`astrocs-stage2`（`lib/phase2/CMakeLists.txt::add_executable`，:113）读 stage2 JSON（`model.control_grid_per_tile`）→ 透传采样器 → 采样器按 512/G 出 control 点与 cell 中心 leaf → 同一进程内 UPM 按 8×8 建 θ/双线性节点并扣 `calibrated = raw − C_f` → save/hash。
- 真实可达性证据：`reports/REAUDIT_V3/scripts/stage2_fatduck.json:11`、`stage2_rerun.json:11`、`stage2_fatduck5/10/26/32*.json` 全部写 `"control_grid_per_tile": 4`；`reports/evidence/PAR002_blocker.md:47` 记 `control_grid_per_tile=12` ⇒ **G≠8 不是假想配置，而是已发生的生产配置**（该路径按协议 §0 不作问题登记处，此处仅作可达性证据）。
- 错位的量级（可算，非笼统）：G=4 → 采样 cell 中心落在 64 网格的**格线**（64/128/192…）上，UPM 反推 `gx = x/64` 得 gx=1,2,3… 其节点中心为 `gx*64+32` ⇒ 每个 control 相对其 θ 节点**系统偏移 32 px（半 cell）**；G=16 → 4 个采样点塌进同一 UPM cell（`cell_index` 去重，:266-274）⇒ 三处观测被当作同一格；G∈{3,5,6,7,…} → `512/G` 整型截断，cell 铺不满 tile，F5 的 `gx*64+32` 假设同时失效。
- 反向事实（另记，不另立条）：IR 生产节点 `lib/core/src/module_adapters.cpp::p2 sample op`（复核时 :2904）用 `p2_sampler_default_config()`（sampler.cpp:294-296 → grid=8）**忽略用户配置**，只覆写 `cpu_workers` ⇒ 该路径恒对齐（不错位），但 `docs/development/CONFIG_SCHEMA.md::§model`（:11）与 `docs/contracts/DATA_SEMANTICS.md::§23.1`（:1377）登记的「用户可配 G」在此通道**静默失效**。此条与主缺陷互斥地取决于「P2 生产入口是 `astrocs phase2 run`(IR) 还是 `astrocs-stage2`」——该裁决属 M4/前台（`_merge/00_COORDINATION.md:463` 记 M4 判「stage2 是唯一真实生产 CLI」），已列移交。
- 权威依据: 宪章 §1.1、§6.3（control point 规则冻结）、§12.3-1/-2；SCI-UPM-001 §5（`C_f(p)=双线性(8×8 control cell, θ_f)` 为 FROZEN 连续定义，复核时 :43）
- 影响: Phase2 calibrated/mosaic 产品的背景校正面按错误格架插值后被**当作科学校正量扣除**，而 hash/save/open/frame_id 绑定门全绿。属「文档与实现的行为差异会改变输出数值」的 P0 典型。
- 建议处置: ①ALG 层声明「UPM 网格必须等于采样器 `control_grid_per_tile`」并在 `p2_upm_build_geo` 加显式 rc（或把 G 从可配字段删除，与 SCI 冻结面对齐）；②`PHASE2_SAMPLER.md::§9` 禁改清单补「512 可整除」前置；③补一条 G≠8 的负面回归测试（已另立 `M7-T-104`）；④`p2_upm_geometry_hash` 必须把**采样器实际 G**纳入 payload。
- 置信度: 高（六处代码本代理逐一读毕）
- related: L08 §6 第 5 条（同一线索的待复核项，本条定稿）、DISP-P2SMP-001、L20-011、M4 的入口裁决、A.3（前台入口口径）
- 四态判定: **仍成立**（前提由本代理读码闭合；文档侧 `docs/algorithms/PHASE2_SAMPLER.md::§3/§6/§10` 与 `docs/algorithms/PHASE2_UPM_IMPL.md::§6 F4/F5` 复核时文本未变）
