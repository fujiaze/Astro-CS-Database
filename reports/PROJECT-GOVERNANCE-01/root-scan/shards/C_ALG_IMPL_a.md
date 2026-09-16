# 分片 C_ALG_IMPL_a（ROOT-004 分片执行层）

- 分片名 `C_ALG_IMPL_a`｜分配 35 条（V10-N-01..V10-N-08，C_ALG_IMPL，P1/P2）
- 产物 `reports/PROJECT-GOVERNANCE-01/root-scan/shards/C_ALG_IMPL_a.psv`（表头 1 + 数据 35 = 36 行，10 列，无列内 `|`/换行）
- 四态计数 **OPEN 35｜RESOLVED 0｜VOID 0｜UNVERIFIABLE 0**｜日志 `run/PROJECT-GOVERNANCE-01/ROOT-004/logs/shards/C_ALG_IMPL_a.log`

## 1. ID 覆盖自证（命令 + 逐字输出）
```
$ PSV=reports/PROJECT-GOVERNANCE-01/root-scan/shards/C_ALG_IMPL_a.psv
$ ASSIGN=reports/PROJECT-GOVERNANCE-01/root-scan/shards/_assign/C_ALG_IMPL_a.tsv
$ echo "psv lines: $(wc -l < $PSV)"; head -1 $PSV; echo "assign rows: $(($(wc -l < $ASSIGN)-1)) psv data rows: $(($(wc -l < $PSV)-1))"
psv lines: 36
psv header: ID|原类别|原优先级|旧判据(文档+节号/路径)|最新权威条款|当前证据(命令+输出)|结论|归属|GAP关系|备注
assign rows: 35 psv data rows: 35
$ comm -23 <(tail -n +2 $ASSIGN|cut -f1|sort) <(tail -n +2 $PSV|cut -d'|' -f1|sort)   # psv 缺的 ID →（空）
$ comm -13 <(tail -n +2 $ASSIGN|cut -f1|sort) <(tail -n +2 $PSV|cut -d'|' -f1|sort)   # psv 多的 ID →（空）
$ diff <(tail -n +2 $ASSIGN|cut -f1) <(tail -n +2 $PSV|cut -d'|' -f1) && echo "order identical: YES"
order identical: YES
$ awk -F'|' 'NF!=10{printf "%d:%d ", NR, NF}' $PSV   # 列数违规 →（空）；结论分布：35 OPEN
```

## 2. UNVERIFIABLE 清单
**无。** 35 条均可在当前树静态复现，第 6 列给出本轮真跑的命令与逐字输出。

## 3. 异常
1. **基线漂移（最重）**：任务书声明基线 `ecf6ad6f`，开工时 HEAD=`2c328348`、收尾时=`c44adc08`（前台持续提交治理与根清洁）。`git diff --name-only ecf6ad6f..HEAD` 实测改动分布：`设计大纲/` 344（已删目录）、`工程控制/PROJECT-GOVERNANCE-01/` 15、`ci/checks.json`+`ci/root_manifest.json`、`tools/quality/check_root_cleanliness.py`、`tests/quality/test_root_cleanliness.py`、`.gitignore` —— **无一条落在我 35 条取证的文件面**（lib/**、cli/**、providers/**、runtime/**、packaging/**、cmake/**、scripts/**、tools/check_warning_suppression.py、tools/quality/contracts/**、contracts/**）。已在新 HEAD 复点代表锚点（fits_reader.cpp:425、runtime.cpp:121、commands.cpp:934、snr_estimator 未跟踪态），逐字一致。
2. **分配表路径笔误（V10-N-07/N-08）**：`文件` 列写 `p1/V10-c.md`，实际在 `p2/V10-c.md`（p1/ 无此文件）；已按实际路径读取并在 PSV 第 4 列注明。
3. **原 finding 锚点偏差（PSV 备注已记，不改结论）**：①V7-N-04 把 CD 解析归因给 `hp_drizzle_api.cpp:474` 裸 atof，该行实为 SIP 系数，CD 走 `aio_frame_kv_get_double`（strtod，仅 end==val 拒），「无 isfinite 门」不变；②V21-N-15/16 引的「07 §1/§3/§4」不在权威链，改用 `ENGINEERING_SPEC §8/§10` + `ASTROCS_DESIGN §8`。
4. **行号漂移（重定位后判定不变）**：V21-N-15 `:912/:946`→`:934/:974`；V21-N-16 `:901`→`:923`；V10-N-03 `:1213`→`:1269`；V7-N-07 `:2891`→`:2966`；V10-N-01 消费点 `:1808/1826/1833`→`:1896/1900/1905/1911`。
5. **集合类口径重算，与原 finding 不一致处不混用**：W6-N-02 本轮 26/145（原文 18/135）；W6-N-04 本轮 336/191/13/29（原文 124/20/81/70）；两处以逐点可复跑站点为判据。V21-N-13 的「234 次写/100 键」总数未复算，只逐键复算 14 键零回读。
6. **需真机/运行期才能定的子项（PSV 备注已写「未判」，不影响本条 OPEN）**：V10-N-06 bad_alloc 是否必现；W6-N-03 交付树是否真多 2 个 schema 文件（需 `cmake --install` 清点）；W6-N-05 实际导出符号面（需 Windows dumpbin）。
7. **纪律**：外部命令全部带 `timeout`；`FATDUCK_ACCESS.md` 未 read/未打印/未复制；零修复、零 git 写、未碰禁改路径与仓库根条目；仅写本分片 `.psv`/`.md` 与分片日志。

## 4. 最重要 3 条 OPEN（判词）
- **V10-N-01**：`fits_reader.cpp:425-431/514-515/530` 短读仍 `return true` 且 `img.width/height` 用头声明值，drizzle 输入侧按 `pixels[y*img.width+x]`（drizzle_engine.cpp:1896/1900/1905/1911）索引 ⇒ 截断 FITS 直进越界读，输入侧零长度校验。
- **V21-N-12**：manifest 完整性两面被「期望为空」短路——两平台 `units[].sha256` 10/10 恒 null、`module_registry.c:735` 比对被 `sha_registered[0]` 短路、`:780` 的 `expected_build_id` 硬传空、`verify_install_tree.py` 非空分支不存在且校验器在 145 项检查里零注册 ⇒ 换掉 `modules/*.dll` 只要文件存在即放行。
- **W3-R2-001**：`aio_upm.cpp:101-110` Windows 正常路径先 `std::remove(path)` 再 `rename`，二次失败再删 tmp ⇒ 旧新模型同灭而注释称「失败可恢复」为假；同仓 `io_adapter.cpp:124-127` 与 `hiss_stream_writer.cpp` 已有正确写法。

（次重要：V21-N-17 内存回压双哨兵恒假、W3-R2-002 取消通道整体断线、W6-N-05 导出面合同无机器消费者。）
