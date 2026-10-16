"""Preserve detection research sources and separate methods from real API."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[3]
DOCUMENTS = {
 'docs/science/algorithms/normalize/STAR_DETECTION_ALGORITHMS.md': r'''# 图像盲检与引导拟合的数值方法

本页补充[检测科学](../../detection/STAR_DETECTION.md)的数值构造。Normalize先由IPV内部候选求WCS，再以Gaia引导独立science_sources；两者不共享候选职责。真实sdet入口、字段、所有权与调用见[检测模块](../../../detail/normalize/modules/star_detection.md)。本页不授权更换检测核、拟合模型或阈值。

## 噪声与阈值

图像单位继承输入，不能一概称原生ADU。行差分d=I_yx−I_y,x−1在局部平稳噪声下有Var(d)=2σ²(1−ρ_adj)。现存估计器逐行以median/MAD做至多3轮5σ裁剪，MAD系数1.482602218505602，取行标准差中位数再除√2。该程序与未裁剪总体方差恒等式不同，相关、有限样本、结构和选择均影响偏差；另一全帧裁剪RMS也不是同一估计量。

盲检平滑图S上的阈值取median(I)+5σ_raw，σ_raw来自未平滑图。独立同方差与固定核下，σ_sm=σ_raw√ΣK²；连续二维高斯的1/(2σ_s√π)只是近似，不能把实际递归核σ_s=2直接当解析高斯。实际核范数与引用域见检测正本。高斯星的连续峰值衰减可用σ_p²/(σ_p²+σ_s²)解释趋势，但不直接确定99%召回阈。

## 递归平滑

现存YvV族每维参数为

\[
 q=\begin{cases}3.97156-4.14554\sqrt{1-0.26891\sigma_s},&\sigma_s<2.5,\\
 0.98711\sigma_s-0.96330,&\sigma_s\ge2.5.\end{cases}
\]

原始系数b0=1.57825+2.44413q+1.4281q²+0.422205q³，r1=2.44413q+2.85619q²+1.26661q³，r2=−1.4281q²−1.26661q³，r3=0.422205q³；B=1−(r1+r2+r3)/b0，b_i=r_i/b0。前向w_n=Bx_n+Σ_{i=1}^3b_iw_{n−i}，反向同系数；当前边界用首/末值种子。σ≤0.5或宽高小于4直通，float/double分别计算。

q²的1.4281是当前实现值，旧文转述1.4251及“完整Triggs边界矩阵”的对应未统一。保持当前系数与边界，原文与冲激响应需后续核验；不能因名称相近自动换值或把简单末值种子当已实现全部论文边界方法。

## 候选、分裂和初值

阈值图按8邻接提取组分。现存去混叠构造采用30层指数阈值t_i=t_det(Smax/t_det)^(i/30)，相对父组分的流量比例δ_c=5e−3，至少两枝满足条件才分裂。枝流量、父流量是否扣阈底及重分配规则必须随定义给出；当前以双变量高斯argmax分配剩余像素、平局按登记序，不把它说成原论文逐项相同实现。

局部极大扫描以11×11邻域和确定平局规则选峰，半径5的边缘带另处理。一阶差分过零给中心初值；二阶差分首次零交叉给两侧距离，Sr=(d_left+d_right)/2、Sc类似，缺侧有现存回退。平滑幅度读数通过√e缩放，只有相应高斯假设下才有严格参数解释。

候选拟合盒R=max(ceil(√(−2ln0.001)Sr),ceil(√(−2ln0.001)Sc),5)，上限200且按边界收缩。该高斯能量半径与实际方形窗、Moffat翼或饱和窗不是相同通量份额。候选曼哈顿去重半径max(1,floor(0.2R))与分裂树并存，不能由分裂树存在推断近邻召回已经完整。

## 模型拟合、质量与输出

检测侧模型为B+Aexp(−½[(x'/σ_x)²+(y'/σ_y)²])。现参数化SX=2σ_x²、r=(cos(fr)+1)/2、σ_y=rσ_x，θ与内部alpha符号按真实adapter转换。FWHM=2√(2ln2)σ；不能与PSF库Moffat4同名宽度互换。halfA边界搜索和局部背景初始化后调用现有信赖域LM，既有迭代/容差不是全域误差证明。

饱和像素可从拟合样本排除，样本不足拒绝。平顶诊断使用dynrange=min(max(I),65535)−median(I)，0.7/0.1倍的邻域条件不等于探测器饱和真值；原生阈值、校准和实际标度另保留。饱和候选的某些质量门豁免按实现解释，不能推广到所有科学消费者。

拟合后质量检查包括FWHM正且大于0.5px、轴比、MAD残差/A（非饱和时阈0.2）和候选宽度相关上限。额外盲检形状门以帧候选FWHM中位参考、峰占比和四分之一峰像素数筛选；当前默认比例0.5/2.5、峰占比0.35、最少像素4，属于具体采样域政策，不适用未标定窄核或不同相位。引导路径不经过该额外门。

mag由实际方形窗扣拟合背景后的和计算，非正和得到NaN；API名flux当前记录拟合振幅A，二者不能混称总通量。输出拟合后去重、按mag稳定排序、NaN置尾和maxStars截断。Gaussian定位/粗量与后续Moffat积分PSF的测量身份分别保留。

## 统计与独立核验

盲检阈值表、相位、PSF、种子、注入/匹配数及虚警分母必须绑定。零失败二项试验的一侧下界、至少299的样本条件和择阈多重比较见检测正本；旧n≥298和“经验零失败格点必定是阈”不再作为正式定义。单个宽度上的非单调有限样本读数不能单独证明不同物理区制。

核验需要噪声/相关场、冲激核、边缘、不同宽度/相位、拥挤与不等通量双星、饱和/失配、纯噪声、坐标转换及并发输出。稳定排序不证明全浮点逐位相同，库间行为相似也不证明独立真值。记录的历史读数、召回阈表、谱系备选和逐算子原文完整保存在[检测来源](../../../../实验/documentation-refactor/docs/DETECTION_SOURCE.txt)；本轮未重跑这些实验。

## 参考资料

Young与van Vliet 1995的递归滤波、Triggs与Sdika 2006边界处理、Bertin与Arnouts 1996提取、Stetson 1987测光、Moré 1978及MINPACK非线性求解为原文参考范围，完整著录与转引限制在来源档案。论文方法、作者软件不同版本和项目参数分别归因，30/32层、argmax分配及强最优性等细节的外部核验仍待完成，不以旧“已核验”文字替代本轮阅读证据。
''',
 'docs/detail/normalize/modules/star_detection.md': r'''# 星检测接口与生产调用

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
''',
}

if __name__ == '__main__':
    archive = ROOT / '实验/documentation-refactor/docs/DETECTION_SOURCE.txt'
    if archive.exists():
        raise SystemExit('Archive already exists')
    revision = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT).decode().strip()
    with archive.open('wb') as out:
        out.write(f'Detection sources; commit={revision}\n'.encode())
        for name in DOCUMENTS:
            raw = (ROOT / name).read_bytes()
            blob = subprocess.check_output(['git', 'hash-object', '--stdin'], cwd=ROOT, input=raw).decode().strip()
            out.write(f'\n===== {name} | blob={blob} | bytes={len(raw)} =====\n'.encode())
            out.write(raw)
            out.write(b'\n===== END SOURCE =====\n')
    for name, body in DOCUMENTS.items():
        (ROOT / name).write_text(body, encoding='utf-8', newline='\n')
