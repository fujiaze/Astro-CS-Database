# ACSD v2 协作合同

先读 docs/engineering/rebuild/README.md、五份foundation和本主题正本，再读 include/acsd/contracts.hpp。科学公式归foundation，字段与函数签名唯一归声明合同；SYSTEM/OBJECTS/API/EXECUTION/VERIFICATION分别拥有算法架构/存储语义/协议/资源精度/独立验证。

当前只交付设计、结构和声明配置。负责人正式接受新设计后才开始全面算法编码。不复制旧内部源码或测试，不以历史结果验证新实现。一个算法一份，所有入口调用同API。

只在rebuild/acsd-v2或用户明确的独立重构分支工作；不改main、不强推、不创建PR。保留他人改动。提交并直接推送本分支。

未知误差不填零，数值包含不等于真实物理误差已知。发现科学矛盾给反例并修订科学正本。无实跑不声称算法通过。独立审核者不参与设计编写，修订后再独立复核。

改规范运行tools/check_design.py；改声明用C++20语法检查。按VERIFICATION提供最少具有独立证伪力的测试。sources/如存在为只读同步资料，不改名移动删除。许可证不发明，historical-notices仅追溯旧文件。
