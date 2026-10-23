# ACSD v2 协作合同

先读docs/engineering/rebuild/README.md与SYSTEM.md，再读本次所属process文档和公共标准。SYSTEM唯一维护固定基建骨架；processes/<process_id>.md唯一维护科学步骤的输入输出、算法、并行/资源/探针与反例。科学公式归五份foundation，公共字段/签名唯一归include/acsd/contracts.hpp；API/OBJECTS/EXECUTION/VERIFICATION分别拥有共享协议/对象存储/执行/验收标准。

内部开发允许破坏式重构，只维护一套当前API/产品/配置；无v1或早期v2兼容、双reader、转换/产品迁移义务。Git历史及原始证据保留。v1只对照文档完整度，不套用其内部源码、Runtime或测试。一个科学算法一份实现，所有入口按统一typed process注册标准接入，模块内helper复用同一实现。GPU只设计扩展，不在本轮实现。

负责人本轮只要求文档重整；已有实现按真实证据报告，新ProcessHost/registry等是待实施目标。不把目标文档当已可运行接口。骨架改动集中在SYSTEM，共用标准各有唯一正本，算法步骤不得重新集中塞回骨架文档。

底层用统一VirtualComputePool抽象CPU/内存/I/O，唯一资源账本和CPU池；Task/有界Pipe支持异步I/O与背压，科学process优先组合可复用typed模块函数。公共计算积木标准见standards/COMPUTE_MODULES.md。新增模块按完整模板包登记，不向框架加算法switch；GPU只保留同一抽象的未来扩展。

只在rebuild/acsd-v2或用户明确的独立重构分支工作；不改main、不强推、不创建PR。保留他人改动；基于最新远端历史追加提交并直接推送本分支。

未知误差不填零，数值包含不等于物理误差已知。科学矛盾给反例并修订科学正本。无实跑不声称算法通过。独立审核者不参与设计编写，修订后再独立复核。

改文档运行tools/check_design.py；新process登记还须检查ID/operation唯一性、覆盖/类型映射与文档引用。改声明才做C++20检查，按VERIFICATION提供必要独立负例。sources/如存在为只读同步资料，不改名移动删除。许可证不发明，historical-notices只追溯。



直接开发时必须读standards/FOUNDATION_API.md（目标接口已定稿）、SCHEDULER.md（具体首期CPU算法）、DEVELOPMENT.md（M0—M6交付门槛）及CALIBRATION_WALKTHROUGH.md。当前实际头未同步不意味着可以自行发明另一套接口；实施后公共头唯一拥有可编译声明，规范摘录由检查保持一致。类型/codec/计划重规划/region/cleanup都走同一合同。
