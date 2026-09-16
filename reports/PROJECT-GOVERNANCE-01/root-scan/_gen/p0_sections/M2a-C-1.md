## M2a-C-1 DLL 通道把 out_match_idx 截断为 matched_count 个元素，破坏"坐标序 + −1 表示未匹配"合同；部分未命中时行↔坐标映射不可恢复

- 类别: C_DOC_CODE_GAP（合同↔实现行为差异，改变产品结构）
- 优先级: P0
- 来源: L10-002（前台指定的逐条确认项之一）
- 复核时点结论: **仍成立**（三项前置核实全部通过，未发现任何"部分未命中"用例，故不降级）
- 位置: docs/contracts/DATA_SEMANTICS.md::§8.2 输出行 out_match_idx 行 + shape 契约段；lib/gaia_xpsd_client/src/module_entry.c::build_result_json（match_idx 编码与 \"count\" 输出段）；lib/gaia_xpsd_client/src/gaia_client.c::gaia_client_query_spectrum_by_coords（match_idx 分配/回填段）；tests/unit/gaia_adapter_test.c::D5；tests/unit/gaia_integration_test.c::by_coords 段
- 证据摘录（逐字，复核时点现文）:
  > （DATA_SEMANTICS.md:113）| out_match_idx | int32 | **坐标序** | **−1 = 该坐标未匹配** |
  > （DATA_SEMANTICS.md:117-119）shape 契约：`out_stars` 为 `out_count` 行连续数组（C ABI 顶层 malloc，调用方 free）；`out_spectra` 为 `out_count × global_spec_count` 字节…
  > （gaia_client.c:2293-2326 区段）int *match_idx = (int *)calloc(n_coords, sizeof(int)); … if (best_idx >= 0) { … match_idx[i] = matched_count;  … } else { match_idx[i] = -1; } … *out_count = matched_count;   // 分配与回填均按输入坐标序、长度 n_coords
  > （module_entry.c:732）uint64_t b64_idx_cap  = match_idx ? b64_encoded_len((uint64_t)count * sizeof(int)) : 3;   // count = matched_count
  > （module_entry.c:743）\"...\"count\":%d,\" 与 （:760-763）else if (!strcmp(c.op, ASTROCS_GAIA_OP_SPEC_BY_COORDS)) schema = {...,\"match_idx\":\"i32\"}   // 载荷只给 count 个整数，无 n_coords 回显
  > （gaia_adapter_test.c:592-607 区段）NC=8 由 find_star 预筛 → CHECK(drc == 0 && dn == NC)   // 断言"全部命中"
  > （gaia_integration_test.c by_coords 段）单坐标查询、单命中 → dn==1
- 权威依据: DATA-GAIA-001（docs/contracts/DATA_SEMANTICS.md §8.2 冻结输出行）；宪章 §4.3（产品结构与语义须记录一致）、§12.3-1/-5（合同↔实现↔schema 一致）、§7.3（不支持/降级语义须显式，不得静默改写）
- 问题说明: 底层 C API 的 match_idx 是**长度为 n_coords 的坐标序数组**（命中元素存行号，未命中存 −1），而 JSON 载荷编码时把它的长度取成 `count = matched_count`。因为同一函数已把结果行压缩为"仅命中行连续存放"（rows compacted），坐标序与行序是两个不同的序：一旦有任一坐标未命中，坐标 ≥ matched_count 的那些元素（含它们的 −1 与其真实行号）在载荷里根本不存在，且载荷不携带 n_coords。消费方拿到的第 k 个整数**既不是第 k 个坐标的映射，也不是行序索引**，坐标↔行的对应关系不可恢复。合同面 (a) 明确写"坐标序 + −1 = 未匹配"，(b) 对 out_stars/out_spectra 给了 `out_count` 口径的 shape 段但对 match_idx 只写序不写长度——实现按后者最短解释执行。核实"测试是否全命中"以判断能否降级：注册面唯一的 by_coords 结构断言（gaia_adapter_test D5）用 find_star 预筛坐标保证 8/8 全命中，并显式断言 `dn == NC`；按字节比对的用例也以 `dn*sizeof(int)` 为长度；另一处 gaia_sanitize_driver 只喂 2 坐标且属内存消毒驱动、非合同测试。→ **不存在部分未命中用例，不降级**。
- 影响: 真实参考星场内必然出现部分坐标无匹配（稀疏目录、边缘坐标、星等窗外）；此时 DLL 通道的星表行与输入坐标的对应关系丢失或被错配，下游按 match_idx 反查即为**星 ID 错位级**污染（HSS 重建/测光标定引用错误参考星）。当前生产链走 C 直连 API（不受影响），但 astrocs_catalog_gaia.dll 是已注册 entrypoint 的对外 ABI 面（module.yaml:20），运行时 typed DAG 接线（SA-RT-05 承接）一旦启用即触发。
- 建议处置: ① match_idx 编码长度改为 n_coords（并在载荷加 n_coords 字段），合同 §8.2 该行补写"长度 = n_coords"；或 ② 若决定按行序语义，须改合同为"行序 + 行→坐标索引"并同步 C API；③ 补注册测试：构造"部分未命中"（含首坐标未命中、末坐标未命中、全未命中）三例，断言载荷元素个数与 −1 位置；④ 该项与 M2a-A-3（by_coords 判据未文档化）同批处置。
- 置信度: 高（合同原文、分配/回填/编码三处代码、两处测试断言均一手复读）
- related: L10-018（by_coords 判据未入文档）、M2a-A-3、L10-011（同域追溯锚）
