# ARCH-002 执行日志 (2026-08-28, vm-bj)

## 输入
03_TASK_DETAILS ARCH-002 行(单一 CLI 组件图/Phase 数据流/生命周期/错误取消恢复/旧 exe 迁移; 验收=与 01/04 一致+不存在第二用户入口); 旧 ARCHITECTURE.md(53 行, 双入口架构); ARCH-001 清单。

## 动作
1. 重写 docs/architecture/ARCHITECTURE.md(V5 冻结版, 8 节): 唯一生产 exe=astrocs; 旧入口迁移冻结(orchestrator.exe→CLI 内嵌 pipeline driver 无进程边界/astrocs-stage2→CLI 命令树/healpix_browser_qt→tool 分类不发布); 组件图(Phase1/2/3 in-process+cpu backend ARCH-003); Phase 数据流; 配置(科学 config 与 cpu profile 分离)/manifest/artifact 生命周期(原子落盘/verify/stale); 错误/取消/恢复(取消粒度引用 ALG 5c 冻结); 7 不变量(机器可验)。
2. MODULE_MAP.md 同步: phase2 产物去 astrocs-stage2.exe(→phase2.a); orchestrator 标迁移无独立 exe; acr 标 V5 不接入 dormant; browser 标 tool 非发布。
3. 机器门 tests/arch/test_single_cli.py 5 用例: 唯一入口声明+旧双入口表述清除/MODULE_MAP 无 orchestrator.exe/迁移节显式列 3 旧目标/清单 production exe=0/三 Phase in-process。

## 验证
- 全量回归 unittest **31/26+5=31/31 OK**(新增 5)。
- 与 01 一致性: 单一 astrocs/无第二入口/ACR 不接入/Phase in-process 均为 V5 控制包冻结语义。

## 产物
docs/architecture/ARCHITECTURE.md(V5 重写); docs/architecture/MODULE_MAP.md(同步); tests/arch/test_single_cli.py; 本日志。

## PASS 判定
单一 CLI 组件图/数据流/生命周期/错误取消恢复齐备; 旧 exe 迁移显式; 机器门保证不存在第二用户入口(文档+清单双侧)。ARCH-002 = PASS。
