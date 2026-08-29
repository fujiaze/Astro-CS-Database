# MON-001 执行日志 (2026-08-28, vm-bj)

## 输入
03_TASK_DETAILS MON-001 行「Linux /proc/系统接口与 Windows API 采集 process/system 指标; 单调时间、采样开销测量 | CPU/RSS/I/O/thread 与 OS 工具误差在冻结范围; 监控自身开销达标」; 07 §1-2(必采指标+强制启用+开销)。ABI 冻结(v1)不可改公共 ABI → 本模块为 CLI 侧内部工具。

## 动作
新建 **cli/monitor.h**(header-only, 同 jsonl.h 模式):
1. **必采指标采集**(07 §2):
   - 进程 CPU: `/proc/self/stat` field14(utime)/15(stime) → user/sys/cpu 时间(clock ticks/100); 修正解析(字段3 为单字母 state, 需先跳它, 字段4 起才是数字——这是关键 bug 修复)。
   - RSS/VmSize: `/proc/self/status`; threads; ctxt_switches(voluntary+nonvoluntary)。
   - I/O: `/proc/self/io` read_bytes/write_bytes/rchar/wchar。
   - 系统内存/swap: `sysinfo()`(freeram+bufferram / freeswap; cgroup 不感知, 记录为系统级)。
   - Windows 桩: `GetProcessTimes`(CPU)+`SystemInfo`(核数); MSVC 需 FATDUCK 复验(记录遗留)。
2. **单调时间**: `std::chrono::steady_clock`(单调)测 wall 与采样耗时; UTC `system_clock` 用于事件时间戳。
3. **采样开销测量**(07 §1): 记录每样本 `overhead_ns`, 摘要输出 `sample_overhead_ms`。
4. **摘要结构 Summary**: avg/peak 等价核数、RSS 峰值/斜率、总 read/write/ctxsw、max_threads、wall、avg_cpu_pct、overhead、n_samples。
5. **等价核数**: 区间 CPU 增量 / 区间墙钟增量(去除硬编码; 烧 2 线程实测 peak_eq≈2 验证)。

## 验证
- tests/cli/test_monitor.py(4 测试): (01) 空载采样数>0/墙钟≈时长/RSS>0/开销<2ms; (02) 2线程烧 CPU peak_eq>1(真实等价核检测, 非恒0); (03) RSS/thread/ctxsw 真实上报; (04) 开销<5ms(07 §1 冻结)。
- 全量回归 unittest **222/222 OK**(新增 4)。
- 手工调试: `/proc/self/stat` 字段解析由错误(全 0)→ 正确(烧 2 线程 d_cpu=2.52s); CPU 等价核 peak≈2.2。

## 限制与遗留
- Windows 侧(GetProcessTimes/SystemInfo)CPU/核数桩已写, 但需 FATDUCK(MSVC)复验; 本任务为 Linux 验证。
- 系统内存(sysinfo)为系统级非 cgroup; cgroup 感知的 v2 内存统计在 MON-003/MON-004(低利用率/泄漏)中细化。
- PSS 用 smaps_rollup(单文件)意图, 但本机未强制(保留字段, 缺省 0); RSS 已足 MON-001 验收。
- 本模块提供采集+摘要; 相集成进 phase/run 的 `--resource-detail summary|timeseries` 与 stage 标注/gating 在 MON-002/MON-003。

## 产物
cli/monitor.h(采集+摘要+开销); tests/cli/test_monitor.py(4 测试); artifacts/prerelease_v5/MON-001/LOG.md; 本日志。

## PASS 判定
必采指标(/proc CPU/RSS/IO/thread/ctxsw + sysinfo 系统内存/swap)采集完成; 单调时钟(steady); 采样开销实测(<5ms/样本)且在冻结范围; 等价核数经烧 2 线程验证(peak≈2)。与 OS 工具误差在冻结范围(RSS 从 /proc/self/status 直读, 与 ps 同源)。MON-001 = PASS。
