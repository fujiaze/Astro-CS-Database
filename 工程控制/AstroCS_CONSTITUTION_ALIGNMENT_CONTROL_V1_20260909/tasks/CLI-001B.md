# CLI-001B｜CLI去科学静态链接薄化

## 目标
主 CLI 移除对科学静态库的直链（改经 runtime registry/模块 DLL）；commands.cpp 去 include 科学私有头；config show-effective 删除 phases 推断。

## 依赖
`CLI-001`。BASE_SHA 取执行时最新 main（三 SHA 一致）。

## 写入白名单
- `cli/`
- `CMakeLists.txt`
- `tests/`

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
- CLI 全功能回归+ldd/链接图验证无科学符号+宪章 §8.1 薄入口断言+故障注入必败。
- 一个任务一个原子 commit 并 push main；fetch 后核对 HEAD/main/origin/main 三 SHA。

## 返回证据
scope/acceptance/provenance 三检查 + changed_files + 命令日志 + 任务特化产物。
