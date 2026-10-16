# 天体测量与世界坐标系

本分册依据[Normalize科学链](../normalize/NORMALIZE_SCIENCE.md)，定义逐帧天测、坐标桥接、TAN与SIP，以及独立残差评价。正式对象见[Normalize数据合同](../../engineering/contracts/NORMALIZE_DATA.md)，生产调用和证据状态见[科学实施差异](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。

## 1. 求解责任与来源

IPV对校准后的实际像元独立盲检solver_detections、匹配本地Gaia并求解WCS；近似指向、焦距、像元尺寸或邻帧解只是hint。输入头缺CRVAL/CD不是求解失败。合法explicit_config解与IPV求解分别登记来源，均须有可解释的有效域；无解帧记unsolved，不虚构球面位置。

WCS成功后science_sources才以完整WCS+SIP引导；天测私有候选不能代替科学测光星表。ICRS坐标系、Gaia参考历元与观测历元分别记录，自行传播是否执行须明确。ICRS不等于位置冻结在J2000。

## 2. 像元中心与FITS桥接

内部(x₀,y₀)是0基连续像元中心，首像元中心(0,0)、边界±0.5。FITS坐标p=(x₀+1,y₀+1)，整数像元号表示中心[1]。CRPIX是FITS坐标的参考点，不必为整数，也不必居中或位于图像内。

IPV生成居中参考点时，可取CRPIX=((w+1)/2,(h+1)/2)。这是生成策略，不约束外部解、裁剪或重投影产品。已为FITS坐标的求解器输出先由adapter转换到公共内部坐标；每次跨边界只换算一次，不能依据字段名猜测原点。

FITS线性变换把像元偏移映为中间世界坐标[1]。二维TAN+SIP下令u=p₁−CRPIX₁、v=p₂−CRPIX₂，则

\[
 (u_d,v_d)=(u+A(u,v),v+B(u,v)),\qquad
 (\xi,\eta)^T=CD\,(u_d,v_d)^T.
\]

CD单位为deg/pixel，SIP的A_ij、B_ij单位为pixel¹⁻ⁱ⁻ʲ。SIP作用在像元偏移域，先加回偏移再乘CD[3]；用角度多项式系数时必须先转换为此单位，不能把inv(L)和inv(L/3600)混用。若L单位arcsec/pixel，则CD=L/3600、inv(L)单位pixel/arcsec，二者逆矩阵数值相差3600。

## 3. TAN投影及适用域

TAN为中心投影，投影中心在原生北极，其原生球面关系为R_θ=(180/π)cotθ[2]。原生纬度θ=0对应距投影中心90°，在那里发散；这不是天球Dec=±90°的普遍奇点。切点可在天球极区，仍须采用稳定球面旋转和正确LONPOLE。

给定中心(α₀,δ₀)，令Δα取一致的短角差，

\[
 q=\sin\delta\sin\delta_0+\cos\delta\cos\delta_0\cos\Delta\alpha,
\]
\[
 \xi_{\rm rad}=\frac{\cos\delta\sin\Delta\alpha}{q},\qquad
 \eta_{\rm rad}=\frac{\sin\delta\cos\delta_0-\cos\delta\sin\delta_0\cos\Delta\alpha}{q}.
\]

目标在中心半球时q>0，q趋零时放大误差并最终发散；中间坐标再从rad换为deg，与CD单位一致。软件可另设85°等保守能力范围，但它是实现或条件数政策，不能称作TAN的数学奇点，也不能把绝对赤纬85°混同离投影中心85°。

RA对外为[0°,360°)、Dec为[−90°,90°]。极区和RA回绕的查询与匹配采用球面距离或有误差界的包围体；经度差乘cosDec只作受限小场近似。保守剪枝需与全量遍历对照，确保所声明域内不漏候选。

## 4. SIP逆与坐标反射

天球到像元先逆TAN、逆CD得到(u_d,v_d)，再求解前向SIP。可迭代反演A/B，也可采用逆多项式

\[
 u\approx u_d+AP(u_d,v_d),\qquad
 v\approx v_d+BP(u_d,v_d).
\]

AP/BP以畸变后的像元偏移为自变量；它们是独立拟合近似，不是简单把A/B取负。拟合采样覆盖实际域，在独立位置评估表示误差；训练网格残差通常乐观，但最小二乘并不保证网格上残差严格为零。缺逆系数时可迭代前向模型，不得直接忽略畸变。

FITS纵坐标反射p′₂=H+1−p₂时，参考点变为CRPIX′₂=H+1−CRPIX₂，故v′=−v；CD第二列取反，A′_ij=A_ij(−1)ʲ、B′_ij=−B_ij(−1)ʲ，逆系数同理。只有居中CRPIX₂=(H+1)/2时参考点数值不变。该变换保持|detCD|，但改变手性，必须与像元数组反射同步。

## 5. 残差与独立评价

线性面积等效尺度s₀=3600√|detCD|，单位arcsec/pixel。它不含SIP局部Jacobian，也不表示两个方向的尺度均相同；rms_arcsec/s₀仅为该等效尺度下的像元残差摘要。明显各向异性或畸变时，按局部Jacobian和实际方向转换。

分别测量坐标桥接、迭代反演、逆多项式表示、星表匹配和留出星的绝对位置误差。内部往返只证明同一模型自洽，不能证明天球位置正确。负例包括漏加1、重复加1、漏SIP、翻转符号错误和错误历元；它们应在第三方WCS读取与独立真值上暴露。

星表闭环记录输入帧身份、检出样本、星表/历元、星等限制、匹配半径、实际匹配数、匹配率、残差中位/p95/最大及单位。最近邻半径筛选会截断残差分布，不能将1 arcsec以内匹配的中位数单独称作全部天测精度。独立留出星不得与求解内点集合混同。

数值容差依像元尺度、投影中心距离、畸变、求解器容差和浮点环境给出。记录中的1e−4 pixel全链目标、1e−9 pixel迭代停止值或1e−6 pixel桥接比较值都是具体实现/构型参数，不是通用浮点下界；尤其不使用无输入域的绝对容差证明所有仪器。

## 6. 数据交接与参考文献

Drizzle消费本帧已解WCS+SIP以定义真实像元足迹，science_sources与photometry消费同一revision。CRPIX合法性、CD非奇异性、畸变维数、求解状态和来源随产品保存。独立对照可用WCSLIB、Astropy WCS[4]及[NASA登记的SIP定义](https://fits.gsfc.nasa.gov/registry/sip.html)[3]。

[1] Greisen, E. W., Calabretta, M. R. Representations of world coordinates in FITS. A&A, 395, 1061–1075. [论文](https://arxiv.org/abs/astro-ph/0207407)。

[2] Calabretta, M. R., Greisen, E. W. Representations of celestial coordinates in FITS. A&A, 395, 1077–1122. [论文](https://arxiv.org/abs/astro-ph/0207413)。

[3] Shupe, D. L. et al. The SIP Convention for Representing Distortion in FITS Image Headers. ASP Conference Series, 347, 491–495. [NASA约定登记与正文入口](https://fits.gsfc.nasa.gov/registry/sip.html)。

[4] Astropy. [WCS接口与独立变换](https://docs.astropy.org/en/stable/wcs/index.html)。

过程记录与复算示例见[记录](../../../实验/documentation-refactor/docs/ASTROMETRY_BUDGET_RECORDS.md)；正文定义不以未配套复现载体的历史读数作为验证依据。
