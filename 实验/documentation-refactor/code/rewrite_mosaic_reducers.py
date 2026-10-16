"""Keep numerical method supplements separate from API and old audit records."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[3]
DOCUMENTS = {
 'docs/science/algorithms/mosaic/PHASE2_INTEGRATION.md': r'''# 接受样本集成的数值方法

本页细化[集成科学](../../integration/INTEGRATION.md)的数值归约。目标量、统计假设和六态语义以该正本为准；接口与真实消费者见[Mosaic模块](../../../detail/mosaic/modules.md#integrate)。本页不另立ADU固定单位、权重模式或产品发布合同。

## 顺序归约

对同一输出位置的候选值 x_i、外部非负有限权重 w_i、支撑 s_i 和接受掩膜，先确定接受且输入合法的集合 E，再取 P={i∈E:w_i>0}。固定候选顺序累加

\[
 v_s=\sum_{i\in P}w_ix_i,\qquad W=\sum_{i\in P}w_i,
 \qquad \widehat B=v_s/W.
\]

x 与输出有相同实际面亮度单位 L/sr；w解释为同估计量逆方差时单位sr²/L²。reducer仅消费数值数组，不认证物理含义。相同位置的公共正权重因子可从归一均值消去，但W的物理单位与含义随之变化，不能把任何数值权重和都称精确逆方差。

支撑独立计算为 max_{i∈E}s_i，更新必须覆盖接受的零权样本；它不作为均值第二次除数。只有s_i确为覆盖分数时，max才有覆盖并集下界的解释，不能当真实并集。缺省数组、错误和状态的具体C接口见模块页。

## 浮点与退化

乘积、分子、分母与商均可能溢出，有限输入不足以保证有限输出。没有正权贡献不执行除法，算术失败与输入非法分开。六态为OK、NO_CANDIDATES、ALL_REJECTED、ZERO_VALID_WEIGHT、INVALID_INPUT、NON_FINITE_OUTPUT；函数rc成功不能替代status判断。

固定求和次序有助同构型重复，但编译器、FMA、舍入、dtype和调用者预处理仍影响数值。reducer自身不建线程，像素间并行由调用者管理；不能据无锁称其不支持独立输入输出的并发调用，也不能据像素内串行宣称整条生产链跨worker逐位相同。

常量场保持是精确实数性质，有限精度下还须评估乘积和相消误差；对所有权重/常数的bitwise承诺没有由该恒等式自动获得。容差和适用域见[测试规范](../../../engineering/testing/TEST.md)，重推导问题见[Q27、Q31](../../../../实验/documentation-refactor/docs/SCIENTIFIC_QUESTIONS.md)。本轮不改累加算法或现有数值阈值。

## 判别性验证

非常量例x=(1,2,4)、w=(1,1,2)给均值2.75；w=(1,1,0)给1.5，能检验权重被替换。常量场本身不能证明权重正确。支撑例s=(0.2,0.3,0.9)、w=(1,1,0)且全部接受，应保留support=0.9，与signal分开判断。

覆盖空栈、全拒、零权、非法接受值、已拒非法值、负信号、合法零及有限大值溢出；状态、计数和消费者无效值转换均核对。统计效率另用独立帧/共享误差/排异实验检验，不能由上述代数例推出最优性。

旧逐符号清单、五态表、行数、模式/冻结任务和历史读数完整保存在[来源档案](../../../../实验/documentation-refactor/docs/MOSAIC_REDUCERS_SOURCE.txt)。当前实际六态与内核行为已按源码核对，旧stage2/ACR路径未作为生产证据。经典参考及其适用解释统一见集成科学正本，本轮未运行核验程序。
''',
 'docs/science/algorithms/mosaic/UPM_SOLVER.md': r'''# 背景控制权重与局部 M+C 求解补充

本页说明现有UPM库的背景控制权重和M+C数值模型，科学目标与规范由[天空模型](../../sky/UPM.md)拥有。当前mosaic生产拟合/施加走p2_sky_plane_build_joint及delta接口，不能把本页旧M+C内核自动等同生产扣除链；入口定位见[Mosaic模块](../../../detail/mosaic/modules.md#upm-fit)。

## 控制估计量与权重

背景控制观测z_kc、其标准差σ_kc和quality分别给统计测量与质量信息。库的control-ivar路径取

\[
 w_{kc}^{\rm raw}=q_{kc}\,\operatorname{control\_ivar}_{kc},
 \qquad \operatorname{control\_ivar}=1/V(z_{kc}).
\]

控制中位数的既有近似为V=k_corr·(π/2)·σ_bg²/N_retained；iid高斯的渐近式、Drizzle相关和污染适用域分别解释，不能将点源SNR或单像元逆方差直接当背景控制精度。值和权重的实际单位由输入标度决定，不固定为原生ADU。

库另有use_ivar_weight=0的诊断式q·support^p·snr²/(1+snr²)/max(abs(uncertainty),sigma_floor)²，必须保留诊断身份，不重新变成产品权重模式。当前control-ivar路径遇非正/非有限civ明确返回错误。

每控制格的归一化采用

\[
 \widetilde w_{kc}=\frac{w_{kc}^{\rm raw}}{\sum_j w_{jc}^{\rm raw}}
                    \operatorname{control\_reliability}_c.
\]

这是无量纲格内份额，与原始绝对精度不是一个量。归一化改变不同控制格的相对贡献，不能因为和为reliability就宣称仍在求原绝对逆方差目标；科学解释与误差需单独论证。

## M+C与稳健迭代

局部模型z_kc≈M_c+C_kc，r=z−M−C，z_std=r/σ_eff，σ_eff=max(abs(uncertainty),sigma_floor)。Huber损失为小残差区z_std²/2、大残差区δ(abs(z_std)−δ/2)，IRLS因子分别为1与δ/abs(z_std)，既有δ=1.345无量纲。实际权重、尺度、平滑和弱锚共同决定目标，不能仅由Huber名字推出全问题最优或收敛保证。

内核按控制/帧图固定分量规范，在公共控制场和逐帧场间迭代，帧内低维系数可经CG更新，再按需要物化稠密缓存。既有弱零锚默认1e−3，选择规范与抑制不稳定不等于恢复绝对天空，也不由这个数值直接推出偏差低于0.1%。单帧区的延拓是所选模型约定，不产生独立观测信息。

M+C求值和生产raw−δ_k分开；当前生产δ相对公共连续信号面定义，不能用旧C双线性求值接口替代。同名端口或历史模型文件不是算法仍在生产的证明。

## 秩、收敛、并发和精度

有效边决定连通分量，参考帧身份和frame-index映射必须一致。构建返回成功与迭代达到容差分别报告，p2_upm_convergence提供后者，耗尽不能当已收敛。不同帧/控制的并行可改变聚合结合次序，同配置与跨worker比较分开；线程预算由宿主传入，源码存在调用内线程池不等于已经满足全部统一资源纪律。

稠密与稀疏互读是同一模型表示的一致性测试，不是独立科学真值。旧1e−12与单次误差/耗时属于相应构型，不能推广成无量纲全域误差保证。k_corr≥1及拒绝1的现有代码政策、有限样本中位数和负相关适用域的疑点见[Q31](../../../../实验/documentation-refactor/docs/SCIENTIFIC_QUESTIONS.md)，本轮不改控制方差算法、阈值或扣除模型。

独立验证应覆盖有效图断连、规范、已知加性场、真实低频保留、有限样本与相关控制统计、污染、未收敛和空观测，完整生产背景链还需其自身入口与反例。旧实测表、测试ID、逐符号锚和文献完整保存于[来源档案](../../../../实验/documentation-refactor/docs/MOSAIC_REDUCERS_SOURCE.txt)，本轮仅阅读源码与文档。
''',
}

if __name__ == '__main__':
    archive = ROOT / '实验/documentation-refactor/docs/MOSAIC_REDUCERS_SOURCE.txt'
    if archive.exists():
        raise SystemExit('Archive already exists')
    revision = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT).decode().strip()
    with archive.open('wb') as out:
        out.write(f'Mosaic reducer sources; commit={revision}\n'.encode())
        for name in DOCUMENTS:
            raw = (ROOT / name).read_bytes()
            blob = subprocess.check_output(['git', 'hash-object', '--stdin'], cwd=ROOT, input=raw).decode().strip()
            out.write(f'\n===== {name} | blob={blob} | bytes={len(raw)} =====\n'.encode())
            out.write(raw)
            out.write(b'\n===== END SOURCE =====\n')
    for name, body in DOCUMENTS.items():
        (ROOT / name).write_text(body, encoding='utf-8', newline='\n')
