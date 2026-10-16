# 母版生成、单帧校准与标度应用

本文细化校准科学正本[1]的离散算法与实际入口。Normalize 的对象身份、单位、有效性和生产消费关系由数据合同与阶段架构规定[2,3]；母版协方差的科学责任不能由本页的实现缺项取消。

## 数据与实现边界

公共内存接口位于 `lib/algorithms/calibration/include/astro_calibration.h`。主要实现为同目录 `src/master_generator.cpp`、`calibrator.cpp`、`cosmetic_corrector.cpp`、`photometry_apply.cpp` 和 `ac_api.cpp`。产品读写由统一 AIO 承担，曝光、母版和单位绑定由调用 adapter 提供，算法不根据文件后缀或目录名推定单位。

`ac_generate_master_bias`、`ac_generate_master_dark`、`ac_generate_master_flat` 生成母版；`ac_calibrate_frame[_f64]` 校准单帧；`ac_correct_frame[_f64]` 做坏点处理；`calibration::apply_photometry[_spatial]` 施加已求得光度标度。拟合光度比例属于 photometry，不能用应用函数的存在证明比例来源正确。

根构建和独立 DLL 装载面分别核对。完整 Normalize 主链从 CLI 的 `run_phase1_block` 经 Runtime 到 `p1_op_*`，局部 `p1_session` 不是另一条完整 WCS→HiPS 链[3]。

## 输入单位与两种暗场约定

FITS 的物理数组值为 `BZERO+BSCALE*stored_value`；它是否为 ADU、电子数或率由来源与单位决定。XISF 浮点 bounds 表示数值范围，不能单独推出到 ADU 的换算因子。只有明确从 16 位整数除以 65535 归一的输入才使用这一因子；12/14 位或其它缩放必须采用自身来源。各母版可有不同文件标度，进入算术前转换为同一物理单位。

令曝光比 K=t_light/t_dark，归一响应 f 无量纲。暗场已去偏置 D_c 时，
\[
 y=(L-B-KD_c)/f.
\]
暗场含偏置 D_t，且采用相同 B 时，
\[
 y=[L-B-K(D_t-B)]/f.
\]
两式适用于不同母版约定，不能把同一个含偏置数组当 D_c 后多减偏置。K=1 时含偏置式化为 `(L-D_t)/f`，而不是对任何暗场类型都可删 B。缺母版只表示未执行该校正，保留能力和来源，不用虚构零误差补齐。

当前 `ac::calibrate` 与 `calibrate_d` 均使用调用方传入的 k_init，并回填 actual_k；没有黄金分割搜索。`dark_opt==1 && bias && dark` 走含偏置分支，否则走独立减 B、K D 的标准分支。缺任一项时跳过对应算术；含偏置模式缺 B 后的回退不保证仍符合用户的物理暗场约定，应显式报告。

暗流按曝光线性缩放需稳定温度、增益及读出条件。缺曝光来源不得假称已由 EXPTIME 求得 K；显式已校准比例与曝光比例的冲突依配置合同解释[4]，不以固定输入确认流程替代物理判断。

## 平场归一及 floor

盘上平场 M 的有效区归一统计量为 m，合法正有限 m 给 `f=M/m`。平场文件中位数不接近 1 本身不是非法输入；合法计数平场应按声明归一。中位数小于 1 的 bias/dark 也不单独证明单位错误。当前 `master_unit_guard` 的幅度/中位数检查只能作为实现政策与诊断，不能当探测器单位定律。

实际校准逐像元使用 `max(flat,0.1)`。对正响应低于 floor 的区域，这改变算子并引入偏差，不是无损稳定化。例如 f=0.05、floor=0.1 时输出幅度为理想值的一半；N=0 时相对误差无定义。无效或非正响应应有有效性状态，不能把 floor 当作已测响应。

`normalize_flat` 是就地辅助原语：中位数非正时原样返回，否则归一并施加 floor。它不证明原样输出是合法归一平场。其生产可达性与 master-flat 生成路径分别核对。

## 通用母版生成

`ac::generate_master` 的输入 stack 为连续 `[n_frames,H,W]`，每像素沿输入帧顺序处理：

```text
n_frames == 1: 复制单帧，不执行跨帧排异
否则对每个像素:
    收集该像素的各帧数值
    重复至 max_iter:
        从非 NaN 样本取 median
        sigma = MAD / Phi_inverse(0.75)
        sigma <= 0 或无样本时停止
        以配置的非对称 sigma_low/sigma_high 剔除，标记 NaN
        无新剔除时停止
    对保留值求 median 或按帧序的 mean
    没有保留值则输出 NaN
```

偶数中位数为中间两个次序统计量的均值。当前代码跳过 NaN，不能据此宣称已经过滤所有 Inf；单帧复制还保留全部缺陷和无效值。MAD 的高斯一致性因子约 1.482602218505602，不是有限样本无偏修正。50% 击穿点不表示污染少于一半时结果完全不受影响，更不是剪裁成功的充分必要条件。

当前 mean 在每个像素内部按输入次序 FP32 累加。更改输入排序、编译重结合或改用双精度会影响舍入；跨 worker 的等价必须实测。固定最终接受集合后才能套用线性组合方差，排异本身有选择效应。

## Master-flat 专用生成

`ac::generate_master_flat` 先对每帧减 B，取非 NaN 样本的中位数，归一且施加 0.1 floor；随后调用通用 sigma-clip mean，最后按合成平场中位数再次归一和施加 floor。

当前实现的边界应如实描述：

