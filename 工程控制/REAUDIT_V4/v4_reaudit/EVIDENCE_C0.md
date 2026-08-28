# C0 证据（V4）

## C0-001 控制包自校验

- validate_control.py → CONTROL_PASS files=21 sha256=bf5b51b3…d631（98 行账本+C0–C9 链）。
- 账本行尾归一 LF（修复 csv.writer CRLF 行尾）。

## C0-002 起点 SHA 冻结（2026-08-28T12:37Z）

- origin/main = `020cdc994dc42035c6eba7efd68c07e19d175415`（git fetch 后冻结，V4 唯一候选起点；该 SHA 即 C0-001 控制包提交）。
- 后续任务不得重设起点；历史代码仅 git archive 只读导出。

## C0-003 工具链与主机盘点

### Linux（vm-bj，开发/构建平台）
- gcc (Debian 14.2.0-19) 14.2.0；nproc=2；内存 3GB。
- 推导：并行度上限受 2 核约束；TSan/ASan 全门禁在 2 核上单线程运行（V3 实测 7–17 分钟量级）。

### Windows（Fatduck，验证节点）
- MinGW64 g++ 16.1.0 / Make 4.4.1 / CMake 4.3.2 / Ninja 1.13.2 / Python 3.12.2；16 线程；
  在线窗 07:00–23:30 CST（见 FATDUCK_ACCESS.md）；接入 `ssh fujia@100.104.10.71`。
- V3 实测：stage2 avg_cores≈0.85（串行债务）、A32/B32 墙钟 1651s/1741s（V4 修复后对照基线）。

### Fatduck 实测补充（2026-08-28 在线窗内实测）
- NUMBER_OF_PROCESSORS=16；g++.exe (Rev4, MSYS2) 16.1.0（与盘点一致）。

## C0-004 V3 继承证据与债务登记

继承包：`run/reaudit_v3/AstroCS_REAUDIT_V3_REVIEWPACK_20260828T1126Z.zip`
（sha256 76a43bddb9e7958c8e719baee9d74d84bc92514a974892cbfba28889a4949089）。

| # | 债务（V3 证据） | V4 处置任务 |
|---|---|---|
| 1 | TSan 5634 条数据竞争（upm.cpp:532/577/625 build_impl OMP 段） | C1-007 登记 → C5-017 修复 → C6-013 干净验证 |
| 2 | 串行架构：stage2 avg_cores≈0.85；重计算单线程段 | C1-006 扫描 → C5-006/007/019 清零 → C8/C9 资源门禁 |
| 3 | mosaic 层缺口（仅 signal+support） | C2-002 覆盖语义 + C4-010 层合同 + C9-002 判定 |
| 4 | stage1 输入版本漂移（A32 rejected 差 0.57%） | C8-005/C9-001 固定当前候选单链路（不再跨版本拼接） |
| 5 | DLL 时代双入口/运行时加载（BLD-002/003） | C4-003 退役方案 → C5-001..004 进程内实现 |

## C0-005 账本初始化与策略核验

- 98 行全 NOT_STARTED→按任务推进；依赖闭合（validate_control.py 强校验）。
- 原子 commit+push 策略：每 Task 恰一 commit、立即 push（规则 4）；review capsule 自 C1 起每任务生成。
- 唯一终态：AWAITING_EXTERNAL_RELEASE_REVIEW（C9-005）。
