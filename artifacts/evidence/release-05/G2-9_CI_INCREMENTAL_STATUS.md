# G2-9：CI 增量/并行化未完成项处置台账（GATE-501）

生成：RELEASE-05 / GATE-501（门禁合理性审计与修复）。
权威：docs/ci/CI_SPEC.md §2（增量/并行）、§2.6（档位与重步骤）；ENGINEERING_SPEC.md §10。
本台账逐项登记 6 个未完成项的**处置与证据**；域外项只登记不改（派单给对应文件域）。

| # | 项 | 状态 | 证据（命令与实测） | 处置 |
|---|---|---|---|---|
| 1 | 并行 runner 等价性验证 | **已完成（本任务）** | ① 子集实测：14 个检查 / 33 个 step 在 `--serial@@ 与 `--jobs 8@@ 下逐 step `(id, verdict)` **完全一致**（38.8s → 21.0s，1.85×），见 `run/RELEASE-05/evidence/parallel_equiv_{serial,parallel}.json`；② 全档实测：`--all --profile fast` 108 个 step 中 107 个一致（131.6s → 60.5s，2.17×），唯一差异 `CTEST-REGISTRATION` 已定位为**并发写者**（另一任务在我两次运行之间向 `eng/tests/unit/CMakeLists.txt` 加入未注册目标 `core_block_frame`），复跑两条道各 2 轮均稳定 FAIL —— 非并行性差异；③ fixture 级用例 `S6_serial_parallel_equivalence`（`run_checks.py --self-test`，17 例全绿） | 判据：并行化只改墙钟，不改逐项判定与结果顺序；结果不一致即自测判红。**并行等价性结论以"同一工作区快照"为前提**——并发写者会改变内容面判定（本次实测），故等价性复核须在冻结快照上做 |
| 2 | g++ 预编译 | **未完成（域外）** | 未实施 | 域外：`CMakeLists.txt` / `eng/cmake/**`（构建配置，非 GATE-501 文件域）→ 派单 BLD/PERF 线 |
| 3 | SECRET-HYGIENE 并发读 | **未完成（域外）** | 实测单次墙钟 **21.486 s**（fast 档 `CHK-SECRET-HYGIENE` step `duration_seconds`，tracked 全域单线程扫描；占并行档总墙钟 60.5 s 的 35%） | 域外：`eng/tools/quality/check_secret_hygiene.py` → 派单 eng/tools 线；本任务只提供耗时基线 |
| 4 | RESOURCE-GATE 合并窗口 | **未完成（域外）** | `eng/tools/quality/check_resource_gate_real.py` 以 busy/serial 两个**独立**监控窗口各跑一次 | 域外：`eng/tools/quality/**` → 派单；合并窗口须保持"正例绿/负例红"双向判别力（不得合并成一个窗口后无法判红） |
| 5 | 构建图反查 | **已在位并实测** | `printf 'lib/infrastructure/scheduler/src/module_adapters.cpp' > /tmp/inj.txt`；`python3 eng/ci/run_checks.py --changed --changed-paths-from /tmp/inj.txt --explain --plan-only` → `build_graph: used=True affected_tests=15` | 无需改动；本任务补实测证据（此前无验证记录） |
| 6 | 指纹缓存 | **本任务补齐（机制→可用）** | 见下节 | 机制此前**零声明**且字段未进 schema/校验器（声明即被 R2 判 unexpected fields）；本任务三处补齐 |

## 6 指纹缓存：本任务补齐明细

1. **字段进 schema**：`eng/ci/checks.schema.json` 顶层与 step 两级新增 `fingerprint`
   （`archive` 必填；`commit/config/data/workers/extra` 可选）；
2. **字段进校验器**：`eng/ci/validate_registry.py` 的 `STEP_OPT_FIELDS` 与顶层
   extra 白名单加入 `fingerprint` —— 修前 `--strict` 直接判 R2 违规；
3. **重步骤声明**（`CI_SPEC.md` §2.6 "重步骤带输入指纹缓存"）：`DEEP-SAN-ASAN` /
   `DEEP-SAN-TSAN` / `DEEP-COV-CPP` / `DEEP-COV-PY` / `CHK-REALDATA-E2E`
   五个 prerelease 重步骤声明 `fingerprint`（commit + `CMakeLists.txt`/
   `CMakePresets.json`/`eng/cmake` 配置摘要 + 数据清单 + worker 数，
   归档落 `run/ci/fingerprints/<step>`）；
4. **fail-closed 补强**：`run_checks.py` 指纹命中分支此前直接 `PASS(reused_fingerprint)`，
   不校验归档证据是否存在 —— 归档被清理后会"以复用为名"跳过判定。现改为：命中后仍执行
   `evidence_verdict`（登记 outputs / 监控证据齐备）；归档证据缺失则**照常执行**并记
   `fingerprint_error`。判据来源 `ENGINEERING_SPEC.md` §10（fail-closed）。

## 复核命令

```bash
python3 eng/ci/run_checks.py --self-test                      # 17 例（含 S6 并行等价）
python3 eng/ci/validate_registry.py --registry eng/ci/checks.json --strict
python3 eng/ci/run_checks.py --all --profile fast --serial --json-out run/RELEASE-05/evidence/fast_serial.json
python3 eng/ci/run_checks.py --all --profile fast --jobs 8 --json-out run/RELEASE-05/evidence/fast_parallel.json
```
