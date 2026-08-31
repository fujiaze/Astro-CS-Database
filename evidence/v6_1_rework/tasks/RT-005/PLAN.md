# RT-005 PLAN — 可执行 ModuleRegistry（真实 Phase1/2/3 工厂）

## 需求 (04_TASK_SPECIFICATIONS.md RT-005)
Descriptor 必须含 module ID/version/ABI、factory、plan、execute/inspect、ports、config schema、
execution class、parallel axes、memory model、SCI/ALG/DATA/API/TEST。注册时校验完整合同、
重复 ID/端口、heavy+serial、ACR production。registry 导出用 JSON library 正确转义。
至少一个真实 Phase1、一个 Phase2、一个 Phase3 模块通过 factory 执行，不接受只注册 metadata。

## 现状证据
- 旧 ModuleRegistry 只有 metadata（find/export_index_json 手写转义）；无工厂、无执行。
- 真实 Phase1/2/3 session（C ABI create/validate/run/inspect/destroy）已存在并链接进 CLI。

## 修改
1. include/astrocs/core/module.h：ModulePlan/IModule 移入（RunContext 前向声明）；
   ModuleRegistry 加 register_factory/create/has_factory。
2. lib/core/src/module.cpp：register_module 完整合同校验（重复端口/heavy+serial/ACR 拒）；
   register_factory 立即实例化探测（拒绝假模块）；create；export_index_json 用 nlohmann 正确转义。
3. include/astrocs/core/module_adapters.h + lib/core/src/module_adapters.cpp（新）：
   SessionModule 适配器把真实 p1/p2/p3 session 包成 IModule；register_phase_modules 注册 3 个真实模块。
4. CMakeLists.txt：新 astrocs_module_adapters 库（core + 3 session + cpu）。
5. tests/unit/rt005_registry_test.cpp（新）：5 组测试。

## 科学影响
无（模块注册与工厂层；科学计算仍在 session 内，未改动公式）。

## 风险
- module.h 结构重排（ModulePlan/IModule 移入）→ 已全量重编译验证 12 测试 + CLI。
- 适配器 execute 真实调用 session run（需合法 config；测试只走 create/validate/inspect/plan 路径，
  不触发需要真实数据的 run）。

## 验收命令
1. `cmake --build run/temp/build_v61 --target rt005_registry_test` → build=0
2. `./run/temp/build_v61/tests/unit/rt005_registry_test` → RT-005_PASS
3. `ASTROCS_REPO=$(pwd) ctest -R "core_|rt0"` → 12/12 PASS
4. `cmake --build run/temp/build_v61 --target astrocs` → CLI 链接成功
