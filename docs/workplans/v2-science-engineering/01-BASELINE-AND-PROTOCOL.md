# A0｜冻结实际状态、查清源码—文档差异、建立科研与工程双轨

## 范围与已知事实
基线仅用于说明调查起点：`rebuild/acsd-v2@27bd7925`（2026-10-25）；执行时读取实际 HEAD。仓库目前有五份 foundation、22 process/57 typed operation、Runtime/Host/registry/pipe/storage、CLI/GUI 源码、`validation/x01`。Issue [#112](https://github.com/fujiaze/Astro-CS-Database/issues/112) 仍 OPEN。已复现记录中的 Linux X01 三命令 8/8 Normalize、Mosaic 1、Export 1，盲解 RMS 0.211px；但 `source_recovered` 最大信号相对误差 0.103 > 0.05，科学比较 FAIL；不要宣称 V2 完成。`validation/run_summary.json` 仍记录旧 7pass/4pending，其中 X01 为 pending，与当前 X01 comparison FAIL 相冲突。`docs/audit/IMPLEMENTATION-STATUS.md` 的数值认证 P0/N-01..04 尚未修复。

## 核对程序（人读报告，不运行文档门禁）
1. 对每一运行入口（内存 API / contract / Normalize / Mosaic / Export / GUI）列调用到哪一个实现文件、其是否唯一、是否可能绕过 Host/账本、返回和结果状态。以实际符号引用和调用栈证明，不以目录存在推断。
2. 阅读 `include/acsd/model.hpp`、`runtime/*.hpp`、`modules/*.hpp` 和 `contracts.hpp`，核对各公共 type/operation 当前形态、真实 consumer、持久化编码和生命周期；用表明列“规范正确/源码正确/两者不符/科学不明/待独立复算”。
3. 按 22 Process 逐项建立 `Operation → formula/proof → source file/function → Request → persistent object → consumer` 行；审查是否有重复 helper/算法 switch/静态全局模型/半成品占位。没有观察或不能推理的一项必须写 UNKNOWN。
4. 识别已有模块的正确科学成果并保留，反向检查哪些所谓“实现完成”只是 registered/compiles/smoke；不要删除可复用数值算法仅为展示重写量。
5. 对所有现有科学/工程/审计正文标注“唯一正本/参考/冻结历史/被替代”，逐个消除冲突段落，**直接修原正文**，而非在文末无限追加“最新补充”。历史审查记录不回写。
6. 建立风险清单：`interval` 四类包含性 P0、`linalg` 严格 rank/residual、typed Dyadic Storage、persistent BlockStore 清理死锁、M5 process 逐项缺证据、R01/P01/F01/S01 旧 pending、盲解性能长尾、X01 B组测光差、真实数据与 Windows 缺口。每项指定文件和实际反例，而非笼统判全部坏。
7. 区分**现有文件中已经出现的证据**与**本轮确实重新跑出的证据**；不在统一报告中混不同 SHA/算法规范/测试样本的计数。保留原始失败时序。

## 文档与产出
- 一份当前 SHA 的“科学公式/代码实现/实际证据/未证条件”四栏清单，不要求把这份清单实现为程序。
- 一份旧 11 case、Runtime 16 case、X01 和相关过程用例的证据差异解释，尤其说明为什么 `published` 不能推导 `science_correct`。
- 每个实际 bug 附最小输入、触发路径、预期数学值、真实输出和影响范围；没有证据只列疑点。
- 与 [10 工程矩阵](engineering/10-ARCHITECTURE-PROCESS-MATRIX.md)互相引用，但不要再做第二套“当前状态”权威文档；实际实施状态仍在 `docs/audit/IMPLEMENTATION-STATUS.md` 维护。

## 完成标准
由未参与审计对象实现的审核者独立选择 5 条跨模块数据边，从最终使用者追到原生源和随机根；能独立指出实际未决能力、原始证据和每项责任人。没有后续文档检查脚本或 CI 签核。
