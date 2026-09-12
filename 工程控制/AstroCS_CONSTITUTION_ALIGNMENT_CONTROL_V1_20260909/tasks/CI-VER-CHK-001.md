# CI-VER-CHK-001｜版本一致性检查器排除标准条款号（UT-VERSION 转绿）

## 目标
修复 `UT-VERSION`（fast/linux-main/windows-main 全红，非豁免门）。根因（前台实测）：`tools/check_version_consistency.py` 的"未知版本字面量"正则将 `docs/standards/STANDARDS_REGISTRY.md` 中 **FITS WCS Paper I/II 与 IVOA HiPS 的条款号**（`§2.1.1`、`§4.2.1`、`§4.4.1`、`§6.3.1`）误判为版本号，19 条 findings **全部**落在该文件。

必须动作：
1. 修 `tools/check_version_consistency.py`，令版本字面量识别**排除标准条款号形态**（`§` 前缀、以及 `Paper`/`HiPS`/`§` 上下文中的 `a.b.c`）；
2. **同提交**在 `tests/version/` 增加**先红后绿 + 正向/负向**用例：
   - 正例：真实产品版本漂移（如 `VERSION` 与 CMake/CLI 不一致）**必须**仍判 FAIL；
   - 负例：`§2.1.1` 形态的标准条款号**必须**判 PASS；
   - 回归：现有 16 用例全绿。
3. **严禁**为过检查而改写 `docs/standards/STANDARDS_REGISTRY.md` 的标准引用（标准条款号是可追溯锚，见裁决 R-08）。

## 依赖
`无硬依赖`。BASE_SHA 取执行时最新 main（三 SHA 一致）。

## 写入白名单
- `tools/check_version_consistency.py`
- `tests/version/`

## 裁决依据（前台 rev6，见 `07_FRONT_DESK_RULINGS_20260912.md`）
- 裁决 **R-08**：修检查器口径，不改标准条款号；检查器**只准更精确、不准更宽松**——必须带"真实版本漂移仍 FAIL"的正向守卫用例。
- 宪章 §19 基础科学与格式参考；§7.3「以标准为基础，而不是根据现有代码反推科学定义」；§16.1 版本唯一输入为根 `VERSION`。

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
