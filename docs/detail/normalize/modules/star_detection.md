# 星检测接口与生产调用

本模块连接[检测科学](../../../science/detection/STAR_DETECTION.md)、[数值方法](../../../science/algorithms/normalize/STAR_DETECTION_ALGORITHMS.md)与[Normalize数据合同](../../../engineering/contracts/NORMALIZE_DATA.md)。lib/algorithms/star_detection/提供图像盲检与预测位置拟合；当前科学源表经完整WCS后的Gaia引导产生，IPV候选属于WCS内部，两者不互换。

## 公开入口与内存

公开头include/star_detector.h声明sdet_create、sdet_destroy、sdet_detect_ex、sdet_detect_ex_f64、sdet_detect_guided_ex_f64、sdet_free_detect_ex。实现src/sdet_api.cpp，平滑/统计在src/sdet_image.cpp，LM在src/nls_lm.*；源码行数不作为接口身份。

| 入口 | 输入/输出 |
|---|---|
| create/destroy | SDetParams或null创建opaque handle，匹配destroy释放 |
| detect_ex | uint16行主序图像、宽高、输出地址与extras请求 |
| detect_ex_f64 | double图像，其余输出布局相同 |
| detect_guided_ex_f64 | double图像、pred_x/pred_y、n_pred，另给可空SDetGuidedStats |
| free_detect_ex | 回收x/y、flux、saturated、mag、has_saturated及extras，extra_count须配对 |

图像与预测数组由调用者持有，至少覆盖同步调用；输出由库分配并用专属free一次释放，不能自由混用宿主delete或另一DLL分配器。x/y为double，flux/mag/extras是float，标志为int；f64入口不意味着全部输出列FP64。六个核心输出数组加extras表和count，不存在含糊“十数组都同一精度”的保证。

rc=0含零输出，参数/分配失败返回−1；零源是否影响下游由消费者判断，不能等同完整科学模型成功。拟合失败和质量拒绝不作为有效行输出；错误域、CLI退出码与库rc不同。当前句柄没有共享调用同步保证，按worker独立handle管理，退出前等待在途调用，模块不能私建长期线程池。

## 坐标与单位

科学合同内部整数为像元中心。当前sdet头/拟合使用index+0.5中心，外部预测/输出的转换须在单一adapter核对；不能把二者都标“0-based”而忽略半像元差。当前p1_op_star_psf_impl直接把gx/gy赋给StarSource并以lround取峰，后续完整WCS、PSF和Catalogue关联须共同验证。这里只登记静态路径，不声称已复现半像元偏差或更改原点。

flux的实际赋值为拟合振幅A，mag由方形窗扣背景积分得到；消费者需知道其真实估计器和单位，不能由字段名直接当Moffat总通量。saturated/has_saturated当前按项目平顶/幅度政策产生，不证明物理满阱；输入校准尺度不由65535常数重新归一。extras按请求名解释，高斯宽度不当Moffat宽度使用。

## 当前参数消费

SDetParams现有13字段，默认成员初始化与sdet_create(null)通过消费者的零哨兵规则配对。使用值初始化再设字段，不能用旧9字段表判断ABI大小。

| 字段 | 当前用途 |
|---|---|
| maxStars | 最终输出上限，正值截断 |
| maxAxisRatio | 大于0启用轴比检查 |
| fitRadius | 盲检额外形状量测窗半径，≤0默认6、实际最大12；不是LM候选盒半径 |
| psfFwhmLoRatio/HiRatio | 盲检相对帧FWHM门，0默认0.5/2.5，负值关闭相应门 |
| maxPeakFraction | 盲检峰/正通量比例，0默认0.35，负值关闭 |
| minQuarterMaxPixels | 盲检四分之一峰像素数，0默认4，负值关闭 |
| structureLayers/hotPixelFilterRadius/iterativeClipSigma/iterativeMaxRounds/medianFilterDetail/fwhmClipSigma | 保留字段与既有赋值/日志，未据这些名字承诺有效算法调节；使用前核对当前真实读点 |

额外形状门在sdet_apply_psf_shape_gate，仅盲检路径调用；引导拟合不接受由这些字段带来的盲检召回保证。固定阈值和未启用的CLEAN/DAOFIND/Kron提案不是本轮实现授权，旧替换任务/冻结流程已移入来源记录。

## 引导生产者与消费者

module_adapters.cpp::p1_op_star_psf_impl读取cleaned平面，取得本帧WCS/SIP并经p1_guided_predict查询Gaia和逆投影，调用sdet_detect_guided_ex_f64。数组输出转成StarCatalog，继续PSF拟合及sources工作载荷，供photometry、noise和Drizzle使用。原orchestrator和独立盲检wrapper是不同调用面，存在不等于当前CLI命中。

当前g_count=0分支在adapter直接返回DATA错误并拒绝盲检回退；科学目标允许已有合法signal伴随源/精度不可用，两者需要单独核对实际失败传播，不能由文档宣称该能力已完整实现。该分支不授权将盲检候选改成science_sources。

SDetGuidedStats有n_predicted、n_dropped、n_fit_failed、n_rejected、n_fit_ok、n_output；fit_ok在去重/截断前，output在其后，不把所有计数直接相加当总预测数。引导定义域不代表无错误关联或虚假拟合，仍需独立真源和匹配负例。

## 数值与验证边界

数值公式、核和质量常数归算法页，统计定义归科学正本，本页不复刻全套O1–O16。跨worker重复核对排序与实际数值，不以候选独立推定全链等价；线程/取消预算从真实调用核对。有效缓存、开发探针、科学工作文件各自分工，不使用开发快照取数。

核验包含真实公开入口、错误/空源、输出释放、全部列类型、原点/畸变、源ID关联、参数真实消费、额外形状门与引导路径隔离。正负例独立生成，召回表只在相应校准域解释。本轮已读公开头、主要调用与关键实现段，未构建或运行检测程序，不把旧论文核验与性能数字当新执行证据。

旧逐算子规格、完整数学构造、替换提案、引用冲突、召回表及观察记录完整保存在[来源档案](../../../../实验/documentation-refactor/docs/DETECTION_SOURCE.txt)。科学/数学待核事项集中登记，避免detail再立第二份阈表或“冻结判据”改变上层科学定义。
