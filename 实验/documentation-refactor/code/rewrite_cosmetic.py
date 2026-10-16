"""Preserve cosmetic source and separate algorithms from ABI/history."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[3]
DOCUMENTS = {
 'docs/science/algorithms/normalize/COSMETIC_ALGORITHMS.md': r'''# 坏点与坏列的离散检测和修复

本页细化[CCD/CMOS缺陷科学](../../calibration/CCD_DEFECT.md)的数值步骤；物理分类、信息量与协方差定义以该正本和[校准科学](../../calibration/CALIBRATION.md)为准。实际入口、掩膜、精度与消费者见[Normalize模块](../../../detail/normalize/modules.md#cosmetic)。像素值继承实际校准标度，不能由输入文件类型一概称原生ADU。

## 母版坏点候选

热候选由dark面、冷候选由bias面各自估计med=median(src)、MAD=median(abs(src−med))、σ=1.482602218505602MAD。分别按src>med+hot_sigma·σ和src<med−cold_sigma·σ严格比较；sigma倍数无量纲。统计量和信号作相同正仿射变换时，精确算术下此类候选不变，有限精度、溢出和量化另核对。

当前中位数的奇数样本取中央值，偶数取两中央值均值，这是一种明确约定，不是相对“真中位数”的缺陷。实现采用nth_element再取下半最大值，全局MAD另建绝对偏差数组；不把它误述成绝对偏差求和。

MAD=0时阈值退化到中位数，偏离中位的值可能全部被标。非有限输入又影响选择比较，不能断言任意NaN注入都必然得到同一个全false掩膜；检测的能力状态、零检出和关闭必须分开。非零MAD也不单独证明源面无结构或误报率已标定。

候选按8邻接分量标号，size≥max_size的分量被清除，即只保留较小结构；max_size≤0清除全部候选。它不证明大结构都是天体、小结构都是固定坏点，分类仍需母版/探测器信息。背景label=0单独跳过。

## 坏点插值

bad_mask中非零表示需修复，与排异accepted的极性不同。好像素复制原data，坏像素读取原data的好邻域，输出不作为后续插值输入；没有可用邻域时保留原值，不能称修复成功。

method=0使用5×5好像素中位数，边界先镜像再钳位；小帧可能重复采样，重复权重会改变中位数。其余库内method值走四正交方向最近好像素，权w_d=1/dist_d，再算Σw_dx_d/Σw_d。现枚举名BILINEAR并不使该公式成为通用双线性插值。四方向均不可用时也保留原值。

固定输入、掩膜和邻域时，这些程序具有明确复算规则；中位数是非线性选择，不能把它直接当固定线性J。IDW在固定掩膜和索引时可以形成线性映射，但其对天体真值的偏差仍取决于结构。方向性、长游程、图像边缘和星核要分别验证。

## 列段检测与修复

列统计c_x=median_y I_yx，差分d_x=c_x−c_{x−1}，尺度σ_d=1.482602218505602MAD(d)。当前按abs(d−median(d))≥column_sigma·σ_d取跳变，以边界形成段，短段修复，宽段只标记；与坏点的严格大于/小于不是相同边界约定。neighbor_k是现存保留参数，真实段长由max_seg_len决定。

对段[a,b]，锚L=a−1、R=b+1都可用时，按原data线性插值ŷ_x=x_L+(x_R−x_L)(x−L)/(R−L)。单侧锚复制并带EDGE_ONE_SIDED，无锚保留原值并带NO_ANCHOR；mask的干净、已修、仅标记分别保留，不能把计数合成“全部已修”。

science/dark/bias三路使用各自统计源形成并集并记录source_mask。dark+bias、单一母版、science-only按现政策分层置信标签；标签本身不是经过概率标定的置信度。宽候选预算可能抑制某路标记，抑制数和来源要保留，不能将丢弃后零标记当无缺陷证据。

## 方差与修复信息

固定线性修复的正确传播形式为J C Jᵀ，详见缺陷正本；与原好像元的交叉协方差不能消失。当前column_variance_inflate计算(w_L²+w_R²)·var_in[修复位置]·κ，单侧取w²=1。它没有读取两锚点各自方差和相关项，因此与w_L²V_L+w_R²V_R只在额外同方差等条件下对应，不能命名为完整传播。

κ由调用者配置，既不自动补回所有相关，也不保证对任意下游估计保守；未知误差仍应有模型缺项。插值的噪声传播和缺失位置的真值预测误差分别报告。需要重推导的边界及统计量见[Q36](../../../../实验/documentation-refactor/docs/SCIENTIFIC_QUESTIONS.md)，本轮不改插值、标记策略或方差函数。

## 数值、复杂度与验证

当前坏点和坏列基本计算为FP32，包括名为f64的部分转接入口；均值中间求和可溢出，精确实数不变量不自动是逐位保证。检测/连通标号需要O(N)存储，中位窗每坏点至多25样本；方向搜索成本还依坏点游程，不能无条件称全部IDW为O(N)。非重叠输出的像素/行可并行，调用者预算、取消与ABI另按真实边界核对。

独立测试应区分开启无缺陷、关闭、已检出未修、MAD退化、宽段抑制、NaN/Inf、小帧镜像、恒等、斜向/长段、单侧/无锚、异方差及修复相关。检测计数、实际改值数和物理坏点真值是三个量；同公式oracle只证明实现一致性，不能证明物理分类和误差充分。

旧逐符号/状态表、退役cc_*对照、全部历史读数、测试建议与参考文献保存于[来源档案](../../../../实验/documentation-refactor/docs/COSMETIC_SOURCE.txt)。原参考含MAD、连通域和一般插值资料；宇宙线算法只作邻近背景，不冒充母版坏点检测来源。本轮未运行缺陷探针或更换算法。
''',
}

if __name__ == '__main__':
    archive = ROOT / '实验/documentation-refactor/docs/COSMETIC_SOURCE.txt'
    if archive.exists():
        raise SystemExit('Archive already exists')
    revision = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT).decode().strip()
    with archive.open('wb') as out:
        out.write(f'Cosmetic sources; commit={revision}\n'.encode())
        for name in DOCUMENTS:
            raw = (ROOT / name).read_bytes()
            blob = subprocess.check_output(['git', 'hash-object', '--stdin'], cwd=ROOT, input=raw).decode().strip()
            out.write(f'\n===== {name} | blob={blob} | bytes={len(raw)} =====\n'.encode())
            out.write(raw)
            out.write(b'\n===== END SOURCE =====\n')
    for name, body in DOCUMENTS.items():
        (ROOT / name).write_text(body, encoding='utf-8', newline='\n')
