# Normalize 天体测量

本节点在校准像元上调用 IPV，自行盲检、匹配本地 Gaia 并求 TAN/SIP。原始 FITS 没有 CRVAL/CD 只是输入属性；实际 solved/unsolved 由求解尝试决定。[Normalize 科学正本](../../science/normalize/NORMALIZE_SCIENCE.md)规定此因果顺序，[天测分册](../../science/detection/ASTROMETRY.md)说明坐标与残差口径。

## 入口、上下游与载体

[module_adapters.cpp](../../../lib/infrastructure/scheduler/src/module_adapters.cpp) 的 `p1_op_wcs` 对应 `acsd.phase1.wcs-platesolve`，输入 `calibrated`，输出 `wcs`；它调用 `ipv_solve_from_memory_with_callback_d`，不依赖下游 `science_sources`。该解供 guided sources、photometry、Drizzle 和 writer。

近似指向/尺度可来自 header_pointing、config、neighbor_crval；它们只是初值。明确配置的完整 WCS 标为 `explicit_config`，不能标为 IPV 求解成功。当前结果写 `p1_wcs.json`；Runtime 对象、attempts 和版本合同见 [Normalize 数据合同](../../engineering/contracts/NORMALIZE_DATA.md)。

## 公开 API 与字段

[ipv_api.h](../../../lib/algorithms/platesolve/cpp/ipv/include/ipv_api.h)声明 `ipv_solve_create/destroy`、`ipv_solve_from_memory*`、`ipv_solve_from_detections_v1`、参数初始化和 inlier 读取接口。后一个入口可接外部候选，但不改变 CLI 从像元求解的实际调用。

| 输出/输入 | 单位与合同 |
|---|---|
| pixels | 行主序校准图像；回调候选属于 solver 私有域 |
| CRVAL、CD | ICRS degree、degree/pixel；几何与匹配元数据 FP64 |
| CRPIX | FITS 一基连续参考像元；外部合法 WCS 不强制位于画幅中心 |
| SIP | A/B 与 AP/BP 按声明阶数和 i*6+j 布局保留；不能丢畸变 |
| rms_px、rms_arcsec、n_pairs | 内部拟合诊断，不能当留出绝对残差 |
| success、error_msg | 成功状态和真实失败原因；不可用解不发布伪几何 |

内部 `(x0,y0)` 是零基数组中心，进入 FITS/SIP 恰加一。IPV adapter 同时处理其自身坐标输出，不能再重复加半像元或一像元。`p1_wcs.json.samples[]` 的 sky 已包含 SIP；消费方不再加一次畸变。完整 SIP 反演、参考历元与 epoch propagation 依正式合同解释。

## 所有权、并行与错误

solver 用 create/destroy 配对，result POD 和 inlier 缓冲由调用方提供；callback 视图只在调用期有效。帧级并行中 solver/detector 按 worker 隔离，Gaia 查询缓存由星表模块管理；线程预算由 Runtime 提供。

返回值与 `success` 一起检查，参数/匹配不足、CD 退化或不收敛逐帧保留原因。取消与失败由 adapter 上行，不能由 API 未声明取消参数推断长调用已经可中断。后续无合法 WCS 的帧不进入 Drizzle；合法其它帧状态单独记录。

## 验证入口与实现边界

[WCS 单元入口](../../../eng/tests/unit/test_wcs_plausibility.py)及[阶段一管线入口](../../../eng/tests/pipeline/test_stage_normalize.py)分别用于几何和实际调用检查。需要独立 WCS reader、留出 Gaia、非零 SIP、RA 回绕、手性、边角和真实无 WCS 头输入；内部往返自洽不证明绝对坐标正确。本页未运行这些验证，具体生产差异与已有证据边界以[实施地图](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)为准。
