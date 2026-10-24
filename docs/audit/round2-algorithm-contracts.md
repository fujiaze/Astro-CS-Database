# 第二轮算法实施合同补全

日期2026-10-24。输入为负责人提供的《ACSD v2 第二轮深度审查》（原核查a17b199c2e17）；本轮先读取较新远端代码/文档[13740080](https://github.com/fujiaze/Astro-CS-Database/commit/13740080d44a587947652b7544ba7ce5d0af7921)再修订，保留既有框架和科学基础。本轮只改Markdown，未修生产数值源码、公共头、schema、构建或验证产物；GPU只设计扩展。

## 更正完成标准

上一轮“可以完整指导开发”的判断只覆盖了框架/接口层，不能用来认定22个算法均已无歧义复现。本轮明确区分架构完备、算法可独立实施、实现验收，新增[ALGORITHM_CONTRACTS](../engineering/rebuild/standards/ALGORITHM_CONTRACTS.md)，并在全部22篇process中逐项写入估计量、参数来源、执行步骤、资源/停止、状态及固定独立例。模板和DEVELOPMENT/VERIFICATION同步约束陌生开发者复现，不能只靠目录/登记数认证。

数学仍归五份foundation；没有重建第三套架构，也没有增加v1/早期v2兼容或GPU实现。公开字段/签名仍由model/runtime/modules头拥有，文档新增案例/标签不表示已经新增API或可运行测试命令。

## 重点补全及其余模块

- [sources.measure](../engineering/rebuild/processes/sources.measure.md)：窗口连通/噪声耦合、固定位置/PSF、原生背景密度经实际采样链、完整GLS/PSD、通量可估性、数值解包围和同数据influence。
- [response.integrate](../engineering/rebuild/processes/response.integrate.md)：唯一认证路径为解析多项式→Taylor8及显式余项→纯包含积分；GK仅作名义值/细化排序，冻结断点、photon单位、尾界和停止。
- [geometry.drizzle](../engineering/rebuild/processes/geometry.drizzle.md)：外存稳定归并CSR、归一前N支持判断、同一R作用所有载体、signed误差、共同ledger、局部细化失效闭包及R/F/E。
- 其余19篇分别补了校准/重组/WCS、Gaia解码/历元/索引、天测匹配与接受、PSF拟合/尾界、latent score/Hessian/Schur/KKT、action七项操作、covariance unknown/同根、信息量PSD/三角网/holes、sky控制与联合解、Selection状态/BLUE/QP/归约和Export支持/同叶合并。

这是目标合同补全，不把已有代码/某个正常样例升级成生产验收。当前能否在独立环境复现，仍须逐篇从公开规范/API写出伪代码/数学例，再冻结truth跑真实入口。

## 数值阻碍与状态不漂移

原审查所列四则求值顺序、sin/cos固定相位枚举、pow交叉端点和atan2切线问题，在受检interval源码仍可定位；linalg严格残差/能力判定亦需整改。已把它们纳入[NUMERICS](../engineering/rebuild/standards/NUMERICS.md)的具体实现规则、独立原语/复合反例及逐族认证门槛，并在[状态正本](IMPLEMENTATION-STATUS.md)保持BLOCKED_CERTIFICATION。**本轮没有修复这些实现，不宣称上游严格证书已经恢复。**

较新Runtime manifest确实记录16/16冻结PASS（tested source b592d88、multi01 42/42）；旧multi失败转历史，不再在README/SYSTEM/调度规则同时称当前FAIL。比较b592d88→13740080时，有PSF及fixture/harness等变化，未见Runtime主体源代码变化；本轮未重新构建运行，不能据此认证新HEAD所有断言。后续自动门须校验source/依赖、fixture/oracle、spec/断言、flags及case聚合，尚未在本轮实现。

Storage的Dyadic精确typed read、尺寸运算checked、跨进程full闭包恢复，控制节点动态metadata与并发账本快照、真实多段pipe/fork/join/barrier和逐模块probe生产等，都补成明确实施/反例要求；保持待实现/待验证，不用文字要求代替实跑。

## 独立复核的具体修订

两位未参与编写的只读审核者分别复核数值/拟合与其它process执行合同，并独立复算固定数学例。根据必要发现已修：

1. PSD信息量先变换θp/N及F泛函，不能对原h/Z直接投影。
2. 背景列先在原生footprint/PRF积分再经G/α，不能对非均匀bin固定Z=1；tail_absolute_flux按实际观测单位和max|F|换算。
3. 非线性无噪声约束使用完整KKT/Lagrangian Hessian及influence，不能C+漏约束。
4. sky的科学gauge分量与计算分块分开，有跨分量C/shared-root时保联合/全局耦合。
5. LOO负例改为能使错误“全超阈同时拒绝”实际失败的20幅度污染；log方差明确0/负/unknown/跨0分支。
6. 固定u64 source_id %5留出映射和共圆Delaunay的插入/对角线/输出排序，补正方形中心插值反例；修正process→foundation链接。

两位审核者在修订后分别定向复核，所报必要问题均已闭合，本次审查范围无未决必要P0/P1/P2。此结论只针对规范与固定数学例；源码中已知的严格原语阻碍依然未修，不认证生产数值/资源/性能实现。

## 本轮实际检查的边界

独立标准库有理数/公式计算复核20项数学检查，含联合通量/背景/协方差、同根消除、重组、Drizzle、BLUE/负权、跨分量GLS、Export同叶合并、PSD信息量、PSD profile梯度、解析积分、幂范围、binary64精确和与LOO反例判别性。它们校验文档中固定真值，不调用生产C++/求解helper，也不证明任意输入正确。

使用13740080的实际check_design.py、schema生成器、model.hpp和science-input schema，在独立只读参考overlay运行设计检查通过（302定义、78操作变体、六个根文件、示例/链接及4295条删除记录）。本地起点仍是旧设计快照，远端Git路径清单仅补充引用目标的存在性，未把占位引用文件作为源码编译。生产build/process-manifest.json缺失，工具明确跳过生产registry覆盖，不宣称其已通过。

另核22篇独立算法合同、57 typed operation绑定唯一、33科学probe所有权唯一及Markdown-only变更。结构检查不证明算法语义正确，数学小例不证明任意输入。生产构建、NUMERICS包含性Oracle、22模块实际复现和性能均不在本轮已运行范围。
