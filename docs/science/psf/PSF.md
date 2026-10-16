# PSF轮廓、像元积分响应与空间模型

本分册依据[Normalize科学链](../normalize/NORMALIZE_SCIENCE.md)，定义点源轮廓、尺度、总通量、有限窗口与空间积分响应。检测的高斯候选参数与本页Moffat参数分名；参考通量估计及协方差由[噪声分册](../noise_snr/NOISE_SNR.md)定义。正式对象见[Normalize数据合同](../../engineering/contracts/NORMALIZE_DATA.md)，实现状态见[科学实施差异](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。

## 1. 连续轮廓与离散像元

在以pixel为长度单位的连续探测器坐标中，局部轮廓写为

\[
 I(x,y)=B+A(1+Q)^{-4},\qquad
 Q=p_1dx^2+2p_2dxdy+p_3dy^2.
\]

A、B是连续密度振幅和背景，单位为linear_unit/pixel²，面积元dxdy的单位为pixel²；积分才得到linear_unit。探测器数组记录像元积分值x_i，其单位为linear_unit。以像元中心函数值近似像元积分时，隐含乘单位像元面积；数值相近不代表两者的单位或欠采样行为相同。拟合参数若来自中心采样近似，必须说明其振幅到密度的解释，不把数组ADU写成ADU/pixel的一维密度。

相对初始中心(cx,cy)的偏移为(x₀,y₀)，dx=x−(cx+x₀)、dy=y−(cy+y₀)。内部整数0基坐标表示像元中心，边界为中心±0.5。

\[
 p_1=\frac{\cos^2\theta}{2s_x^2}+\frac{\sin^2\theta}{2s_y^2},\quad
 p_2=\frac{\sin2\theta}{4s_x^2}-\frac{\sin2\theta}{4s_y^2},\quad
 p_3=\frac{\sin^2\theta}{2s_x^2}+\frac{\cos^2\theta}{2s_y^2}.
\]

s_x、s_y单位为pixel，θ为rad。该七参数模型适合局部近似椭圆对称星核，不能描述任意光晕、非对称像差、饱和或混叠。背景常量只在实际窗口内适用。

## 2. 尺度、二阶矩与FWHM

圆形时s_x=s_y=σ，Q=r²/(2σ²)。与Moffat族(1+r²/α²)⁻β比较，本页固定β=4、α=√2σ。一般β>2时

\[
 \langle r^2\rangle=\frac{\alpha^2}{\beta-2},\qquad
 \sigma_g=\sqrt{\langle x^2\rangle}
 =\frac{\alpha}{\sqrt{2(\beta-2)}}.
\]

二阶矩的逐轴与径向口径分别定义[1]。本页σ=α/√2只有在β=4时等于径向RMS；把指数改成其它β后，参数σ可仍定义为α/√2，却不再等于径向RMS。径向RMS与逐轴二阶矩高斯宽度始终相差√2，不能把这一关系无条件套到固定参数σ。

半高点Q=2¹ᐟ⁴−1给出主副轴全宽

\[
 FWHM_{\rm major/minor}=f_4s_{\rm major/minor},\qquad
 f_4=2\sqrt2\sqrt{2^{1/4}-1}=1.230307652590102.
\]

一般Moffat族的FWHM=2α√(2¹ᐟᵝ−1)。沿图像坐标轴的截面宽度另为2√((2¹ᐟ⁴−1)/p₁)与2√((2¹ᐟ⁴−1)/p₃)，不能把主副轴宽度按列名误当成图像x/y截面宽度。圆整系数1.230310相对闭式约差1.908×10⁻⁶；数值比较应注明使用的系数及容差来源。

θ的周期为π，且(s_x,s_y,θ)与(s_y,s_x,θ+π/2)为同一轮廓。先归轴s_x≥s_y，再将θ归入[0,π)；圆形时取θ=0为规范。只旋转θ而不交换轴一般改变轮廓，不是等价重参数化。椭率可定义e=√(1−(s_min/s_max)²)，并注明它与其它椭率定义不同。

## 3. 解析总通量与有限窗口

令M=[[p₁,p₂],[p₂,p₃]]，detM=1/(4s_x²s_y²)。将Q化为圆形二次型得

\[
 F_{\rm total}=\iint A(1+Q)^{-4}dxdy
 =\frac{\pi A}{3\sqrt{\det M}}=\frac{2\pi As_xs_y}{3}.
\]

该量是整平面模型积分，单位linear_unit。有限圆窗在圆形模型下有

\[
 f_{\rm out}=(1+r_{\rm win}^2/(2\sigma^2))^{-3},\quad
 F_{\rm window}=(1-f_{\rm out})F_{\rm total}.
\]

r_win<2.1063228378790524σ时窗外模型份额超过3%；r_win=√2σ时为12.5%。3%是具名支持诊断，不是所有真实星核的总光度误差上界。椭圆、方形、坏点掩膜及像元响应需按实际域积分。

解析总通量、窗内孔径通量及经修正的估计器分别命名。总通量依赖窗外模型成立，窗内积分依赖真实窗口；两者的随机误差、模型偏差和截断不同。有限窗保留单位总通量模板时ΣP_i<1；若重新归一成窗内单位通量，须同步改变F_ref。

## 4. 残差尺度与噪声

在数组积分值域拟合时，残差r及其尺度的单位为linear_unit。将|r|排序后取10%–90%段均值，得到s_trim。对独立、固定分布、未受拟合和选择影响的高斯残差，渐近比例为

\[
 \frac{E[s_{\rm trim}]}{\sigma_n}
 =\frac{2[\phi(\Phi^{-1}(0.55))-\phi(\Phi^{-1}(0.95))]}{0.8}
 =0.7316730952806130.
\]

圆整常量0.7316727929211932相对差约4.13×10⁻⁷。此常数不同于MAD的1/Φ⁻¹(0.75)。按lo=⌊0.1m⌋、hi=⌊0.9m⌋取有限样本秩区间时存在样本量及余数相关偏差，附录保留其复算代码。对同数据拟合后的残差、相关噪声、结构污染或非高斯尾，不能单凭该渐近比例恢复独立背景噪声。

若振幅与残差用相同数组单位，q_psf=A_sample/s_trim为无量纲拟合质量代理。A_sample在连续密度表示时须先乘像元面积或采用一致采样定义。q_psf不等于通量SNR，也不作为球面像元逆方差。拟合量的误差由参数设计、背景、采样、协方差及模型失配共同确定。

## 5. 空间像元积分响应

先将局部连续星核按总积分归一，得到密度p(x,y;u)；在各实际像元内积分

\[
 P_i(u)=\int_{\mathrm{pixel}~i}p(x,y;u)\,dxdy.
\]

P_i无量纲，完整响应域ΣP_i=1；子像元相位必须进入积分。像元内非均匀响应若已知，一并包含。欠采样时不以中心函数值替代该积分。

圆对称 Moffat4 的单位总通量密度为 p=3g/(2πσ²)；椭圆情形为 p=3g/(2πs_xs_y)。先归一连续轮廓，再在实际像元足迹求积，不能将裸中心值的和钳到1充当包络。对不重叠的实际像元集合，f_enclosed=ΣP_i、f_out=1−f_enclosed；像元重复或求积误差须报告，不能用 clamp 隐藏超过1的结果。

正式估计器身份分为：`analytic_total` 表示上述总源通量，是 canonical 参考的默认口径；`aperture_with_correction` 要求有明确孔径修正及其误差，并与 F_ref 配对；`diagnostic` 不具正式参考精度身份；无有效模型时为 `unavailable`。截断窗口不能通过重新归一 P 自动成为孔径修正。

模型逐控制点保存 PSF 参数、子像元相位、足迹、积分方法/阶数、误差估计和有效域。帧级最小包络或最大误差只能作摘要，不能代替每个控制点的实际值。相邻求积阶数的差是自检指标，不是任意窄核、相位和椭率下的严格误差上界；需要独立求积、细分收敛或明确有效域。

一个解析反例是圆形宽度 s→0，中心位于像元内部并避开两个固定阶数的全部有限节点。每个节点到中心的距离为正，归一 Moffat4 密度在这些点为 O(s⁶)，故两阶求积值及其差均趋零；真实像元积分却趋1。两阶相近不能在无宽度和相位限制的域内保证精度。这个反例不等于已测得生产故障；有效域和真实误差仍须独立验证。积分误差怎样传播到条件通量信息，见[噪声分册](../noise_snr/NOISE_SNR.md)，源泊松协方差随模板变化时还需同时控制协方差误差。

基础空间模型在有效星位置三角剖分内采用重心凸组合：各顶点单位总通量模型先平移到共同参考中心，再按同一像元边界积分，取P_i(u)=Σ_sλ_s(u)P_i(θ_s;u)，λ_s≥0、Σλ=1。不直接线性插值有周期简并的θ；支持空洞、跨无效区和凸包外位置不自动填满。

凸组合保持正值与总积分，但不保证真实空间PSF无偏。模型状态为approximate，通过空间留出星和注入回收评价；全帧常PSF仅作为有证据的具名受限近似。无支持位置precision unavailable，已有合法signal可保留。

Drizzle消费同一P并形成h=RP，噪声协方差同时传播。PSF、photometry、noise共享源绑定与通量估计器身份，检测Gaussian宽度不能直接当作Moffat参数。

## 6. 诊断与独立核验

核验覆盖半高点、平移、归轴等价、解析积分、有限窗份额、子像元相位和空间留出。对不同θ的非等价模型取最小残差会产生选择效应，不能宣称该步骤保持参数或残差。无噪声恒等式检验数学定义，带噪重复采样检验偏差和覆盖，二者分别报告。

附录数值积分构型s_x=2.3、s_y=3.1、A=7在[-30,30]²的积分约104.5309487，整平面解析约104.5312596；有限窗与求积离散化要分别收敛，不能仅由一个残差断言误差全来自窗外。已有Moffat尺度对照见[尺度复算脚本](../../../实验/absolute-snr/code/audit/route2/exp04_moffat4_sigma.py)。生产调用与模型能力由工程差异记录，不因本页公式存在就视作完成。

## 7. 参考文献

[1] Meyers, J. E., Burchat, P. R. Impact of Atmospheric Chromatic Effects on Weak Lensing Measurements. ApJ, 807, 182. [论文](https://arxiv.org/abs/1409.6273)，使用逐轴二阶矩及其迹定义大小。

[2] Astropy. [Moffat2D模型](https://docs.astropy.org/en/stable/api/astropy.modeling.functional_models.Moffat2D.html)，通用轮廓参数与FWHM的独立实现对照。

## 附录：闭式与有限样本复算

以下自包含代码保留闭式、求积、旋转和截尾统计的复算入口；它是推导示例，不调用生产代码，不构成生产运行或真实数据验证。注释数字为待复核的示例期望，有限样本随机输出依Python实现和浮点环境解释。Moffat轮廓的一般参数命名可与[2]对照；本页固定β=4的口径由正文定义。

```bash
python3 - <<'PY'
import math, random

def Phi_inv(p):                       # 标准正态分位点，二分法
    lo, hi = -12.0, 12.0
    for _ in range(200):
        mid = 0.5 * (lo + hi)
        if 0.5 * (1 + math.erf(mid / math.sqrt(2))) < p: lo = mid
        else: hi = mid
    return 0.5 * (lo + hi)

phi = lambda x: math.exp(-0.5*x*x) / math.sqrt(2*math.pi)
a, b = Phi_inv(0.55), Phi_inv(0.95)
print("Phi_inv(0.55)     =", a)                                # 0.12566134685507402
print("Phi_inv(0.95)     =", b)                                # 1.6448536269514715
print("kTrimMeanToSigma  =", 2*(phi(a)-phi(b))/0.8)            # 0.731673095280613
print("FWHM/sigma        =", 2*math.sqrt(2)*math.sqrt(2**0.25-1))   # 1.2303076525901024
print("MAD factor        =", 1/Phi_inv(0.75))                  # 1.4826022185056023

# 解析通量：复合辛普森积分对任意 sx, sy, theta 都应给出 2*pi*A*sx*sy/3
sx, sy, A = 2.3, 3.1, 7.0
th = math.pi/2*(3.0 + math.sqrt(5.0))
p1 = math.cos(th)**2/(2*sx*sx) + math.sin(th)**2/(2*sy*sy)
p2 = math.sin(2*th)/(4*sx*sx) - math.sin(2*th)/(4*sy*sy)
p3 = math.sin(th)**2/(2*sx*sx) + math.cos(th)**2/(2*sy*sy)
N, L = 4000, 30.0
h = 2*L/N
acc = 0.0
for i in range(N+1):
    x = -L + i*h
    wx = 1 if i in (0, N) else (4 if i % 2 else 2)
    for j in range(N+1):
        y = -L + j*h
        wy = 1 if j in (0, N) else (4 if j % 2 else 2)
        acc += wx*wy*A/(1 + p1*x*x + 2*p2*x*y + p3*y*y)**4
num = acc*h*h/9
ana = 2*math.pi*A*sx*sy/3
print("flux (Simpson)    =", num)                              # 104.5309487261
print("flux (analytic)   =", ana)                              # 104.5312595604
print("flux rel diff     =", abs(num-ana)/ana)                 # 2.974e-06

# 旋转简并：固定 sx,sy 时四个候选给出三个不同的二次型；真简并生成元须同时交换两轴
Q = lambda t, sx, sy: (math.cos(t)**2/(2*sx*sx) + math.sin(t)**2/(2*sy*sy),
                       math.sin(2*t)/(4*sx*sx) - math.sin(2*t)/(4*sy*sy),
                       math.sin(t)**2/(2*sx*sx) + math.cos(t)**2/(2*sy*sy))
base = Q(0.37, 3.0, 2.0)
for name, t2 in [("theta", 0.37), ("pi/2-theta", math.pi/2-0.37),
                 ("pi/2+theta", math.pi/2+0.37), ("pi-theta", math.pi-0.37)]:
    print("  candidate %-12s max|dQ| = %.12f" % (name, max(abs(a-b) for a, b in zip(Q(t2, 3.0, 2.0), base))))
print("  generator g (sy,sx,th+pi/2)  max|dQ| = %.3e" %
      max(abs(a-b) for a, b in zip(Q(0.37+math.pi/2, 2.0, 3.0), base)))

# 拟合窗截断：f_out > 3% 的精确阈值，以及 sqrt(2)*sigma 处的取值
r3 = math.sqrt(2*((0.03)**(-1/3) - 1))
print("f_out=3% r/sigma  =", r3)                              # 2.1063228378790524
print("f_out(sqrt2 sigma)=", (1+1)**-3)                        # 0.125
for r in (1.5, 1.8, 2.0):
    print("  f_out(%.1f sigma)          =" % r, (1+r*r/2)**-3)  # 0.1042 / 0.0556 / 0.0370

# 另一口径 sigma_g：beta 一般式，与 FWHM 因子的比值恰为 sqrt(2)
F_sig   = 2*math.sqrt(2)*math.sqrt(2**0.25-1)                  # FWHM / 本册 sigma
F_sig_g = 4*math.sqrt(2**0.25-1)                               # FWHM / 逐轴 sigma_g
print("FWHM/sigma_g      =", F_sig_g)                          # 1.739917768184329
print("FWHM_sigma_g/sqrt2=", F_sig_g/math.sqrt(2))             # 1.2303076525901024
print("sqrt(2)           =", math.sqrt(2))                    # 1.4142135623730951
print("ratio of factors  =", F_sig_g/F_sig)                   # 1.414213562373095
print("sigma_g (beta=4)  =", 1/math.sqrt(2*(4-2)))             # 0.5

# 半高点方程自洽性；跨口径代入确实给不出 0.5，且原路反解只回到被代入的因子
rh = math.sqrt(2*(2**0.25-1))
print("r_half/sigma      =", rh)                               # 0.6151538262950512
print("(1+r^2/2)^-4       =", (1 + rh*rh/2)**-4)                # 0.5000000000000001
r_cross = F_sig_g/2
print("cross-conv r      =", r_cross)                          # 0.8699588840921645
print("cross-conv value  =", (1 + r_cross*r_cross/2)**-4)      # 0.2770007905069461
v_back = (1 + r_cross*r_cross/2)**-4
print("cross-conv back   =", 2*math.sqrt(2*(v_back**(-0.25) - 1)))  # 1.739917768184329

# 面元方向半高全宽：闭式 1.2303076525901024/sqrt(2*p1)，及两端严格界
p1_of = lambda t, sx, sy: math.cos(t)**2/(2*sx*sx) + math.sin(t)**2/(2*sy*sy)
for mj, mn in ((3.1, 2.3), (2.3, 3.1)):
    row = []
    for t in (0.0, 0.30, 0.70, 1.20, math.pi/2):
        fx = F_sig/math.sqrt(2*p1_of(t, mj, mn))
        row.append("%+.2f%%" % (100*(1.230310*mj/fx - 1)))
    print("  sx=%s sy=%s  1.230310*sx/FWHM_x - 1 = %s" % (mj, mn, " ".join(row)))

# 截尾均值偏差随 m mod 10 的符号：标准库蒙特卡洛，种子 20260901+m，每点 20000 次
for m in (8, 10, 11, 12, 20, 25, 30, 101):
    rnd = random.Random(20260901 + m)
    lo, hi = int(m*0.1), int(m*0.9)
    s = sum(sum(sorted(abs(rnd.gauss(0.0, 1.0)) for _ in range(m))[lo:hi])/(hi-lo)
            for _ in range(20000))
    print("  m=%4d (m%%10=%d)  trimmed-mean dev = %+.2f%%" % (m, m % 10, 100*(s/20000/0.7316730952806130 - 1)))
PY
```
