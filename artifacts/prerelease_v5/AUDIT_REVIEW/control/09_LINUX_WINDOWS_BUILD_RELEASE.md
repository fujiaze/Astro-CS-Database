# Linux 控制节点、Windows 重计算节点与发布

## 1. Linux vm-bj（低资源、常在线）

负责：身份/静态分析、文档与符号核对、CLI/ABI 构建、baseline/可用 ISA 编译、合成小测、sanitizer 小测、benchmark 快速模式、资源监控开发、控制 Windows 任务。

- 先检测实际 CPU/RAM/affinity，不把“2C2G”写进代码。
- 编译并行度按可用内存动态限制，避免 OOM；运行时 worker 与编译并行度分开。
- 小合成 compute 必须仍能用满有效 CPU；不能以机器小为由允许单线程。
- 长任务必须 timeout、日志流式/落盘、失败可恢复。
- Fatduck 离线时继续所有无依赖 Linux Task；空闲很久可跑中等合成任务，但不得拖慢控制工作。

Linux 发布候选必须 clean build、CLI contract tests、backend self-tests、synthetic、sanitizer、package manifest/hash 全过。

## 2. Fatduck Windows（间歇在线、正式重计算）

连接前只做只读在线探测。在线后严格顺序：

1. 从 GitHub 拉取相同 `main` SHA，确认 clean；
2. MSVC clean Release amd64 构建，任何 Error、ignored Error、失败测试均 FAIL；
3. `doctor` 与 backend 安全加载；
4. full benchmark，生成该机器 profile；
5. 全量 synthetic 与资源/内存门禁；
6. 从 Fatduck 本地 testdata 生成小真实数据 manifest，先跑最小代表集；
7. 只有以上全过才对当前候选运行银心 32R 一次；
8. 验证 32/32 contribution、HiPS 完整性、接缝指标、固定视图和资源曲线；
9. 生成 Windows 发布包、SBOM/许可证、hash、smoke install。

不得把 testdata、HiPS、编译产物复制进审核包；只登记路径、大小、hash 和小型摘要/截图。

## 3. 接缝回归专项

这是已知退化，必须有独立当前实现测试，不靠历史图：

- 合成重叠 tile：相同背景、已知常数/渐变/低阶面与嵌入星点；
- 测 overlap difference、boundary jump、低频残差、flux conservation；
- UPM/photometric surface 参数及单位可追溯；
- 32R 最终 HiPS 用固定坐标/FOV/锁定 STF，输出 candidate、support、difference/residual 视图；
- 门限在看到最终图前由 SCI/ALG 合同冻结。

视觉审核是额外门禁，不替代数值指标。

## 4. 双平台一致性

同一 git SHA、同一 schema/CLI 版本、同一 science contract。允许编译器导致的预冻结浮点差异，不允许命令语义、字段、退出码、默认科学参数分叉。

## 5. 发布物

- `AstroCS-Windows-amd64-<X.Y.Z-alpha.N>.zip`
- `AstroCS-Linux-amd64-<X.Y.Z-alpha.N>.tar.zst`（工具不可用时 `.tar.gz`，需记录）
- 各自 MANIFEST/SHA256/SBOM/licenses；
- 源码 tag/commit 由最终审核通过后创建。本控制包阶段不得自行宣称发布。
