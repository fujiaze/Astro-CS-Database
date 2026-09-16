# 分片 C_DOC_CODE_GAP_P0（ROOT-004 旧 bug 清单按最新权威订正 · 分片执行）

- 分片名：C_DOC_CODE_GAP_P0；类别 C_DOC_CODE_GAP；优先级 P0；分配 18 条（M1a-C-001 .. M7-C-001）
- 产物：reports/PROJECT-GOVERNANCE-01/root-scan/shards/C_DOC_CODE_GAP_P0.psv（表头 1 行 + 数据 18 行，10 列）
- 四态计数：OPEN 10 / RESOLVED 8 / VOID 0 / UNVERIFIABLE 0
- 基线实况：本机 HEAD = main = origin/main = 2c328348304d033aecfa81faf79d1c6cd802b30a（与简报所写 ecf6ad6f / origin/main=f96dff61 不同；见"异常"）

## 1. 格式自证（本轮真跑）

命令：python3 run/PROJECT-GOVERNANCE-01/ROOT-004/logs/shards/_v.py reports/PROJECT-GOVERNANCE-01/root-scan/shards/C_DOC_CODE_GAP_P0.psv reports/PROJECT-GOVERNANCE-01/root-scan/shards/_assign/C_DOC_CODE_GAP_P0.tsv
输出：
header_exact_match: True
data_rows: 18 expected: 18
cols_and_order_ok: True
states: {'OPEN': 10, 'RESOLVED': 8}

## 2. ID 覆盖自证

命令：cut -d'|' -f1 C_DOC_CODE_GAP_P0.psv | tail -n +2 | tr '\n' ' '
输出：M1a-C-001 M1a-C-002 M1a-C-003 M1a-C-004 M2a-C-1 M2b-C-01 M3-C-001 M3-C-002 M3-C-003 M3b-C-01 M3b-C-02 M4-C-01 M4-C-02 M4-C-03 M5b-C-01 M5b-C-02 M6a-C-001 M7-C-001
命令：cut -f1 _assign/C_DOC_CODE_GAP_P0.tsv | tail -n +2 | tr '\n' ' '
输出：M1a-C-001 M1a-C-002 M1a-C-003 M1a-C-004 M2a-C-1 M2b-C-01 M3-C-001 M3-C-002 M3-C-003 M3b-C-01 M3b-C-02 M4-C-01 M4-C-02 M4-C-03 M5b-C-01 M5b-C-02 M6a-C-001 M7-C-001
→ 两串逐字相同，顺序一致，无缺号无重号。

## 3. UNVERIFIABLE 清单

无（0 条）。18 条全部可判定：判 OPEN 的 10 条均给出当前树违反最新权威的可复跑证据；判 RESOLVED 的 8 条均给出"对象已不存在/已改"的可复跑证据（M1a-C-002 success=false 分支、M2a-C-1 合同 n_coords + 载荷 n_coords、M3-C-001 星数门 <3 与 >=2、M3-C-002 quality_flags 过滤、M4-C-01 两段分段线性、M4-C-02 两装配显式 1e-3、M4-C-03 fail-closed 与计数、M7-C-001 cfg.grid 与 grid!=8 硬门）。

## 4. 异常与说明

1. 基线不一致：简报写 HEAD=main=ecf6ad6f、origin/main=f96dff61，实测三 SHA 均为 2c328348…。本片一律按实测当前树取证。
2. 行号漂移普遍存在（分配表给的旧行号已失效），已按 path::符号 在当前树重定位并在第 6 列写实测行号；漂移本身未当作结论。
3. M2b-C-01 缺陷面收窄：三处"AIO-002 原子发布原语内建"注释中，p1_op_writer 那一处已消失，仅存 module_adapters.cpp:28 与 :4602，但 writer 直写与 0 原子原语未变，故仍判 OPEN。
4. M5b-C-02 与 GAP-018 证据重复（product.json 单元状态），按简报要求重复不合并、保留原 ID，第 9 列标"与 GAP-018 重复"。
5. 本片无 VOID：18 条的原判据无一条因旧条款撤销/被替换为相反要求而不构成偏差。
6. 未触碰 lib/ cli/ tests/ contracts/ ci/ tools/ docs/ 及仓库根任何条目；唯一写入为本 .psv、本 .md 与 run/ 下日志。
