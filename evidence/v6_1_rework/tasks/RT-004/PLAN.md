# RT-004 PLAN — schema 驱动 PipelineIR 解析与静态验证

## 需求 (04_TASK_SPECIFICATIONS.md RT-004)
使用真正 JSON parser+schema，不写脆弱字符串解析。IR 固定 schema/version、nodes、ports、artifacts、
resources、config reference、outputs。validate 接受完整 ModuleRegistry，必须触发 UNKNOWN_MODULE、
MISSING_PORT、DATA_MISMATCH、UNIT_MISMATCH、COORDINATE_MISMATCH、DUPLICATE_PRODUCER、CYCLE、
SERIAL_HEAVY、UNPRODUCED_OUTPUT。每个错误有 fixture。

## 现状证据
- 旧 pipeline.cpp 是手写脆弱字符串解析（top_key_value/extract_objects），违反"不写脆弱字符串解析"。
- validate 只接收 module_id 列表，无端口/schema/unit/coord 检查。
- third_party/nlohmann/json.hpp 已 vendored；orchestrator 已用 nlohmann。

## 修改
1. include/astrocs/core/pipeline.h：validate 签名改 ModuleRegistry；PipelineNode 加 config_json/
   config_ref_schema/config_ref_sha256；IrError 追加 COORDINATE_MISMATCH/UNPRODUCED_OUTPUT（数值追加不改 ABI）。
2. lib/core/src/pipeline.cpp：全部改用 nlohmann::json 解析；schema 字段校验（v1/v2）；必填检查；
   artifact ref 校验；resources.class 白名单；cpu_heavy→parallel 强制；config inline/ref 解析。
   validate 实现 9 类错误检测（未知模块/端口/DATA/unit/coord/重复 producer/环/serial_heavy/unconsumed/unproduced output）。
3. tests/unit/core_pipeline_test.cpp：13 测试（每错误种类一 fixture + 控制包 fixtures）。
4. CMakeLists.txt：astrocs_core include third_party。

## 科学影响
无（IR 解析与静态校验层）。

## 风险
- nlohmann 头文件较大；astrocs_core 编译时间略增（可接受）。
- 数值追加 IrError 保持既有枚举值不变。

## 验收命令
1. `cmake --build run/temp/build_v61 --target core_pipeline_test` → build=0
2. `ASTROCS_REPO=$(pwd) ./run/temp/build_v61/tests/unit/core_pipeline_test` → CORE-004 TESTS PASS
3. `ASTROCS_REPO=$(pwd) ctest -R "core_|rt0"` → 11/11 PASS
4. `cmake --build run/temp/build_v61 --target astrocs` → CLI 链接成功
