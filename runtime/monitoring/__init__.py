"""AstroCS LOG-002 资源监控伴随器（runtime/monitoring 域，owner SA-LOG-08）。

重任务（cpu_heavy / io / 长运行）自动创建 monitor；同一 run ID 每秒采集：
进程/系统 CPU、active/granted workers、RSS/private/commit、read/write bytes、
queue/lock/io wait、provider/module。原始 CSV 不可手工合成（header 指纹 +
写后只读 + 单调性校验）。I/O 区间与初始化区间分开；Linux procfs 采集与
Windows PDH/ETW 适配代码路径分离（Windows 侧为显式未实现 stub + known_limits，
不误报 Windows 已支持）。provider/module/workers 从 RT-006 trace 真实来源取，
禁止 config 值冒充观测。

子模块：
  monitor.py           — 采样循环、自动建档、CSV 追加、时间序列合同
  linux_procfs.py      — Linux /proc 采集后端（唯一真实实现）
  windows_pdh_etw.py   — Windows PDH/ETW 显式未实现 stub（隔离、不误报）
  trace_feed.py        — RT-006 trace JSONL/事件的真实观测接线
  runner.py            — 重任务 run 守卫与自动 monitor 起动器

科学公式不涉及；本包不改任何科学/算法源码。
"""
