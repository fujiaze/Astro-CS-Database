# 11 Stage 1 验证 Spec

## 1. 总体验收目标

同一输入、同一配置、同一版本重复运行，生成数值稳定且元数据完整的 HISS；每阶段的输出都能独立解释和验证。

## 2. READ_FITS

验证：

- NAXIS1/NAXIS2、通道、float 转换；
- BZERO/BSCALE；
- UTF-8 路径；
- WCS/SIP、DATE-OBS、曝光、滤镜、像元、焦距等元数据；
- 截断/损坏文件错误；
- 原始像素统计与参考读取器一致。

## 3. CALIBRATE

必须分别测试：

- 无主帧退化路径：必须显式标记，不得静默成功；
- Bias/Dark/Flat 完整路径；
- 暗场优化；
- 坏点修复被实际调用；
- `cal_stats` 块完整；
- 主帧尺寸/曝光不匹配立即失败；
- 合成真值下校准误差；
- 不产生异常 NaN/Inf。

## 4. PLATESOLVE

验证：

- 合成 WCS 真值；
- 窄/宽 FOV；
- 高赤纬与 RA=0；
- SIP 正反变换；
- 真实回归成功率、RMS、耗时；
- n_detected/n_catalog/n_pairs 统计真实；
- 错误初值与失败路径；
- 文档硬约束必须先确认来源再作为 Gate。

## 5. PSF

验证：

- 浮点输入链路，避免无记录截断；
- Moffat 参数真值与中心偏差；
- 拟合失败的 fallback 决策；
- 饱和星、边缘星、低 SNR 星；
- 批量与单点一致；
- 多线程确定性；
- 输出 `[N,9]` schema。

## 6. PHOTOMETRIC

验证：

- Gaia 查询与复用策略；
- 滤镜和 QE 曲线版本；
- 合成流量积分精度；
- 匹配与异常值剔除；
- scale 与 sigma_residual 真值；
- 能量/通量比例；
- Gaia 失败的错误或受控退化；
- 41 参数接口后续结构化，不在验证前盲目重构。

## 7. SNR

验证：

- `snr_phot = 1/(ln10*sigma_residual)`；
- 控制点筛选；
- WCS 有效性；
- 球面坐标转换；
- median_snr 与退化路径；
- 序列化布局；
- IDW 插值与角距离单位；
- 控制点顺序对结果无影响。

## 8. DRIZZLE

验证：

- nside 策略与 override；
- HEALPix 覆盖不漏像素；
- pixfrac 边界；
- 常量图不变性；
- 通量/能量守恒语义；
- WCS/SIP；
- SNR 模型同步写入 HISS；
- HISS 读取回写；
- 高 nside 的 uint64；
- 线程数与确定性。

## 9. Stage 1 E2E Gate

至少对 DS-SYN、DS-MINI、DS-STAGE1：

- 所有节点通过；
- HISS schema 校验通过；
- 无静默退化；
- 配置字段全生效；
- 输出哈希或数值摘要稳定；
- 性能基线记录；
- 中断恢复与失败清理符合设计。
