# B5｜Export 目标WCS、支持面积和最终科学产品语义

## 数学正本
对一个目标FITS像元 `U_p`、球面HiPS真实叶 `H_i` 及实际支持 `E_i`，定义 `b_{ip}=Ω(E_i∩H_i∩U_p)`。对支持面亮度均值 `Q_{pi}=b_{ip}/Σ_i b_{ip}`（非零支持）；支持积分使用 `Q_{pi}=b_{ip}`，不可把两个估计量的单位混淆。整像元无条件外推是另一个明确请求，不从支持面积比例臆造未观测结构。负信号与零信号保留，无支持为NaN/具名状态，unknown noise不能填零。

## 逐级证明
1. 从外部 FITS/WCS论文导出目标 pixel0↔FITS1、CRPIX奇偶宽高、CD/PC/SIP、镜像、RAwrap、极点及非方像素；产品头解码与生产生成必须是同一数学WCS。
2. 证明目标几何 overlap 真正包含 `E_i∩H_i`，不因 HiPS leaf 只有近似full-leaf支持就凭单个小数补回子叶结构；没有原始支持形状时显式近似。
3. 证明 `Q` 对 signal、可用 Cov/response 使用同一几何身份；同一leaf被多个输出像元引用必须按共享原随机量相关，不能拆成虚构独立像元。
4. 区分球面 `R` 与目标 `Q` 组合后的 estimand、系统响应与PSF，不能将序列化FITS头近似变化视为精确相同模型。
5. HiPS 输出单个正式 signal层，其他支持/模型为元数据/辅助块；层级导出不等于原始帧重新观测，也不默认新增 variance或weight“科学图”。

## 必备反例和审阅
- 2×2/3×3不同CRPIX，旋转/镜像非对角CD，跨RA和12个face、极细支持、高阶目标像元、负/零/NaN。
- 一张leaf被两个像元拆分后忽略相关性的错误方差；部分支持像素把全像元分母用于支持平均；重复pixfrac造成二次收缩。
- 科学真值使用外部WCSLIB/Astropy/解析球面面积与独立高精度积分；第三方读取实际FITS头/data/padding/endian/单位。不能仅同库“写后读回”自证。
- 从 Mosaic manifest 新进程只读加载/恢复后构造目标WCS，完成正式Export；缺science模型保持conditional，不冒充完整物理FITS。

## 交付
数学的`Q`定义/数值区域界与全部失败状态、实际`projection.export`和FITS writer代码审查、外部reader核验与最终产品逆向追踪。最新V2旧X01 order6曾把小天区压到单leaf，说明export图的局部极大/质心不能总当原生测光真值；最终验证应分清source measurement、球面field和二维export三种不同估计对象。
