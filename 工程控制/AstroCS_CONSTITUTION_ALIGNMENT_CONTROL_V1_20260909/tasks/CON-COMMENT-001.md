# CON-COMMENT-001｜测试头 invariant 科学 ID 锚补全（CON-COMMENTS + CON-FULL-INTEGRATION 转绿）

## 目标
修复 `CON-COMMENTS`（三 profile 全红）与其级联 `CON-FULL-INTEGRATION`（`generate_contract_report` 的 `check_comments` 子检查 FAIL）。前台实测被点名文件与条目：

```
CON-COMMENTS 报 3 条 P1 COMMENT-MISSING-ID:
  lib/phase1_session/tests/p1sess/p1sess_tests_properties.cpp   "invariant without SCI/ALG ID nearby"
  lib/astro_image_io/tests/p1hips/p1hips_oracle.hpp            "invariant without SCI/ALG ID nearby"
  lib/calibration/tests/p1cal/p1cal_fixtures.hpp               "invariant without SCI/ALG ID nearby"
```

必须动作：
1. 对每处 invariant/oracle 补 **真实** `SCI-*` / `ALG-*` ID 锚——ID 必须逐条核到 `docs/science/`、`docs/algorithms/` 的**实际**文档 ID；
2. **不得臆造 ID**；若某处确无对应 SCI/ALG 定义，按宪章 §1.1 登记 finding（说明"该 invariant 无上位定义"），不得伪造；
3. `tools/quality/contracts/check_comments.py` 判定规则**不放宽**；
4. 提交前必须复跑 `CON-COMMENTS` 与 `CON-FULL-INTEGRATION` 两个检查器（后者不得再有级联红）。

## 依赖
`无硬依赖`。BASE_SHA 取执行时最新 main（三 SHA 一致）。

## 写入白名单
- `lib/phase1_session/tests/`
- `lib/astro_image_io/tests/`
- `lib/calibration/tests/`
- `tools/quality/contracts/`

## 裁决依据（前台 rev6，见 `07_FRONT_DESK_RULINGS_20260912.md`）
- 裁决 **R-09**：补科学 ID 锚，不弱化检查器。
- 宪章 §12.1/§12.2 文档—模块—源码符号—测试追踪；§13.1「不调用生产实现的独立 Oracle 或解析解」「科学不变量和性质测试」；§1.1 断链必须建 finding。

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
