# D_COMMENT_P1 分片执行报告（ROOT-004 分片执行层）

- 分片名：`D_COMMENT_P1`（D_COMMENT × P1，分配 32 条：L28b-D-001 .. W5-N-06）
- 产物：`reports/PROJECT-GOVERNANCE-01/root-scan/shards/D_COMMENT_P1.psv`（表头 1 行 + 32 行 = 33 行，10 列）
- 四态计数：**OPEN 32 / RESOLVED 0 / VOID 0 / UNVERIFIABLE 0**；归属：NP-DC-01 8、NP-DC-02 15、NP-DC-03 1、GOV-001 2、OBS-001 1、CPU-001 2、CLI-003 2、AIO-001 1
- GAP 比对：逐条对 `工程控制/PROJECT-GOVERNANCE-01/GAP_AUDIT.md`（GAP-001..025 + U-01..U-08）比对，**无一条重复**，第 9 列全为 `无`。

## ID 覆盖自证

```
$ python3 - <<'PY'  # csv 解析 PSV
TOTAL_LINES 33
HEADER_COLS 10
BAD []
N_IDS 32
IDS L28b-D-001 L28b-D-002 L28b-D-003 L28b-D-005 L28c-D-001 L28d-D-001 L28e-D-001 M1a-D-001
    M3-D-001 M3-D-002 M3-D-003 M4-D-01 M4-D-02 M5a-D-001 M6a-D-001 M6a-D-002 M6a-D-003
    M6a-D-004 M6a-D-005 M6a-D-006 M6a-D-010 V8-N-01 V8-N-02 V8-N-04 V8-N-07 W1-N-02
    W5-N-01 W5-N-02 W5-N-03 W5-N-04 W5-N-05 W5-N-06
$ awk -F'|' '{print NF}' D_COMMENT_P1.psv | sort -u   # 全 33 行
10
```

分配表 32 行与 PSV 32 行 ID 逐字一一对应、行序一致（分配表第 2..33 行）。

## 异常与口径记录

1. **基线漂移**：派发写 `HEAD=main=ecf6ad6f`，开工实测 `HEAD=main=2c328348`（领先 2 个治理文档提交），收尾实测 `HEAD=939d3f6c`（并发线提交前移），`origin/main=f96dff61`；全部判定按**取证时点当前树**，零 git 写、零跟踪文件改动。
2. **行锚大面积漂移**：ipv_api.h 字段表 :203-219→:229-234、sdet_api.cpp 常量块 :1783-1789→:1819-1825、aio_api.cpp :20→:19-25 等；漂移只作 E_TRACE_BREAK 类证据，不改结论。
3. **原报数字被本轮复算修正（结论方向不变者）**：
   - `L28d-D-001`：原判「实现按像素比较、有效阈=宣称值 2 倍」**被推翻**——`ipv_select.cpp:951` U=像素、`:1098-1099` W=角秒，`pred=A·U+t` 与 W 同域 ⇒ dist2 真域=角秒²，MAX_DIST=100 与原实现 50px×2"/px 等价；同址仍有单位注释写反（`residual_px`(:943) 注「像素」与 `:1012` 自述「角秒」互斥、`:10` 把 U 写成角秒），故仍 OPEN 但判词按复算订正。
   - `V8-N-04`：原报「15 处、12 处无前缀」本轮同口径重算为 **3 处**裸锚（star_detector.cpp:19、module_adapters.cpp:1604、dpsf_psf.cpp:112）；根 `REPORT.md` 仍不存在，现象成立。
   - `W5-N-01`：原报「:254（IoExecutor 同形）」只对一半——IoExecutor 同样吞异常但不计数、不写 COMPLETED；观测面污染限于 CpuHeavyExecutor(:137-151)。
   - `L28e-D-001`：范围锚命中行本轮实测 90（本域口径：lib/cli/include/tools/ci/cmake/packaging/tests，排除 run/build/out/artifacts/evidence/reports/graph/worktrees/Testing/logs/third_party），不复用原报 492/136。
4. **引用面已消失**：`M3-D-002` 原引 `docs/architecture/PUBLIC_API.md` 不存在，`docs/contracts/API_CONTRACTS.csv` 内 `API-CAL-*` 零命中 ⇒「已登记偏差 API-CAL-008」无可复核落点（已写入该行第 10 列）。
5. **D_COMMENT 无归属任务**：30 任务表无「注释-实现一致性」任务，多数条目归一 `NEXT-PACK:NP-DC-01/02/03`（注释锚与溯源／注释与实现矛盾／公共头注释要素缺口）；W5 吞错族按现有任务归 OBS-001/CPU-001/CLI-003/AIO-001；两处登记面缺口（DISP-WCS-007/008、负责人裁决 2026-09-14 两族）归 GOV-001。

## UNVERIFIABLE 清单

无。32 条均在当前树取得命令证据；不可复算的数字（如 M6a-D-010 的 15%/10%/55% 为 20 样本判读值）已在该行第 10 列注明「不作规模结论」，未以之支撑结论。
