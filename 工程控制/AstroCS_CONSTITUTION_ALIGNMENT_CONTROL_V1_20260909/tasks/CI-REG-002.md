# CI-REG-002｜新增测试全注册与关键检查不可豁免

## 目标
1. 本轮新增全部测试目标（p1001/p2001/p2002/p3002 real_nodes、p2002_unc_rej_prov、p3002_uncertainty、p3_projection_units/fault、p1wcs_apbp、p1wcs_astropy_cross、aio_abi_* 等）逐个注册为 ci/checks.json 显式检查项（per-domain id，绑定 profile）；2. linux-main profile 必须包含全量 ctest（新增 profile 或扩展现有）；3. BUILD-GCC-RELEASE/DEEP-SAN-ASAN/DEEP-COV-CPP 改 waivable=false；4. validator（tools/quality 或 ci/tests）加负向检查：发现未注册的新 add_test 目标即 FAIL；5. p1wcs_astropy_cross 的 astropy/numpy 登记进 prerequisite_tools（STD-F10）。

## 依赖
`无硬依赖`。BASE_SHA 取执行时最新 main（三 SHA 一致）。

## 写入白名单
- `ci/`
- `tools/quality/`
- `tests/unit/CMakeLists.txt`

## 非目标与禁令
- 不顺手修复域外问题；发现后登记 finding（05 号登记册续写）。
- 不修改/放宽科学公式、默认容差、冻结门或负责人裁决。
- 不 reset/stash/clean/rebase；不创建 branch/worktree/clone；SubAgent 不 git add/commit/push。
- 新增/修改测试必须同提交注册 CI 检查项（宪章 §17 + 负责人 CI 指令）；运行产物禁止落根目录。

## 必须动作
1. 读取冻结宪章相关条款、STD 注册表（如已建立）、本任务域 SCI/ALG/DATA/ARCH 文档。
2. 测试设计先行：先红后绿、负向注入必败、1/N worker parity（适用时）、确定性 bitwise（适用时）。
3. 执行并保存命令证据（timeout/cwd/argv/起止/rc/stdout/stderr/SHA）；heavy 同 run ID 资源监控。
4. 同步订正 CI：新测试目标→checks.json 显式检查项；漂移锚→复测工具。

## 验收
- write_scope 零越界；预存 dirty 零覆盖；所有新测试故障注入必败。
- 全部新目标在 checks.json 有对应项；负向检查用例（故意加一个未注册 add_test）必败；python3 ci/run.py --profile linux-main --plan-only 列出新检查项。
- 一个任务一个原子 commit 并 push main；fetch 后核对 HEAD/main/origin/main 三 SHA。

## 返回证据
scope/acceptance/provenance 三检查 + changed_files + 命令日志 + 任务特化产物。
