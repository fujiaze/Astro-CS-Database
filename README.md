# ACSD v2：固定基建与科学process

从[设计入口](docs/engineering/rebuild/README.md)开始：[一份基建骨架](docs/engineering/rebuild/SYSTEM.md)维护注册、生命周期、调度和服务；[各科学process](docs/engineering/rebuild/processes/README.md)独立维护算法与模块边界；公共API/对象/执行/验收标准统一复用。

基建以VirtualComputePool统一CPU/内存/I/O资源，提供Task、有界内存管道和异步I/O；算法通过[封装好的计算模块](docs/engineering/rebuild/standards/COMPUTE_MODULES.md)与强类型函数组合，实现逐模块维护和后续接入。

内部开发只维护一套当前格式，允许破坏式重构，不兼容v1或早期v2产品/接口，也不实现旧产品转换。Git历史及原始科学证据保留。GPU只设计KernelExecutor/ArrayBacking/Completion扩展，本期CPU，不实现GPU后端。

本分支已经从纯设计推进至 Runtime（M0—M4）和部分 typed 科学模块（M5）的实际实现。**当前唯一进度与缺口入口**：[实施与验收状态](docs/audit/IMPLEMENTATION-STATUS.md)。截至被核查提交 `6f08c4e`，Runtime 冻结证据为 **16 case 中 15 pass、multi01 fail**；旧科学汇总为 7 pass / 4 pending，不能据此宣称新 Normalize→Mosaic→Export 全链完成。新增/修改算法继续通过模块 process 文档和唯一 typed descriptor 接入，不向 Host 增加算法分支。

```text
python -m pip install -r validation/requirements.txt
cmake --preset design
cmake --build --preset design
ctest --preset design
python tools/check_design.py
```

上述是现有设计检查入口，实际科学构建/运行状态依CMakePresets与冻结验证记录。[本次更正与复核](docs/audit/process-framework.md)记录文档范围和验证边界。继续在rebuild/acsd-v2追加提交，完整保留已有历史。



直接开发从[任务书](docs/engineering/rebuild/standards/DEVELOPMENT.md)开始，依次读[定稿基建接口](docs/engineering/rebuild/standards/FOUNDATION_API.md)、[调度实施算法](docs/engineering/rebuild/standards/SCHEDULER.md)和[完整校准模块例](docs/engineering/rebuild/standards/CALIBRATION_WALKTHROUGH.md)。它们规定具体字段/函数/资源和错误规则，当前实现接入状态仍按实际证据。

## 当前执行与验证入口

- 基建：`validation/runtime/build.sh` + `validation/runtime/run_cases.py --no-build`（依赖已配置的 CPU OFF/DEV ON 构建；当前 multi01 预期仍是 FAIL，必须保持其证据）。
- 已接入的科学模块：`validation/process/README.md` 及各模块冻结 case。
- 完整科学链：`validation/run_summary.json` 只是旧身份的 11-case 汇总；正式验收以 [VERIFICATION](docs/engineering/rebuild/VERIFICATION.md) 与最新成功发布/重开证据为准。

本次文档订正未重新运行以上测试，不改变科学与 Runtime 代码。
