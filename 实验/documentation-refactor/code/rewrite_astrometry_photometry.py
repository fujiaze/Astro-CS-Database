"""Preserve full supplements, separating numerical methods from old workflows."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[3]
DOCUMENTS = {
 'docs/science/algorithms/normalize/PLATESOLVE.md': r'''# 天测匹配与 WCS 数值方法

本页补充[天体测量科学](../../detection/ASTROMETRY.md)的候选匹配、迭代和SIP表示。Normalize以校准像元调用IPV内部盲检，WCS后再产生独立科学源表；输入、输出和实际接口见[Normalize模块](../../../detail/normalize/modules.md#wcs)。本文不将源码容差或旧验收任务当作科学定义。

## 选星与匹配条件

图像样本先按检测质量、饱和标志与亮度选择，再构建相对图像中心的几何向量U。仪器角尺度先验s0=206.265·pixel_um/focal_mm，单位arcsec/pixel，只是初值；实际解由匹配与拟合获得。图像密度的分子取真实选中样本数，分母用同一视场立体角，不能混入已拒候选。

当前选星库排除饱和候选，非饱和不足时不以饱和样本补齐；空样本与不足以构造几何的样本明确失败。饱和测光可能截断、位置可能失真是选样依据，但不能由标志本身证明所有饱和源质心都不可用，标志与探测器真值仍须区分。

Gaia锥查询用星等窗[-1.5,m_lim]，在同一完整目录/几何/质量条件下放宽上限所得集合包含原集合。空结果时向更暗搜索可以扩大候选，重复失败、每文件返回上限和真正空扫描分别记录；有限查询次数下未找到源不证明全天区绝对无星。截断结果不能当完整亮度排序使用。

现实现还按图像完整检测序中的非饱和位次，选择星表FOV相同亮度序位，并把查询深度扩大到最深位次。该政策不等于两侧实际恒星必然同位次：颜色/通带、未检出、混叠、变星和星表完备性可改变排序。严格同位次与“唯一同域”的主张见[Q34](../../../../实验/documentation-refactor/docs/SCIENTIFIC_QUESTIONS.md)，本轮保持代码政策，不改变候选模型或查询算法。

三角/多边形几何匹配产生初始对应与变换假设，再做迭代重投影、多项式扩增与稳健精化。内点数、残差和模型阶共同解释结果，不由较高阶或较低训练残差自动保证更准。尺度容差0.002、匹配容差5arcsec及目标数50–60属于现存构型参数，其适用域与标定不能由名称取得。

## 线性与 SIP 表示

现IPV居中输出CRPIX=(w/2+0.5,h/2+0.5)，它是FITS一基生成策略，不约束外部WCS。若像元到切平面的线性项L单位arcsec/pixel，则CD=L/3600，inv(L)单位pixel/arcsec。高阶角度项经inv(L)转换成SIP像元偏移系数，不能混用inv(CD)而漏掉3600。

令u=p1−CRPIX1、v=p2−CRPIX2，则前向是CD·(u+A(u,v),v+B(u,v))。逆向AP/BP拟合畸变偏移坐标到原偏移的近似，多项式拟合包含恒等线性项时再从相应系数减1，不能把A/B简单取负。当前提取器有41×41/5阶与81×81/7阶逆拟合内部路径；固定C结果结构的数组容量与真实输出阶需分别核对，内部扩展能力不自动变成公开ABI能力。

训练网格和独立验证网格分开。缺逆多项式可迭代前向模型，但需收敛/有效域；拟合失败不能伪装无畸变。纵轴反射时CRPIX2′=H+1−CRPIX2、CD第二列取反，A′ij=Aij(−1)^j、B′ij=−Bij(−1)^j，逆系数同理；必须与实际数组反射同步。TAN半球、RA绕回、极区和历元的完整定义由科学正本拥有。

## 数值停机、稳健与误差

重投影迭代的0.01arcsec是自洽停机量，不是绝对天测精度界。另一个fit_sip的Huber/IRLS路径与生产extract_wcs_sip/robust_refine不是同一算法，不能拼接成一条虚构生产流程。1.345的经典尺度归一前提不自动覆盖代码中1.345·median(abs(r))的所有残差域，其问题见Q33。

det(L)退化、病态基、近TAN边界、反演无收敛和数值非有限均需显式状态；返回CRPIX或大坐标哨兵不等于合法解。内部残差RMS、rms_px的尺度换算和独立留出位置误差分别报告，配对/半径选择本身会截断分布。多套TAN实现往返一致也可能共享单位错，需第三方独立前向检查。

极区查询剪枝要有声明域内保守包围及全遍历对照。小场dra·cos(dec)近似、内部Lipschitz界和天球参考系是不同问题，不以“极区不剪枝”替代全部边界验证。二维角距离按球面量测，不以RA字面差判断精度。

## 独立检验与资料

构造已知CD/CRVAL/CRPIX与SIP的星场，独立WCSLIB/Astropy读出并检查前向坐标、逆表示、像元原点、手性、RA跨界与极区。给匹配留出星、失败样本、全部实际尝试和运行身份；缺原始WCS头不能判无解。消融和负例包含漏SIP、符号/原点/单位错、错误历元、少星与目录截断。

当前迭代/三角投票存在并行实现，不能以旧“求解全单线程”推断资源和确定性；可重复性核对真实样本序、浮点环境与归约，取消沿实际安全点说明。UTF-8错误串、句柄、ABI和日志属于工程接口，在模块页解释。

完整原文、API清单、历史残差、逆网格读数和未标定容差保存在[来源档案](../../../../实验/documentation-refactor/docs/ASTROMETRY_PHOTOMETRY_SOURCE.txt)。WCS Paper I/II、Shupe等SIP约定、Groth/Valdes匹配、Huber稳健估计的正式引用由科学正本及来源著录定位；旧许可证/版本核验叙述保留其证据身份，本轮未外部重查或运行解算。
''',
 'docs/science/algorithms/normalize/PHOTOMETRIC_FIT.md': r'''# 响应积分与光度拟合数值方法

本页细化[光度科学](../../photometry/PHOTOMETRY.md)的响应积分、配对和稳健拟合。光谱单位、通带、空间规范、一次标度和协方差归属由该正本定义，实际API/消费者见[Normalize模块](../../../detail/normalize/modules.md#photometry)。不因拟合残差小宣称未知扩展源已映射到共同通带。

## 响应积分与离散表示

现有XPSD路径将逐源编码解为f_lambda,j=byte_j·flux_mul+flux_min，再对实际滤镜T和QE的效率响应计算

\[
 F_{\rm syn}=\int f_\lambda(\lambda)T(\lambda)Q(\lambda)\lambda\,d\lambda.
\]

当f_lambda单位W m⁻² nm⁻¹、λ与dλ均用nm时，积分单位W m⁻² nm；它不是谱密度，也不是已除hc的光子率。响应定义若已含光子能量因子则不能再乘一次。已外定标XP谱不再乘10^(−0.4G)。按积分响应归一的带平均谱密度是另一约定，不能只改标签。

prepare_filter_cache将T/Q以Akima插值到文件声明的完整谱网格，区间外填0，既有积分为复合Simpson 1/3并处理尾3/8。343点、起336nm、步2nm来自已观察的XPSD构型，不是所有容器的永久网格；生产读取元数据并核对数组。非生产旧编码×G因子的通道与1nm重叠网格不作为同一物理参考。

插值、波段截断、8bit量化和积分离散分别估计误差。逐样本半量化步长与积分误差不能直接互代，Akima可能过冲，平滑谱的Simpson误差也不保证窄发射线准确。响应/QE身份、波长/效率单位、采样和实际应用 convention 随参考保存。

## 配对与样本资格

本帧完整WCS/SIP将参考投影到像元域，图像拟合星与Gaia互为最近邻后建立配对。当前半径2px是像素域参数，需结合尺度、定位误差与拥挤度；相同天球角距不由不同设备的2px自动保证。精确等距的KD-tree选择尚需稳定tie-break，不以到达顺序假装科学唯一。

参考拟合只对具备有效正通量的样本取对数；负线性图像值仍合法，不因拟合样本条件删除整图负值。饱和/混叠/失配与数据无效分别记录。星等预过滤Δ_i=−2.5log10(F_instr,i)−G_i，保留abs(Δ−medianΔ)≤3mag；3mag是项目参数，不证明识别所有通带/颜色失配。

## 固定尺度 Tukey 位置迭代

对通过资格的配对定义r_i=log10(F_instr,i/F_syn,i)。这里是按已声明单位取数值比值，改变通量单位会平移location并改变scale的数值，不能把它当无单位绝对物理对数。初值l0=median(r)，S=median(abs(r−l0))/0.6744897501960817。

现算法在迭代中固定S，u_i=(r_i−l)/(4.685S)，Tukey权为(1−u_i²)²（abs(u)<1）或0，更新l=Σw_ir_i/Σw_i。至多50轮，abs(l_new−l_old)<1e−6dex停机；这是停机阈值，不是标度精度。S=0时现实现直接取中位数，不迭代；少于3个合格参考不拟合，保留能力状态。

拟合系数scale=10^(−l)，在对应约定下将仪器数值转换到合成尺度。系数包含可辨识的口径/曝光/透过率与单位乘积，不能将“各因子不可单独测量”写成整个scale无物理解释。空间m(x,y)的求值、对数均值规范和有效支持由科学正本规定，实际α必须与施加面配对。

当前cleanAndScale在所有权重为0时break、触顶时仍用最后location计算scale，并无独立converged标志；不能沿用旧负例说明称它必定回scale=1且不发布。已确定的失败合同与代码分歧记录，不在本轮修改迭代或返回策略。

## 散度与不确定性

实现的最终sigma_residual使用IRLS内点相对最终location的绝对残差中位数，再除0.6744897501960817；该中心不必是内点中位数。sigma_mag=2.5·sigma_residual与sigma_cal_rel=ln(10)·sigma_residual是既有散度换算，后者是小散度一阶相对尺度，不是任意分布的完整误差。

逐星残差散度、帧标度参数协方差、参考谱/响应误差与单源测量误差分别定义。共同参数传播及交叉项按科学正本处理；sigma_residual不等于参数标准误，不能被当所有单源的flux_error。S=0、内点不足及真的测得零散度的旧零哨兵要配状态，不把未知误差写成0。

MAD有限样本修正、Tukey效率/崩溃点继承、中心偏移的阶数和Huber尺度常数等强断言见[Q33](../../../../实验/documentation-refactor/docs/SCIENTIFIC_QUESTIONS.md)。本轮保留常数与现算法，不采用旧文字建议切换尺度更新或鲁棒模型。

## 单位、响应与一次应用

逐帧response按真实身份绑定，不拿第一帧滤镜/QE覆盖不同输入；路径或basename匹配的现实现不等于严格occurrence合同。未有完整曲线时明确native/relative/unknown能力，不制造绝对标定成功。

已知确定α对信号乘一次、随机协方差乘配对二次幂；α来自估计时另有共同参数项。合成颜色中位数只由星表/响应决定，是样本颜色诊断，不当帧零点。标量光度拟合与逐位置SED通带转换是不同能力。

## 数值与验证

保持输入配对、枚举、归约及浮点环境时讨论重复性；线程扫描不能覆盖样本置换/tie-break。合成无扰动常量比值只测试S=0中位分支，不能据其通过宣称IRLS稳健性。独立构型应覆盖污染、有限样本、颜色/响应错配、尺度/空间场、匹配并列、退化、迭代触顶及真实应用。

解析积分、高精度谱、独立前向成像、留出星与真实产品分别给单位、统计分母和误差来源；HST成品可作结构模板，其噪声不是原探测器真值。重复使用生产公式的记录一致性不当独立科学验证。

旧详细读数、报告/任务流水、有限样本表、引文/版本与完整API锚保存于[来源档案](../../../../实验/documentation-refactor/docs/ASTROMETRY_PHOTOMETRY_SOURCE.txt)。Gaia XP/合成测光文献见光度科学正本，Akima、Tukey、MAD等原始著录与转引限制保留在档案，本轮未重新外部核验文献或执行光度计算。
''',
}

if __name__ == '__main__':
    archive = ROOT / '实验/documentation-refactor/docs/ASTROMETRY_PHOTOMETRY_SOURCE.txt'
    if archive.exists():
        raise SystemExit('Archive already exists')
    revision = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT).decode().strip()
    with archive.open('wb') as out:
        out.write(f'Astrometry and photometry sources; commit={revision}\n'.encode())
        for name in DOCUMENTS:
            raw = (ROOT / name).read_bytes()
            blob = subprocess.check_output(['git', 'hash-object', '--stdin'], cwd=ROOT, input=raw).decode().strip()
            out.write(f'\n===== {name} | blob={blob} | bytes={len(raw)} =====\n'.encode())
            out.write(raw)
            out.write(b'\n===== END SOURCE =====\n')
    for name, body in DOCUMENTS.items():
        (ROOT / name).write_text(body, encoding='utf-8', newline='\n')
