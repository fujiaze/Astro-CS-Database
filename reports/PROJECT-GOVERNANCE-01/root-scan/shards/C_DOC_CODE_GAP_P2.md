# 分片 C_DOC_CODE_GAP_P2（ROOT-004 旧 bug 清单按最新权威订正）

- 分片名 `C_DOC_CODE_GAP_P2`｜类别 C_DOC_CODE_GAP｜优先级 P2｜分配 24 条，产出 24 行（+1 表头 = 25 行）。
- 产物 `reports/PROJECT-GOVERNANCE-01/root-scan/shards/C_DOC_CODE_GAP_P2.psv`（10 列 UTF-8，行序=分配表顺序）。
- 取证基线 `HEAD = main = 2c328348`，`origin/main = 5f891080`（只读取证；零修复、零 git 写）。
- 四态计数：OPEN 22 ｜ RESOLVED 2（M7-C-201、V1-N-11）｜ VOID 0 ｜ UNVERIFIABLE 0。
## ID 覆盖自证
命令与输出：
```bash
cd "/workspace/Astro CS Database"
timeout 60 wc -l reports/PROJECT-GOVERNANCE-01/root-scan/shards/C_DOC_CODE_GAP_P2.psv
timeout 60 awk -F"|" 'NR>1{print $1}' reports/PROJECT-GOVERNANCE-01/root-scan/shards/C_DOC_CODE_GAP_P2.psv | tr "\n" " "
timeout 60 awk -F"|" 'NR>1{print $7}' reports/PROJECT-GOVERNANCE-01/root-scan/shards/C_DOC_CODE_GAP_P2.psv | sort | uniq -c
timeout 60 awk -F"|" 'NR>1{print NF}' reports/PROJECT-GOVERNANCE-01/root-scan/shards/C_DOC_CODE_GAP_P2.psv | sort -u
```
```text
25 reports/PROJECT-GOVERNANCE-01/root-scan/shards/C_DOC_CODE_GAP_P2.psv
M2a-C-13 M2a-C-14 M2b-C-05 M2b-C-06 M2b-C-07 M3-C-013 M3-C-014 M4-C-08 M4-C-09 M6a-C-004 M7-C-201 M8-C-003 M8a-C-007 M8a-C-008 M8a-C-009 V1-N-03 V1-N-05 V1-N-06 V1-N-07 V1-N-11 V3-N-01 V3-N-02 V3-N-03 V5-N-01
     22 OPEN
      2 RESOLVED
10
```
ID 与 `_assign/C_DOC_CODE_GAP_P2.tsv`（同命令取 $1 得同串）逐字逐序一致；每行 NF=10，列内无 `|`、无换行。
## UNVERIFIABLE 清单
无（0 条）。
## 异常
1. **基线漂移**：派发声明 HEAD=main=ecf6ad6f，实测 HEAD=main=2c328348（领先 2 提交 5f891080、2c328348），origin/main=5f891080；全部按现树重取证，未沿用旧行号。
2. **路径锚漂移**（原路径不在、等价物在位）：`lib/hips/types.h`→`lib/hips/include/astrocs/hips/types.h`；`lib/plate_solve/src/ipv_triangle.cpp`→`lib/plate_solve/cpp/ipv/src/ipv_triangle.cpp`。
3. **条款引用失效**：V3-N-01/N-02 引 AGENTS.md「科学定义=算法=接口=代码=测试 五处同源」，现 AGENTS.md 该串 0 命中，已改挂 ENGINEERING_SPEC §8/§9；结论不变。
4. **子锚死亡**：V1-N-06 的 N-06b（`snr_estimator.cpp:1119-1121`）不存在（实测 917 行、`support` 0 命中），该子项需重定位；v1 未初始化新字段主判据已在树内复现 ⇒ 整行仍 OPEN。
5. **判据文档自变**：M7-C-201 目标文档 `docs/algorithms/PHASE3_FITS_IMPL.md` 全文「精度」0 命中且 :205 明说 -64 上游为 f32 ⇒ 原声明无逐字对应文本，判 RESOLVED。
6. **消费方漂移**：M8a-C-008 原写 `tests/unit/cpu_features_test.cpp` 消费 `acs_cap_classify_v1`，实测消费方为 `tests/cpu/dispatch/cpu_capability_matrix_test.c:68` 与 `tests/cpu/avx512/check_avx512_illegal_instr.py:50`；README 缺行与 FROZEN 两项本身仍在。
7. **半数为非权威链文档**：M2b-C-07 的 `docs/standards/STANDARDS_REGISTRY.md:224` 已非权威链，该半只按「活动文档事实性断言失真」计；另一半 `docs/algorithms/PHASE3_PROJ_IMPL.md:12/:51`「165 行」在链上 ⇒ 整行 OPEN。
8. **未跑构建/未跑 Windows**：M8a-C-007 为静态读入判断（未跑 MSVC）；V1-N-11 未做运行期实测，判定依据是「串行前提消失」而非性能数。
9. **GAP 关系**：24 条逐条比对 GAP_AUDIT.md 的 GAP-001..GAP-025（含 U-01..U-08），无重复登记，第 9 列全为「无」。
10. 本分片只写 3 个文件（.psv、本 .md、`run/PROJECT-GOVERNANCE-01/ROOT-004/logs/shards/C_DOC_CODE_GAP_P2.log`）；未触碰代码/文档/测试/CI/根条目。
