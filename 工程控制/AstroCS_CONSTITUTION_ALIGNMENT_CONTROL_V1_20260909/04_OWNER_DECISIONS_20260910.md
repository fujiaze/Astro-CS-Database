# 04｜负责人裁决与真实数据验收指令（2026-09-10）

> 本文件是负责人裁决的包内规范记录。与 `00_READ_FIRST.md`、冻结宪章冲突时以冻结宪章为准；
> 本文件由负责人裁决驱动，Agent 不得修改裁决内容，只能执行或在 BLOCKED_EXTERNAL 中上报。

## 裁决 1｜WCS-002 F1（选择 B）

F2 冻结 1e-4px roundtrip 在当前冻结 fixture（边缘畸变 ~117px）下数学不可达（现状 7.6438px，
与独立 numpy 最优投影一致）。负责人裁决：**AP/BP 布局扩展 + 消费方迭代式反演**。

- 不接受缩小 fixture 畸变量级（方案 A 否决）；
- 暂无依据修改冻结门数值（方案 C 否决）；
- `<50px` 仅为防退化观察线，不得作为验收线；
- 执行载体：`WCS-003`（规格见 tasks/WCS-003.md）；完成冻结门前 G-SCI 不开放，
  WCS-002 保持「整改执行完成、科学 finding OPEN」。

## 裁决 2｜F-P1001-005（选择 A）

p2001/p2002/p2006 `utilization_p75_low` **派独立诊断节点**（`BASE-UTIL-001`），
用 BASE 主构建二进制归因；不得仅凭「BASE 预存失败」进入审核包豁免。
分类口径与记录项见 tasks/BASE-UTIL-001.md。须在 `RT-001` 与真实数据终验前闭环。

## 裁决 3｜F-AIO-002（选择 A）

p1hips tree_digest 多 HDU 归一化缺口**派独立小修复任务**（`P1-HIPS-DIGEST-001`），
仅测试域，独立提交，验收含连续 ≥10 轮跨秒全绿与 payload 敏感性负向验证。

## 管理意见

`P1-001` 保留一次独立架构抽验（`ARCH-AUDIT-P1`，独立 SubAgent 非原实现者自查），
五项确认见 tasks/ARCH-AUDIT-P1.md。

---

## 真实数据验收指令（负责人，2026-09-10）

本地已具备真实数据：`testdata/`（30G，944 文件）、`GaiaDR3/`（41G，16 个 .xpsd）、
`GaiaDR3/` 与 `GaiaDR3SP/`（64G，20 个 .xpsd）为真实 Gaia 数据库（用户资料区，禁改禁删）。

### 指令 1｜testdata 全量校准与解析正确执行

- 按 T1/T2/T3/T4 四套望远镜**正确匹配对应校准帧**（bias/dark/flat，含滤镜、曝光、binning、传感器尺寸）；
- **T1 无数据：显式空集**，仍须完成全枚举并 PASS（不得跳过、不得报错、不得伪造帧）；
- 其余三套的全部受支持文件（.fts 亮场 + .xisf 母版）**解析、校准匹配、平场和解算（plate solve）全部正确执行**；
- 匹配键与数据源：`testdata/index.json`（v1.1：望远镜、母版清单、滤镜 brand/model/bandwidth、
  filter_wheel、面板、逐面板滤镜计数、素材信息文件）。匹配确定性规则由 `REAL-000` 冻结并测试。

### 指令 2｜M42 与银心两套代表性数据完整链路

- `testdata/M42_T2T3_mosaic_Flying_dutchman/`（T2+T3 双望远镜、M1..M6 面板、196 帧亮场）
  与 `testdata/Galaxy_Center_T4/`（T4、panel1..3、157 帧亮场）：
  - **Phase1 完整链路**：每帧亮场 → 校准 → 解析 → 测光/SNR/Drizzle → 标准化单帧 HiPS + manifest；
  - **Phase2 完整链路**：以本数据集 Phase1 产品集为输入 → UPM/rejection/integration →
    马赛克 HiPS + variance/ivar/nused/nrej + 完整 provenance + manifest；
  - 各 Phase 独立进程、跨 Phase 仅经磁盘产品/manifest/哈希交换（宪章 §3.2）。

### 指令 3｜本地真实 Gaia 数据库

- Plate Solve 与星表路径必须使用本地真实数据库（不得用桩、不得联网替代）：
  - `GaiaDR3/`（db_type=GaiaDR3，32B 记录）用于两套代表性数据的解析链；
  - `GaiaDR3SP/`（db_type=GaiaDR3SP，384B 记录，DATA-GAIA-001）至少在一套数据（银心）
    完整走一次星表消费路径，覆盖两种 db_type 分支；
- 每帧记录查询命中数与缓存行为；零命中 → FAIL 并附诊断（中心坐标/半径/文件数）。

### 已知数据缺口与处理原则（盘点 2026-09-10，由 REAL-000 落成精确计数）

| 缺口 | 影响 | 冻结策略 | 禁止 |
|---|---|---|---|
| T2/T3 无 300s 暗场（M42 T2 53 帧、T3 94 帧） | 300s 亮场无精确曝光 dark | `docs/science/CALIBRATION.md:57,90`：dark 线性缩放 `K=t_light/t_dark`；`docs/algorithms/CALIBRATION_ALGORITHMS.md:158,292`：`OPTIMAL` fallback `EXPOSURE_RATIO`（配置默认 false，须显式开启并逐帧记录 K） | 不得即兴发明第四种缩放；不得静默跳帧 |
| T2 无 Lum 平场（NGC247 Lum 15 帧） | Lum 亮场无匹配 flat | 分类 `UNAVAILABLE(NO_LUM_FLAT)`，登记 finding 报负责人（补拍母版或扩库） | 不得用其他滤镜 flat 顶替 |
| M42 数据集未入 `testdata/index.json`（v1.1 仅 7 数据集/710 帧） | 匹配计划缺输入 | `REAL-000` 扩索引至 v1.2（multi-telescope mosaic schema），数据从 M42 素材信息文件提取 | 不得改名/移动任何数据文件 |
| 滤镜名大小写不一致（OIII/Oiii） | 匹配歧义 | 匹配器统一大小写不敏感归一（文档化于匹配计划），文件名不改 | 不得改数据文件 |
| T2/T3 传感器像元尺寸未记录 | 解析配置（focal/pixel_size）不完整 | 从素材信息文件提取；缺失则置 null 并登记 `OWNER_CONFIRM` finding | 不得凭相机型号臆造数值 |

### 验收底线（指令级）

1. 枚举对账：`count_enumerated == count_classified`，逐帧有记录，**零静默丢弃**；
2. 不可执行帧必须带机器可读 reason_code（如 `NO_LUM_FLAT`、`NO_MASTER_DARK_BEYOND_POLICY`）；
3. 校准/解析负向：错配（错滤镜/错曝光/错传感器/错望远镜）必须确定性拒绝；
4. T1 空集枚举 PASS 是显式用例，不是跳过；
5. Phase2 消费的每个 Phase1 产品必须通过 `contracts/data/phase_product_exchange.schema.json`
   与 matrix 拒绝码校验（含 variance/ivar 平面，DATA-001 冻结合同）；
6. 全部 heavy 区间按宪章 §10.5 资源门（平均 ≥85%，连续 10s<60% 或单活跃线程失败）；
7. 原始数据只读：产物一律落 `run/`、`artifacts/`、`reports/`，testdata/Gaia 目录禁写。