| 条件 | 源码行为及限制 |
|---|---|
| 空帧或无有效中位数 | 返回参数错误；与合法无覆盖区区别处理 |
| 中位数非有限或负数 | 拒绝该生成路径 |
| 中位数为零 | 代码将除数置 1，再施加 floor；全零输入可生成常数响应，不能据此称合法母版 |
| 中途失败 | norm 私有缓冲会释放，但调用方 out 在后续失败前可能已被写入；非零返回不保证缓冲逐位未改 |
| 部分 NaN | 不参与中位数，数组中的对应位置仍须按后续算术核对 |

科学目标要求归一统计量正有限、有效域可解释。不能借“保持兼容”把零中位数路径提升为正确科学设计。生成需要完整 norm 栈 O(NHW) 额外内存，不是仅 O(N)；通用合并则使用每线程 O(N) 暂存。

## 不确定性与共享来源

校准输出的不确定性应按 `C_y≈J C_z Jᵀ` 传播[1]。在独立输入和固定 K 的受限模型下，已去偏置暗场式的方差为
\[
 V_y=(V_L+V_B+K^2 V_{D_c})/f^2+y^2 V_f/f^2.
\]
实际共享 B、归一统计量、母版及拟合参数时需保留交叉项；含偏置式中 B 的导数为 `(K-1)/f`，K=1 时共同 B 的贡献抵消。多个 light 共用 master 还产生跨帧共同误差，不随帧数自动消失。

仓内 `src/calibration_covariance.cpp` 和 `include/acsd/calibration/calibration_covariance.h` 提供相关辅助实现；其存在不证明 Normalize 当前已完整传播 master Jacobian、归一和修复相关项。缺项记录 unmodelled/approximate；背景 MAD 不唯一恢复这些量。不能再将“variance 不传播”写成永不传播的科学规则。

## 光度比例应用一次

`apply_photometry` 检查输入/输出、尺寸和正有限比例，逐像素以 double 中间乘法后写 float，可原地使用；它仍保留输入 NaN/Inf，不保证消除所有非法值。空间版本应用 `alpha(x,y)=k_photo*m(x,y)`，order=0 委托标量版本。配置、场支持与实际单位必须同实际 applied plane 绑定。

确定性标度使信号乘 alpha、随机协方差乘 alpha_i alpha_j；由数据拟合的标度还有参数不确定性与交叉项。后续 Drizzle 不再重复应用。测试同时核对应用前后数值和来源；接近零的值用合适绝对尺度与相对误差组合，不强迫在零分母下用相对误差。

k_photo 的数值依响应、单位、归一和曝光等约定而定，不能笼统说“绝对值没有物理意义”。小残差只支持选定模型与样本上的拟合，不唯一识别通带，也不证明对未知星云 SED 的转换无偏。

## 精度、错误和并发

`ac_calibrate_frame_f64` 直接调用 double 校准；当前若干 master/cosmetic `_f64` 门面仍转换到 float 再回写 double，名字与输出 dtype 不代表内部 FP64。C API 的指针/尺寸检查、C++ void 原语静默返回、异常屏障及数组乘法溢出需按各函数逐项核对，不能用 AC_ERR_MEMORY/INTERNAL 枚举存在证明分配异常已处理。

OpenMP 的逐像元工作应消费宿主租约；原始 `ac_set_num_threads` 改写调用线程 ICV，不能称完善的对象级预算 API。模块内无完整行带取消回调时，应报告当前取消粒度。坏点的四方向 IDW 和中位数修复详见[5]。

## 独立检验与证据边界

保留以下可区分缺陷的构型：常量与解析梯度；K≠1 的两种暗场；非零 B 的参与及 K=1 共享 B 抵消；低响应 floor 偏差；不同文件标度转换到同单位；单帧和带污染/NaN/Inf 的 stack；零/负归一中位数；f64 低位保持；同输入不同 worker；取消与失败后的产品状态。

独立 NumPy 逐像元 oracle 必须使用正确单位与母版约定。仅“提供与不提供 bias 的输出不同”不能发现少减三万倍的 bias；只检查图像中位数又可能漏掉空间错误。完整残差分布、有效域和实际输入来源必须一起记录。性能加速比和内存界由受控实测确定，不预设任意机器必达 1.60 倍。

登记中的 XISF 构型提供单位风险线索：bias 中位 0.015288 对应声明的 1001.87 ADU，flat 中位 0.206381 对应约 4.845 的未归一除法；NGC1727 示例中位由 7048.618 到 436.155 ADU，Galaxy Center 示例由 3650.390 到 361.236 ADU。暗场两曝光外推截距不能单独证明暗场模型完全正确。这些数值在本页未绑定完整复跑输入与命令，不作为当前运行或当前单位处理已修复的证据。

共址 `tests/test_photometry_apply.cpp` 的路径需以当前文件树核对，不能由文档登记推断测试存在或已通过。数学、静态接线、已有局部记录与真实全链运行分别报告[3,6]。

单位声明、暗场外推、真实帧标度和 NumPy 比较的详细保留读数与来源身份见[证据记录](../../../../实验/normalize-precision-p5/DOCUMENTATION_EVIDENCE_NOTES.md)，其中未复测部分不能当成当前运行结果。

## 参考文献

[1] [校准算术与共享误差](../../calibration/CALIBRATION.md)。

[2] [Normalize 数据合同](../../../engineering/contracts/NORMALIZE_DATA.md)。

[3] [Normalize 生产链与责任](../../../engineering/architecture/NORMALIZE_PIPELINE.md)。

[4] [配置合同](../../../engineering/contracts/CONFIG.md)。

[5] [缺陷检测与修复算法](COSMETIC_ALGORITHMS.md)。

[6] [科学设计与实现差异](../../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。
