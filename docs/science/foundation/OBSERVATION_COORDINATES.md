# 观测、坐标与像元：重建科学基础

**候选科学规范，待外部独立审核。** 本目录的五篇文档是 #111 所要求的重新开发依据；旧 science、工程合同和 C++ 只作参考，不要求新算法继承其对象或调度。本文拥有观测模型、坐标和 Export 定义；[响应](GAIA_RESPONSE.md)、[几何](SPHERICAL_DRIZZLE.md)、[统计](STATISTICS_SKY_COADD.md)、[证据](INDEPENDENT_ORACLES.md)分别展开其余定义。本文不声称旧产品或真实数据已验收。

## 1. 量、单位与前向模型

| 符号 | 定义 | 单位 |
|---|---|---|
| \(I_\lambda(\omega,t)\) | 到达大气前的天体光谱辐亮度，含所有真实扩展结构 | W m⁻² sr⁻¹ nm⁻¹ |
| \(f_{s,\lambda}\) | 恒星的光谱辐照度 | W m⁻² nm⁻¹ |
| \(\omega\) | ICRS 单位方向；另附观测历元 | 无量纲向量 |
| \(A_0,T,Q,E_{\rm atm}\) | 集光面积、光学/滤镜透过率、QE、大气透过率 | m²、其余无量纲 |
| \(P_i(\omega,\lambda,t)\) | 一个入射光子最终被像元 i 收集的几何概率，尚未乘 QE | 无量纲 |
| \(g,b_{\rm elec},\sigma_r\) | 原生 gain、电子偏置、读出 RMS | e⁻/ADU、ADU、e⁻ |
| \(d_i,x_i,B_p\) | 原始数值、校准后积分线性量、重建面亮度 | ADU、L、L/sr |

这里 L 必须绑定具名尺度：native ADU、同设备相对线性尺度、或本文响应分册定义的 photon-model 标度。标度未获得物理锚时不得写成电子或 SI 辐亮度。

假定探测器在线性、未饱和域，曝光 k 的电子期望为

\[
\mu_{ki}=\int_{t_k}^{t_k+\Delta t_k}\!dt\int d\Omega\int d\lambda_{\rm nm}
 I_\lambda(\omega,t) A_{0,k} T_k(\lambda,\omega,t)Q_k(\lambda)
 E_{{\rm atm},k}(\lambda,\omega,t){\lambda_{\rm nm}10^{-9}\over hc}
 P_{ki}(\omega,\lambda,t)+\mu_{{\rm sky},ki}+\mu_{{\rm dark},ki}.
\tag{1}
\]

\(e_{ki}\sim\operatorname{Poisson}(\mu_{ki})\)，读噪 \(r_{ki}\) 零均值，
\(d_{ki}=b_{{\rm elec},ki}+(e_{ki}+r_{ki})/g_{ki}\)。额外 sky 项只含尚未放进 \(I_\lambda\) 的大气/仪器背景；不得双算夜空辐射。偏置不是 photon 均值。相关读噪、非线性、IPC、CTI、rolling shutter、flat 和共享母版另作为有身份的响应/误差项；它们不因式(1)写成简式而被假定为零。

