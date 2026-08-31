# R0-002 PLAN — 生成完整自有源码清单

## 需求
- 建立 `SOURCE_INDEX.csv`：path、kind、owner、target、sha256、size_bytes、generated、required_for_build。
- 解析所有 CMake target/source，逐项证明存在；生成 `TARGET_SOURCE_GRAPH.json/.dot`。
- 第三方单列（lock/version/license），不进自有清单。
- 故意删除一个 CMake source 的 fixture 必须让 checker 非零退出。

## 现状证据
- 根 CMakeLists.txt 声明 19 个生产 target（astrocs CLI + 18 库），tests/unit 39 个测试 executable。
- `tools/quality/gen_source_index_v61.py` 新生成器解析 add_library/add_executable 显式源。

## 影响文件
- tools/quality/gen_source_index_v61.py（新）
- tools/quality/check_source_index_v61.py（新）
- evidence/v6_1_rework/tasks/R0-002/{SOURCE_INDEX.csv,TARGET_SOURCE_GRAPH.json,TARGET_SOURCE_GRAPH.dot,third_party_sources.csv}
- evidence/v6_1_rework/tasks/R0-002/{PLAN.md,TASK_RESULT.json,logs/*}

## 科学影响
无（纯清单/构建图任务）。

## 风险
- CMake 解析用正则近似；不解析 ${var} 展开。已通过“存在性证明”逐文件校验，负例验证。

## 验收命令
1. `python3 tools/quality/gen_source_index_v61.py --repo . --out-dir evidence/v6_1_rework/tasks/R0-002` → SOURCE_INDEX_PASS
2. `python3 tools/quality/check_source_index_v61.py --repo . --index .../SOURCE_INDEX.csv` → SOURCE_INDEX_CHECK_PASS
3. 隐藏 lib/io/src/io_adapter.cpp 后重生成 → SOURCE_INDEX_FAIL missing target sources（负例）
