# E_TRACE_P2_J_FS 分片报告（ROOT-004 旧 bug 清单按最新权威订正）

- 分片名 `E_TRACE_P2_J_FS`｜分配 27 条（`L28e-E-004` .. `W4-R2-08`；类别 E_TRACE_BREAK / J_FS_PUBLISH）
- 产物 `reports/PROJECT-GOVERNANCE-01/root-scan/shards/E_TRACE_P2_J_FS.psv`：表头 1 行 + 27 行 = 28 行，逐行 10 列
- 四态计数：**OPEN 27 ｜ RESOLVED 0 ｜ VOID 0 ｜ UNVERIFIABLE 0**
- 判定时点 HEAD=main=`2c328348`；命令日志 `run/PROJECT-GOVERNANCE-01/ROOT-004/logs/shards/E_TRACE_P2_J_FS.log`

## 1. ID 覆盖自证（命令 + 原始输出）

```
$ wc -l reports/PROJECT-GOVERNANCE-01/root-scan/shards/E_TRACE_P2_J_FS.psv
28 reports/PROJECT-GOVERNANCE-01/root-scan/shards/E_TRACE_P2_J_FS.psv
$ awk -F'|' 'NR>1{print NF}' reports/PROJECT-GOVERNANCE-01/root-scan/shards/E_TRACE_P2_J_FS.psv | sort -u
10
$ python3 比对 psv 第1列 与 _assign/E_TRACE_P2_J_FS.tsv 第1列（顺序敏感）
psv_lines=28 data_rows=27
field_counts=[10]
ids_in_order_identical=True
first=L28e-E-004 last=W4-R2-08
```

## 2. 异常（不影响行数与结论）

1. **基线漂移**：brief 写 HEAD=ecf6ad6f，实际 HEAD=main=2c328348（+2 commit）。`git diff --stat ecf6ad6f..HEAD` 仅 `工程控制/PROJECT-GOVERNANCE-01/**` 12 文件（治理文档），产品面零改动 ⇒ 无一条可因修复转 RESOLVED，与 27/27 OPEN 自洽。
2. **M2a-E-5 子项不成立**：第三个函数名 `test_missing_required_each` 不在 DATA-002 现文（仅 tests/artifact/test_manifest_schema_negative.py:5 注释）；证据已按现文收窄。
3. **M6a-E-002 计数更新**：finding 记 39 处且称「全部落在 sdet_api.cpp」；本轮复算 40 处（sdet_api.cpp 39 + test/sdet_saturation_cursor_test.cpp 1），PSF.c 锚行号由 :2285 漂至 :2321。
4. **M6b-E-005 锚规模**：ANCHOR_CONTRACT.md:18 自述 36 文档/793 锚；本轮口径（docs/science+docs/algorithms 的 `path.ext:N`）复算 43 文档（37 含锚）/814 锚，口径差异已写入 PSV 备注列。
5. **SA-N-05 分组口径缺失**：「硬悬空 48 种/55 出现」的分组未在 SA_anchor_repair.json 的 meta 给出，未能逐字复算；可复算项为产物名 34/75、外链 7/18（与 finding 相同）。
6. **W4-R2-08 普遍量词不成立**：树内 `lib/astro_image_io/v6/src/v6_atomic_publish.cpp:325/330` 与 `runtime/io/hips_output_store.py:216` 有 O_EXCL 用法（非 vendored），故「全仓独占创建 0 命中」不采；本条四站点缺独占创建的实质仍成立。
7. **已订正子项（不改变 OPEN）**：M5b-E-06 的 ERROR_MODEL 词表被改写；M6b-E-005 唯一性表述补「共享引用只登记不判重」；M6b-E-006 的 `$F.2` 已消失；W4-R2-03 根残留已被清运（测试代码回退仍在）。
8. **位置锚漂移**（已按当前树重定位；漂移本身即 E_TRACE_BREAK 证据）：M1a-E-004 §8→§10、§15→§13；M8-E-001 CMake :103/:110→:123/:130；M7-E-201 多处行号。
9. **M7-E-201 表内符号**：`::build_fits_wcs_from_solution` 非真实符号（与 SA-N-02 同判），该条已改用 `ipv_wcs.cpp:651` 的 Y-down 块重述。
10. **SA 轴口径说明**：SA-N-01/03/05 的语料是 `问题扫描/findings/**`（审计档案，非产品文档面），判据 E4/C-18 为扫描协议内部编号，第 5 列已改写为现行权威 ENGINEERING_SPEC §8。

## 3. UNVERIFIABLE 清单

无（0 条）。SA-N-01/03 的台账口径可由 tracked 的 `问题扫描/_verify/SA_anchor_repair.json` 用命令复算，故不判 UNVERIFIABLE；未能复算的点（SA-N-05 48/55）已在其第 10 列写明缺什么。
