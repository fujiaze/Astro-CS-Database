# infrastructure

本目录存放基建模块的插件工作细节文档：I/O、命令入口、运行时、基准、可观测性、星表客户端与浏览器七个模块。

## 职责边界

- 放：基建模块各自的职责、输入输出与边界说明。
- 不放：科学算法模块（在同级 algorithms_phase1/2/3 子目录）；I/O 合同条款正文（在 docs/contracts/ 与 docs/interfaces/）；模块代码说明（在 docs/modules/）。

## 内容

- `17_aio.md` —— FITS/HiPS/manifest 的唯一 I/O 边界：读、写、校验、原子提交、缓存与形态解析。
- `18_cli.md` —— 三个平级命令的入口：命令解析、配置校验、检查页面、模板生成与退出码。
- `19_runtime.md` —— typed DAG 执行、统一线程预算、locality-aware 编排、流式内存管理与取消。
- `20_benchmark.md` —— 按 kernel 测量数值误差、吞吐、线程扩展与内存带宽，生成机器绑定的 cpu_profile。
- `21_observability.md` —— 结构化日志、事件流、运行图、资源监控与诊断。
- `22_gaia_xpsd_client.md` —— Gaia/XPSD 本地星表解析：多级缓存、坐标/数据集版本语义；**离线、零网络、不联网**（本地缓存，最高设计 §3.3）。
- `23_hips_browser.md` —— HiPS 可视化浏览组件的渲染、缩放、导航与工程诊断。

## 上游

上游：docs/ASTROCS_DESIGN.md §7.1（命令树）、§7.2（配置、事件与退出码）、§8.1（顶层结构）、§9（CPU 后端与资源）、§10（I/O 与原子产品）。
