# Export 管线与平面文件

Export 将单 signal HiPS 按目标 WCS 重采样为普通二维 FITS。输入输出范围由最高设计规定，投影和采样的科学含义由专题正本规定 [1–3]。本页描述阶段内交接、裁剪与实现限制。

## 输入与输出

输入具有可解释坐标、光度标度和有效域。科学主流程消费 Mosaic 产品；独立单帧产品是否兼容，由实际 reader 合同确认。导出不依赖最终马赛克不存在的 variance、ivar、PSF 或点源信息量图层。

输出是二维浮点信号图像，携带 WCS、BUNIT、来源和实际采样方法。有限零和负值合法，无有效贡献写 NaN。HiPS 的分辨率层级属于同一信号通道，选择粗层会改变有效分辨率。

## 节点交接与资源

| 节点 | 消费 | 产生与消费者 |
|---|---|---|
| properties | 输入产品及来源 | 几何、层级、单位和身份，供规划与采样使用 |
| wcs | 输出中心、尺寸、尺度、方向、投影与裁剪请求 | 未裁剪 WCS 和窗口，供采样、writer 与校验使用 |
| resample | 输入信号、WCS 和实际核 | 平面结果和实际层级，供 writer 使用 |
| writer | 平面信号、窗口、WCS 和来源 | 二维 FITS 及摘要，供独立 reader 核验 |
| verify | 完成的文件及请求描述 | 读回、坐标与数值一致性结果 |

Runtime 管理线程、缓冲和取消，I/O 管理 tile 缓存及输出文件。生产 writer/verify 已有 P3FitsStream 与 P3FitsVerifyStream 的子块路径；这证明存在流式处理路径，仍需实际峰值测量核对全阶段内存上限。当前适配器的 p3_resampled.json、p3_wcs.json 是实际阶段内依赖，其生命周期按 Runtime 和 I/O 合同管理 [4,5]。

## WCS 与重采样

内部数组中心从零起，FITS 像元中心坐标从一起，两者相差一。投影有效域、RA 回绕、手性和像元中心分别核验。生产 p3_wcs_make 当前实现 TAN，配置枚举不等于已实现投影。

p3_wcs_validate_request 及相关变换包含当前软件的纬度和视场限制；这些不构成 TAN 在某个赤纬必然发散的理论结论。TAN 奇点取决于方向距投影中心的角距离 [6]。

最近邻与局部双线性核处理线性面亮度。常值保持、目标面积平均和点源孔径通量分别核验；部分邻域失效后的重归一改变有效核。精确面积交叠是独立几何能力，未接入生产时不能列为已实现 [2,3]。

最终单信号输入不足以恢复完整协方差。研究中若另有可靠协方差，固定算子可用 C_out=R C_in Rᵀ；该公式不提供从单图生成绝对方差的方法。

## 矩形裁剪

裁剪在已定义画幅上取子窗。像素形式采用配置声明的 FITS 一基闭区间；天区形式通过目标 WCS 转为像素窗口。字段和互斥关系由导出配置定义 [7]。

子窗在未裁剪数组中的零基原点为 (x0,y0) 时，CRVAL 与线性变换不变，CRPIX 分别减去 x0、y0。子窗像元对应整幅输出的相同位置，裁剪不增加一次插值。应独立核验非对称子窗，不要求裁剪后参考像元仍位于画幅中心。

当前 p3_crop_window_from_sky 固定密度采样天区边界，再求整数包围盒。有限采样本身不给连续边界极值的严格包络保证；需要独立误差或自适应极值控制才能声称不裁掉目标区域。越界、空窗和投影域错误由接口给出具体状态。

## 文件与质量核对

当前 p3_op_writer 使用单 HDU，并明确不确定度不可用。它仍含固定面亮度 BUNIT 和输出模式检查；相对标度或模型通带输入需要核对这些实现与真实单位合同的差异 [8]。

独立 FITS/WCS reader 检查头、尺寸、像元、坐标与裁剪。目标画幅可大于足迹，内部也可能有真实未观测区域；NaN 的拓扑位置不足以单独判缺陷。输出支持应与输入有效域及采样核比较，区分合法缺数、异常丢失和无来源有限值。

质量评价分别测量 CCD 到球面、HiPS 层级派生、球面到平面的影响，检查星点光度及形状、扩展源低频和边界。单信号通道允许这些独立实验，但不意味着绝对误差已随文件发布。

## 参考文献

[1] [ACSD 最高设计](../../ACSD_DESIGN.md)。

[2] [HiPS 到平面 FITS](../../science/projection/HIPS_TO_FITS.md)。

[3] [球面到平面的重采样](../../science/resample/RESAMPLE.md)。

[4] [Runtime 合同](../../engineering/contracts/RUNTIME.md)。

[5] [统一 I/O](../infrastructure/aio.md)。

[6] Calabretta M. R., Greisen E. W. Representations of celestial coordinates in FITS. A&A, 395: 1077–1122. https://doi.org/10.1051/0004-6361:20021327

[7] [配置合同](../../engineering/contracts/CONFIG.md)；字段定义位于 eng/contracts/schemas/phase_config_export.schema.json。

[8] [科学设计与生产实现差异](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。
