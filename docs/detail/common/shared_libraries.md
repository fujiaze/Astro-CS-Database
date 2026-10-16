# 共享基础库

lib/algorithms/shared/ 提供跨阶段复用的几何、内存摘要和标量类型原语，不持有文件通道或业务管线。[依赖标准](../../engineering/standards/DEPENDENCY.md)规定单一实现，[数值标准](../../engineering/standards/NUMERIC.md)规定对象精度，[Runtime](../../engineering/contracts/RUNTIME.md)规定生产资源和寿命。

## 入口与边界

| 公开头 | 入口与职责 |
|---|---|
| healpix/healpix_core.h | acsd::healpix 的 ang2pix_nest、pix2ang_nest、局部位交错、FITS排列、父子层级、query_disc、neighbors |
| crypto/sha256.h | acsd::crypto::sha256_hex 和增量 Sha256，对调用者提供的内存计算摘要 |
| include/astro_scalar.h | AstroScalarType、标量特征与分发支持 |
| include/precision_context.h | 现存 PrecisionContext 单例的标量选择兼容接口 |

HEALPix/SHA 分别有 .cpp 实现，不是纯头库。几何坐标 ra/dec 用度，query_disc 半径用角秒；NESTED 层级差与实际 order 分开。内部像元零基中心、HiPS FITS 排列与独立参照见[映射科学](../../science/algorithms/export/HEALPIX_MAPPING.md)。非法 nside 抛 invalid_argument，pix2ang 的越界 ipix 按现头约定返回零坐标，不能把该哨兵当合法定位成功；层级操作的溢出/范围前提逐函数核对，不将某函数的检查推广到所有 inline。

## 摘要与身份

Sha256 只负责字节摘要，不负责定义科学帧身份。frame_id 的载荷、规范化、截断、排序与缺项必须在实际生产者解释，不能从 SHA 工具存在推断全链统一。

当前 coverage/src/sampler.cpp 的 p2_frame_id 自行组装所选 properties、signal/support 的原生 float 字节和可选 SNR Catalogue，再截取摘要前16个十六进制字符。它不等于完整 tree_hash，也不自动覆盖唯一 frame_science 模型的全部变化；日期字段、原生字节序、目录读取上限和可选面错误语义仍需逐项核对。输入 occurrence 身份也不同于内容摘要，来源和重复输入不能只由截断摘要消歧。

Sha256 实例由调用方持有，不并发修改同一实例，输入内存至少在 update 期间有效。final_hex 为终结调用；当前实现终结后 update 静默返回，再 final_hex 返回64个零字符，调用者不能把它当再次计算成功。文件摘要走 AIO，不在共享算法库自行打开文件。摘要一致不证明来源真实或科学正确。

## 精度、寿命与并发

PrecisionContext 是现有全局兼容机制，默认 FP32，普通字段读写无同步，约定仅启动阶段写一次；跨 DLL 仍须显式传模式，不能以头内单例证明所有动态库共享一个实例。原 orchestrator 有启动设置点，AIO 通过 aio_set_precision_mode 接收；独立三命令的实际路由须沿真实消费者核对。

目标生产模型把精度作为显式对象/调用合同，不能据旧全局开关覆盖稠密FP32、WCS/模型FP64等各对象归属，也不能在运行中无锁切换。几何的无状态调用与返回vector等调用方对象分开管理，错误跨ABI按边界转换，不能让内部C++异常穿C面。

验证覆盖独立几何/排列真值、极区/面缝/非法阶、SHA标准向量与分块、重复终结误用、身份变化面和不同精度/动态库接线。共享库测试只证明所调用原语，原头百万点声明与旧无测试登记属于历史证据；本轮未执行。完整原文与文献保存在[来源档案](../../../实验/documentation-refactor/docs/SUPPORT_SOURCE.txt)。