一个明确的收集核是
\[
P_i(\omega,\lambda,t)=\int_{P_i^{\rm det}}
 p_\lambda(\omega'\mid\omega,t)\,\rho_i(u,v,\lambda)\,
 \left|J_\Omega W(u,v)\right|du\,dv,\quad\omega'=W(u,v).
\tag{2}
\]
PSF \(p\) 的单位 sr⁻¹，按出射方向积分为1；\(\rho_i\) 为 PRF/相对像内效率。无损完整探测器且 PRF 覆盖完整时 \(\sum_i P_i=1\)，有限 ROI/坏区允许小于1。源位置是连续 PixelCenter0 参数；用式(2)拟合像元**积分**，不能默认在整数位置采样连续 PSF 就等于积分。高斯可用 erf，Moffat/Airy 用自适应二维求积并给尾通量及求积误差。有限窗不重新归一为“总通量”。

作用顺序为：光谱/时间天空 → 波长相关传播与 PSF → PRF/像元积分 → 光电子及读噪 → 母版/flat 校准 → 经验光度乘性标度 → 球面 R → 可比较观测的相对 sky → 合成 → Export。只有空间恒定标量与线性空间算子可无条件交换；空间 flat 与卷积、色相关 PSF 与通带积分、像元积分与采样、背景拟合与排异通常不交换。跨仪器不存在由坐标对齐自动得到的共同 PSF。

**DERIVED**：式(1)的量纲、线性算子次序及 shot-noise 传播。**ASSUMPTION**：给定曝光内的线性响应、充分 PSF/PRF 模型。**REQUIRES_REAL_DATA**：真实 flat/大气色项、非线性及 PSF/PRF 适用性。

## 2. 唯一坐标合同

数组为 `data[y,x]`，shape 为 `(height,width)`，x 为列、y 为行。第一像元中心 `(0,0)`，面积单元 `[-.5,.5]²`；完整图像域 `[-.5,width-.5]×[-.5,height-.5]`。中心和边缘都允许实数，不能把两者混成“整数像素”。

```cpp
struct PixelCenter0 { double x, y; }; // detector pixel
struct PixelEdge0   { double x, y; }; // 同一连续平面、不同对象角色
struct FitsPixel1   { double x, y; }; // 唯一边界转换 p1=p0+(1,1)
struct SkyICRS     { double nx, ny, nz; }; // 单位向量；历元另附
struct NestedLeaf  { uint32_t order; uint64_t ipix; };
struct FacetPoint  { uint8_t face; double s, t; }; // s,t∈[0,1]
```

坐标角以 radians 运算；FITS CD/CUNIT 与外部 RA/Dec 接口按明确单位转换。天空角误差用 `atan2(norm(n1×n2),n1·n2)`，极点不比较 RA，RA 回绕不作欧氏减法。Gaia catalog 的 ICRS 位置须按 proper motion、parallax 和观测时间传播到实际匹配历元；输入合同包含 epoch 与时间尺度、`pmRA*cosDec` 约定、观测者站址或 barycentric position，已知 RV 时纳入 perspective acceleration。缺失信息时记录忽略项及其位置误差预算，不能只凭 parallax 和时间制造已知 topocentric 方向。大气折射/DCR 另有观测条件，不能偷偷改变 ICRS 定义。

FITS/WCS 接口仅消费 `FitsPixel1`，其逆只返回同类；公共内部接口负责一次 `±1`。Astropy `all_pix2world(p0,0)` 等价 `all_pix2world(p0+1,1)`；`pixel` API 顺序 x,y，`array_index` API 顺序 row,col。完整 SIP 要用 `all_*` 而非只含线性核心的 `wcs_*`。

## 3. TAN、SIP 与逆函数

令 \(r={\tt CRPIX}\) 为 FITS 1-based，\(u=p_0+1-r\)，SIP 多项式 \(A(u,v),B(u,v)\) 单位 pixel，
\[
q=C_{\rm rad}\{u+(A,B)^T\},\quad
n={c+q_x e+q_y n_N\over\sqrt{1+q_x^2+q_y^2}}.
\tag{3}
\]
c 为 CRVAL 方向，e 为当地东向，n_N 为北向。CD 作用在 SIP **之后**；PC 形式是 `CD=diag(CDELT) PC`。CD 的负行列式合法，表达手性，不是负面积。上述切基对应标准 `LONPOLE=180°` 的 TAN；其他 celestial rotation 交由完整 WCSLIB 解码，不以式(3)覆盖未声明的头信息。

逆过程先 \(q=(n\cdot e,n\cdot n_N)/(n\cdot c)\)，`n·c<=0` 不属于该 TAN 图；再解 \(u+(A,B)=C^{-1}q\)，最后 \(p_0=u+r-1\)。AP/BP 只是可选逆多项式初值，不能用其误差替代真实残差。用有阻尼 Newton 和解析 Jacobian，按 detector pixel 残差停止；遇到非单射、奇异 Jacobian 或不收敛返回具名状态。对单个 Q，\(\sup_Q\|\nabla(A,B)\|_\infty<1\) 充分保证 SIP 全局单射；这是可编码的充分域，并非所有有效 SIP 的必要条件。未满足时不作“已证明 fold”的判断，必须增加单射证据或返回 `injectivity_unresolved`。

```text
sky_from_pixel(WcsModel, PixelCenter0|PixelEdge0) -> {SkyICRS,status}
pixel_from_sky(WcsModel, SkyICRS, residual_pixel_budget) -> {PixelCenter0,residual,status}
pixel_measure(WcsModel, DetectorRectangle, area_budget) -> AreaInterval[sr]
```

多项式次数 d、一个点的直接计算 O(d²)，Newton m 次 O(md²)；批量广播/缓存不改变坐标定义。精度预算分为头模型误差、坐标运算误差、逆求解残差；小 round-trip 只能说明内部自洽，不能发现同向的原点错误。

## 4. crop、bin、Bayer 与手性

统一记录 `p_old=M p_new+b`。坐标变换先作用到**连续像元及面积域**，随后调用旧 WCS；图像数值变换的算子另存，以便传播噪声。

| 操作 | M、b 或数值规则 |
|---|---|
| crop 左上 `(x_c,y_c)` | M=I，b=(x_c,y_c)，CRPIX_new=CRPIX_old−b |
| sum-bin `(b_x,b_y)` | M=diag(b_x,b_y)，b=((b_x−1)/2,(b_y−1)/2)+crop_offset；值是覆盖原像元之和 |
| mean-bin | 同一几何，值再除 b_x b_y，方差按平方除，单位说明为平均而非原积分 |
| flip_x | M=diag(−1,1)，b=(width_old−1,0) |
| flip_y | M=diag(1,−1)，b=(0,height_old−1) |
| transpose | M=[[0,1],[1,0]]，新 shape 交换 |
| Bayer 子格 `(a,b)` | `p_old=(2x+a,2y+b)`；采样间隔不等于实际收光区域变成 2×2 原像元 |

多次变换按矩阵复合，不交换顺序。新 CRPIX 满足
\(r_{new}=1+M^{-1}(r_{old}-1-b)\)，`CD_new=CD_old M`，
\(D_{new}(u)=M^{-1}D_{old}(M u)\)。这给出非对称 SIP 的完整基变量变换；只改 CD 不改 SIP 系数一般错误。Bayer/demosaic 是具有不同 response 与相关性的观测算子，不是无损的“换头”。

sum-bin 的真实收集核是各输入 P_i 之和；用大矩形 WCS 只描述覆盖，并不消除不同 PRF、缺像元或读噪次数。硬件 bin 与软件 bin 的读噪模型不同。

## 5. Export 的坐标与科学量闭环

给定输出 `(width,height)`，将指定中心放在内部 `((width−1)/2,(height−1)/2)`，对应 `CRPIX=((width+1)/2,(height+1)/2)`，奇偶尺寸都成立。指定方向/尺度建立 CD，不再在 Export 局部补偿上游错误。无支持、跨投影无定义域、不可计算位置输出 NaN；合法负值与有限零保留。

正式科学 Export 采用目标像元真实 WCS footprint \(E_i\) 与球面叶的面积交叠 \(b_{ip}=\Omega(E_i\cap H_p\cap U_p)\)，其中 \(U_p\) 是已知支持区域。球面场按叶内常量解释：
\[
 \bar B_i={\sum_p b_{ip}B_p\over\sum_p b_{ip}},\qquad
 F_{i,\rm supported}=\sum_p b_{ip}B_p .\tag{4}
\]
前者是**被支持区域的面亮度平均** [L/sr]；后者是被支持区域的积分 [L]，不自动填充缺覆盖部分。若要求整目标像元通量 `Ω(E_i) Bbar_i`，须明确假定未覆盖部分与已覆盖部分相同，另记外推。仅有全叶 signal、没有精确 subleaf support 时使用声明的 full-leaf/有效面积近似，不能由一个 support 比例恢复真实子叶形状。

`R_export` 对面亮度的行为是式(4)中的归一行，对支持积分是不归一面积行；协方差均按同一行传播。只拿到最终 signal 时不制造完整方差。显示/后期可以另选 nearest 或插值，但记录核及插值目标，不能冒充式(4)的面积积分。几何直接复用本目录的真实曲边交叠定义，将目标 WCS 像元作为 source footprint、HEALPix 叶作为 target，**不做第二次 pixfrac 收缩**。

FITS `BUNIT` 明确对应 L/sr 或 L；非标准模型单位不伪装成标准 SI 单位，附 reference SED、response 和归一说明。单个最终 signal HiPS/FITS 可以附表记录响应、mask 和内部算子身份，不新增虚构物理信号层。HiPS NESTED 的 sky index 与 tile 图像行列是两个对象：tile 宽 2^t 时 leaf_order=tile_order+t；NESTED x 占偶数位、y 占奇数位。采用 IVOA HiPS tile 朝向时明确 `row=W−1−x,col=y`，独立序列化方向验证不能由内部 writer-reader round-trip 代替。其HPX头的CD列分别为 `(-45/N,+45/N)` 与 `(-45/N,−45/N)` degree/pixel；CRPIX由所属tile的face线性offset反解，不套用居中TAN的CRPIX。赤道face6跨全局HPX经度±180中断时，用旋转180°的原生投影中心或合法分片，不能以数组±1补缝。

`export_independent.py` 已实际构造2×2偶数尺寸、CRPIX=(1.37,1.12)、镜像非对角CD及RA回绕，输入真实叶内−2/0/+3的分段常数。四百万独立detector点用外部完整WCS/成员与独立TAN Jacobian积分，对全/部分支持给Hoeffding联合置信界。部分支持像元覆盖约30.08%，正确支持均值−2，误用全像元分母得到−.6015。面积界逐项传播到式(4)，几何中点仅用于固定R的条件covariance实验；同叶重复引用先合并，否则拆.35/.65再假独立会将白噪声方差压到正确值的.545。该参考接口验证不是旧生产Export E2E，初始较紧预算失败与后续实际精度均在证据中说明。

独立 `hips_orientation.py` 由上述HPX头推导直接调用WCSLIB，另用外部HEALPix叶中心对照；12面、Nside=8/512/2¹⁸共3600点，0个无定义值，最大向量角差9.82e−14rad。初次未旋转face6的HPX头出现92个无定义点，失败及修正均保留。该检查证明声明数组/头/天空几何在这些点一致，尚不是第三方真实HiPS生产者互操作或旧writer E2E。

## 6. 坐标证据与使用范围

独立脚本 `coordinates_independent.py` 从式(3)及标准 API 构造真值，并编译未改动的相关生产 TU 探针。9027 个 TAN+2/3/4阶 SIP 测点含非整数 CRPIX、非对角镜像 CD、RA 回绕及双极区；解析向量与 Astropy 最大误差约 `3.63e−10 arcsec`。这仅证明指定数值构型，不是物理 WCS 精度。

IPV 迭代逆接口的 +1 已由 1003 点生产函数探针对照确认；详细位置、调用可达性与 detector 半像元疑点见[独立证据 §3](INDEPENDENT_ORACLES.md)。不把该接口错误扩大成“当前所有 CLI 星位都偏1像元”。crop/bin/flip/transpose 用复合 WCS 对拍，真实 FITS 读写与 HiPS 第三方生产者方向仍需单独验证。

HST coadd 只能作为已知有效 PSF 与分辨率的**条件场景**。若模拟目标仪器 PSF，须从 `P_HST * K=P_target` 求可实现的额外退化核；未证明该卷积关系时不能直接再卷一个“目标 PSF”并称无 PSF 宇宙真值。未知高频不可恢复，真实 M42、DCR、PSF 尾、滤镜角入射和亚像元 PRF 均须实测。

## 7. 一手依据

1. [FITS 4.0，§3.3.2、§8.1–8.2、式(9)–(10)](https://fits.gsfc.nasa.gov/standard40/fits_standard40aa-le.pdf)：整数数组索引与像元中心、WCS 级联；采用其一基外部定义。
2. [Greisen & Calabretta 2002，§2，DOI 10.1051/0004-6361:20021326](https://arxiv.org/abs/astro-ph/0207407)；[Calabretta & Greisen 2002，TAN 与天球旋转](https://arxiv.org/abs/astro-ph/0207413)：采用成熟 WCS，不自行发明天球坐标。
3. [SIP registered convention，§2–3、式(1)–(6)及§6](https://fits.gsfc.nasa.gov/registry/sip/SIP_distortion_v1_0.pdf)：CRPIX 相对基变量、先 SIP 后 CD和近似逆误差限制。
4. [Astropy WCS conventions](https://docs.astropy.org/en/stable/wcs/index.html)：完整 WCS 外部数值对照，不能用自写正逆互相证明。
5. [IVOA HiPS 1.0，§3–4](https://www.ivoa.net/documents/HiPS/20170519/REC-HIPS-1.0-20170519.pdf)：层级及 tile 序列化；不等价于科学面积算法已验证。
