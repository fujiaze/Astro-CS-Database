# 分片 A_SCI_DEF_P0（ROOT-004）执行说明

- 分片名：A_SCI_DEF_P0（原类别 A_SCI_DEF / 原优先级 P0，15 条：M1a-A-001..M7-A-002）
- 产物：`reports/PROJECT-GOVERNANCE-01/root-scan/shards/A_SCI_DEF_P0.psv`（表头 1 行 + 15 行，每行 10 列）
- 行数自证：命令 `timeout 60 wc -l reports/PROJECT-GOVERNANCE-01/root-scan/shards/A_SCI_DEF_P0.psv`；输出 `16 reports/PROJECT-GOVERNANCE-01/root-scan/shards/A_SCI_DEF_P0.psv`
- 列数自证：命令 `timeout 60 awk -F"|" '{print NF}' reports/PROJECT-GOVERNANCE-01/root-scan/shards/A_SCI_DEF_P0.psv | sort -u`；输出 `10`
- ID 覆盖自证（顺序与分配表逐行一致）：命令 `cd "/workspace/Astro CS Database" && timeout 60 diff <(tail -n +2 reports/PROJECT-GOVERNANCE-01/root-scan/shards/A_SCI_DEF_P0.psv | cut -d"|" -f1) <(tail -n +2 reports/PROJECT-GOVERNANCE-01/root-scan/shards/_assign/A_SCI_DEF_P0.tsv | cut -f1) && echo ID_ORDER_MATCH_15`；输出 `ID_ORDER_MATCH_15`
- 四态计数：OPEN 14 / RESOLVED 1 / VOID 0 / UNVERIFIABLE 0
- 归属计数：P1-001 5、P1-002 1、P2-001 2、P2-002 3、P3-001 1、AIO-001 2、DATA-001 1
- 命令日志：`run/PROJECT-GOVERNANCE-01/ROOT-004/logs/shards/A_SCI_DEF_P0.log`

## 异常
1. 基线漂移：简报声明 HEAD=main=ecf6ad6f、origin/main=f96dff61；本轮实测 HEAD=main=2c328348、origin/main=5f891080（GAP_AUDIT §5.1 亦记 ecf6ad6f）。全部判定按工作树当前内容执行，未按声明 SHA。
2. M1a-A-001 锚漂移：`docs/science/ASTROMETRY.md:48` 公式与 :66 一致性句逐字仍在，但 finding 所引 `ipv_wcs.cpp` 行区间(:274-420/:530-576)已不覆盖牛顿段（现 :918）；§5a:75 仍重复该式。
3. M1a-A-002：finding 的 1.8× 换算依赖 s0≈0.989″/px，该值在当前 `lib/plate_solve/memory.md` 无命中，故只复算记录内可复算的 5.9–7.2× 内外差与「无偏差登记」（grep 外部闭环/全帧头域 = 0 命中），其余逐字复现。另：原判据引的 `docs/modules/registry/astrocs.phase1.wcs-platesolve.md` 不在最新权威链，第 5 列改用 `docs/algorithms/PLATESOLVE.md §11.4` F1 与 `ASTROCS_DESIGN.md §11.3/§12`。
4. M3-A-001 由 OPEN 改判 RESOLVED：当前树已落地 P5-SNR（2026-09-14），控制点值改为 Horne 1986 SNR_F，`CONTROL_WEIGHT_SNR.md §2a` 与 `NOISE_MODEL.md §9a` 同步重定义；其未撤的 SCI-CW §4 `support×snr_v²` 已单列在 M3-A-002。
5. M3b-A-03 的装配码位于 `lib/orchestrator`（不在根 CMake 交付图），但 `DATA_SEMANTICS.md:586` 的合同文本属活动规范层，故按合同面判 OPEN 并已在该行备注标注可达性。
6. 未找到原文的条款：无；15 条引用的节号/原文均经本轮 read/grep 逐条核对。无法定位的 ID：无。

## UNVERIFIABLE 清单
- 无（0 条）。所有分片条目均取得当前树的可复跑命令证据，未出现缺构建/缺 Windows 节点/缺数据导致的判不动项。
