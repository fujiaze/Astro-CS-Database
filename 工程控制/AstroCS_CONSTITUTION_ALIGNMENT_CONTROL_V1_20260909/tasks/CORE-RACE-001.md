# CORE-RACE-001｜p1001 链并发撕裂读修复（CTEST-P1001-REAL-NODES 转绿）

## 目标
修复 **P1 生产数据完整性缺陷**：p1001 节点链在并行 worker 下对共享产物路径做**非原子原地覆写**，消费者可观察到半写文件。三方独立互证（CI-REG-002 发现 + 前台 200 次复跑 `pass=176 fail=24`（**12%**）+ ARCH-AUDIT-P1 影子树命中）。

**根因（前台已定位，直接读 `lib/core/src/module_adapters.cpp`）**：
- `p1_op_calibration` 写 `out_dir+"/calibrated_"+base`；
- `p1_op_cosmetic` **读取同一路径后原地覆写同一路径**（`aio_write_fits(im.p, path.c_str())`）；
- `p1_op_drizzle` 经 `p1_calibrated_path` **读取同一路径**；
- chain IR 中 `cos` 与 `drz` 同为 `cal` 下游且节点声明 `resources.parallel=true`，`create_runtime(2)` 下并发执行；
- `aio_write_fits` 本身非原子（AIO-002 只让 HiPS publish 原子）⇒ 撕裂读。

**必须动作**：
1. 消除"同路径原地覆写 + 并发读"：cosmetic 必须写**独立中间产物路径**（或在链 IR 中显式声明写后读的 happens-before 顺序），使任一消费者读到的永远是完整文件；
2. 修**生产代码**，不是改测试超时/加 retry/加重试掩盖（宪章 §14.4 禁止重复重试、禁止为掩盖缺陷堆叠防御）；
3. 先红后绿：构造确定性复现（≥200 次连跑 0 失败；并保留修复前 RED 证据），负向注入必败；
4. `CTEST-P1001-REAL-NODES` 与 `CTEST-LINUX-FULL` 必须本地同构转绿；
5. 同提交注册/更新 `ci/checks.json` 检查项。
6. **附上最小补丁文本**并在任务报告里说明生产语义变化（若无变化须显式声明 `scientific_change=none`）。

## 依赖
`无硬依赖`。BASE_SHA 取执行时最新 main（三 SHA 一致）。

## 写入白名单
- `lib/core/src/module_adapters.cpp`
- `tests/unit/`

## 裁决依据（前台 rev6，见 `07_FRONT_DESK_RULINGS_20260912.md`）
- 裁决 **R-03**：该文件为真实修复面，写域由前台补正（原谱系零写域覆盖 = 控制包规格缺陷，使宪章 §17.10「P0/P1=0」结构性不可达）。
- 裁决 **R-05**：该检查项 non-waivable，**禁止**加入 `ci/known_failures.json`。
- 宪章 §14.4 fail-fast「发现合同违例立即返回明确状态码并停止当前产品提交」「禁止重复重试」；§4.1 provenance/validity；§17.10 P0/P1 为零。
- 关联 finding：**FD-R1-012**（与 FD-R1-013 覆盖缺口同源）。

## 非目标与禁令
- 不顺手修复域外问题；发现后登记 finding（05 号登记册续写）。
- 不修改/放宽科学公式、默认容差、冻结门或负责人裁决。
- 不 reset/stash/clean/rebase；不创建 branch/worktree/clone；SubAgent 不 git add/commit/push。
- **严禁以"放宽检查规则 / 加入 known-failures 基线 / 改 waivable / 跳过测试"的方式让红灯消失**（裁决 R-05、R-13）。
- 新增/修改测试必须同提交注册 CI 检查项（宪章 §17 + 负责人 CI 指令）；运行产物禁止落根目录。

## 必须动作
1. 读取冻结宪章相关条款、`docs/standards/STANDARDS_REGISTRY.md`、本任务域 SCI/ALG/DATA/ARCH 文档。
2. 测试设计先行：先红后绿、负向注入必败、1/N worker parity（适用时）、确定性 bitwise（适用时）。
3. 执行并保存命令证据（timeout/cwd/argv/起止/rc/stdout/stderr/SHA）；heavy 同 run ID 资源监控。
4. 同步订正 CI：新测试目标→`ci/checks.json` 显式检查项；漂移锚→复测工具。
5. 修复后必须**本地复现 CI 同构条件**验证（不得只在本机默认环境跑绿即报 PASS）。

## 验收
- write_scope 零越界；预存 dirty 零覆盖；所有新测试故障注入必败。
- 本任务对应的 CI 检查项在本地同构条件下 **exit 0**，并给出命令与日志路径。
- 一个任务一个原子 commit 并 push main；fetch 后核对 HEAD/main/origin/main 三 SHA。

## 返回证据
scope/acceptance/provenance 三检查 + changed_files + 命令日志 + 任务特化产物。
