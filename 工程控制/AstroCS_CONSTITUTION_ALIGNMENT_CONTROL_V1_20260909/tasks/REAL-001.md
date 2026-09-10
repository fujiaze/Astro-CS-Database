# REAL-001｜Linux 最终 SHA 全量真实数据终验（含 M42/银心完整 Phase1/Phase2 链路）

> 依据：`04_OWNER_DECISIONS_20260910.md` 真实数据验收指令；前置：`REAL-000` 匹配计划已冻结、
> `CI-002` 同 SHA Linux CI 通过。本任务在**当前最终候选 SHA**上执行，不得用中间 SHA 冒充。

## 目标（三层，全部独立进程）

### 层 A｜全量校准与解析正确执行（testdata 除 T1 外全部数据集）

- 数据集：LDN43_T2(42)、NGC1727_T2(64)、NGC247_T2(68)、NGC55_T3(79)、NGC83_cluster_T3(72)、
  Victory_Nebula_T4(228)——共 553 帧亮场；
- 每帧：解析（FITS 头读取）→ 按 `run/realdata/match_plan.json` 确定性匹配校准母版 →
  执行校准（含策略性 dark 缩放，逐帧记录 K/策略码）→ **plate solve**（gaia_data_dir=本地真实
  `GaiaDR3/`，记录命中数）；
- 校准帧匹配错误必须确定性拒绝（错滤镜/错曝光/错传感器/错望远镜不得静默通过）；
- 逐帧记录：输入/输出文件、所用 master、K、solve 结果（ra/dec/rms 或 reason_code）、rc；
- **T1 显式空集**：枚举 0 帧 0 母版 → 记录 `T1: empty(PASS)` 用例；
- `UNAVAILABLE(NO_LUM_FLAT)` 等计划内不可执行帧：如实分类计数，不阻塞其余帧。

### 层 B｜M42 与银心两套代表性数据完整链路（宪章 §5/§6 产品语义）

- **Phase1 完整产品**（每帧，共 196+157=353 帧）：
  M42_T2T3（T2 73 + T3 123）、Galaxy_Center_T4（panel1..3）；
  每帧 → 标准化单帧 HiPS（signal/support/variance/properties/MOC）+ manifest（status=complete，
  输入/配置/输出哈希齐全）；gaia_data_dir 指向本地真实数据库；
- **Phase2 完整产品**（分滤镜马赛克，跨面板；M42 4 个：Blue/Green/Red/H-alpha；
  银心 5 个：Blue/Green/Red/H-alpha/OIII）：
  - 输入 = 对应滤镜全部 Phase1 单帧 HiPS（面板跨 T2+T3 或 panel1..3）；
  - 独立进程消费磁盘产品（宪章 §3.2 跨 Phase 交换规则）；
  - 产品集逐个过 `contracts/data/phase_product_exchange.schema.json` + matrix 拒绝码校验
    （variance/ivar 必需，DATA-001 冻结合同）；
  - 输出马赛克 HiPS + UPM/rejection/integration provenance + diagnostics + manifest；
  - 银心星表路径额外用 `GaiaDR3SP/` 完整走一次（db_type=GaiaDR3SP 分支覆盖）；
- 失败语义：任何帧 FAIL 无 reason_code = 本任务 FAIL；任何 Phase1 产品缺必需平面 = FAIL。

### 层 C｜机器证据

- `run/realdata/<dataset>/`：逐帧记录 JSONL、stage_state.json（断点续跑）、
  每滤镜 Phase2 配置与 manifest 链；
- `artifacts/realdata/`：两套数据集的 manifest 链快照（哈希树）、资源时序（heavy 区间按宪章 §10.5）；
- `reports/realdata/REAL-001-REPORT.md`：对账表（枚举=分类）、缺口计数、solve 统计、
  Phase2 每滤镜 nused/nrej/覆盖率、资源门结论；
- heavy 区间资源监控证据（平均 ≥85%、连续 10s<60% 或单活跃线程即 FAIL）。

## 写入白名单

- `run/`（运行产物与断点）
- `artifacts/`（证据封装）
- `reports/realdata/`
- 禁写：testdata、GaiaDR3、GaiaDR3SP、BASS DR3 数据文件（只读）。

## 执行与断点

- timeout 259200s（3 天）；分阶段：A 层按数据集、B 层按 数据集×Phase；
- 每 stage 落 stage_state.json（输入哈希、已完成帧清单、游标），重跑只续未完成帧；
- 全部命令 timeout/cwd/argv/起止/rc/SHA 入日志；单帧超时单独记录不阻塞批次。

## 非目标与禁令

- 不修改科学公式/容差/冻结匹配策略；发现新缺口 → reason_code + finding，不即兴；
- 不在 Fatduck 执行（那是 WIN-001）；不上传原始数据；
- 不因 P2/P3 实现缺陷顺手改实现——链路缺陷登记 finding 移交对应任务域。

## 验收

1. 对账：`count_enumerated(906) == count_classified(906)`；T1 空集 PASS；
2. A 层：553 帧全部「校准+解算执行」或带 reason_code；solve 命中数>0 的帧占比入报告；
3. B 层：353 帧全部产出 complete 单帧 HiPS（schema 校验过）；9 个 Phase2 马赛克产品
   全部 COMPLETE + provenance 链完整（输入哈希可回放）；
4. 资源门：全部 heavy 区间过宪章 §10.5 或有冻结豁免码（如区间<10s）；
5. 恒定场/守恒抽验：至少 2 个马赛克做 support 覆盖率与 signal 量级 sanity（与单帧中位一致性）；
6. `ci/reconcile_state.py --strict` rc=0；证据绑定最终 SHA 与数据 manifest。

## 返回证据

scope/acceptance/provenance 三检查 + 上述全部产物 + 失败/缺口清单 + OWNER 待决清单。
