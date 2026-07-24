# P04 测试基础设施

## 目标

把现有零散测试接入统一入口，并建立合成/真实数据、报告和 CI。

## 必做

- 根级 test runner；
- pytest 配置和 C++ test 汇总；
- 标签与超时；
- DS-SYN 生成器；
- DS-MINI；
- 证据自动归档；
- 快速 CI；
- 夜间回归；
- flaky/skip 政策。

## Skip 规则

缺可选大数据可以 skip，但缺核心源码、契约测试或 DS-MINI 必须 fail。
