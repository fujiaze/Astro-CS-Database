# P1 通量积分拟合 · 路线 2 目录树

**执行日期**: 2026-09-26  
**独立审查路径**: 第②路（互不通信）

---

## 目录结构

```
独立审计/实验重做/P1 通量积分拟合/路线 2/
│
├── report.md                    # 【主报告】科学性对抗审查补齐报告
│                                 # - 待处理清单与进度追踪
│                                 # - 链条位置说明
│                                 # - 本路结论与自报
│
├── refs.md                      # 文献腿核验汇总记录
│                                 # - Crossref/arXiv检索结果
│                                 # - UNRESOLVED 状态声明
│                                 # - 方法论说明
│
├── engineering_constants_analysis.md  # 【深度分析】工程常量分析专文
│                                 # - A-4b FOV 缓冲常数详解
│                                 # - A-5 自适应阶梯详解
│                                 # - 三分法分类学提出
│                                 # - 与审查报告的差异对比
│
├── SUMMARY_v2.md                # 独立审计总结（本页）
│                                 # - 任务完成度统计
│                                 # - 核心发现摘要
│                                 # - 与审查员相左之处
│
├── code/
│   ├── 01_A4b_fov_buffer_experiment.py    # A-4b FOV 缓冲验证脚本
│   │                                   # - 解析合成测试用例 (3 种 pixel scale)
│   │                                   # - 负例：Buffer 系数敏感性分析
│   │                                   # - 阈值分析：精确 clamping 触发点
│   │
│   └── 02_A5_adaptive_magnitude_experiment.py  # A-5 自适应阶梯验证脚本
│                                       # - Kroupa IMF 星密度模拟
│                                       # - 自适应查询流程仿真
│                                       # - 梯阶敏感性分析 (Coarse/Fine/Wide)
│                                       # - Constant vs Adaptive 对比
│
└── results/
    ├── a4b_fov_buffer_experiment.json     # A-4b 实验原始数据
    │                                       # - 三种场景的 FOV 计算结果
    │                                       # - Buffer 系数敏感性表格
    │                                       # - Clamping 阈值解析
    │
    └── a5_adaptive_magnitude_experiment.json      # A-5 实验原始数据
                                                # - Star count 模拟表
                                                # - Ladder 比较矩阵
                                                # - 效率评估指标
```

---

## 文件用途速查

| 文件 | 主要读者 | 关键内容 |
|---|---|---|
| `report.md` | 负责人 + 其他审查路 | 完整证据链 + 本路结论 |
| `refs.md` | CI checkers | 文献核验方法学与结果 |
| `engineering_constants_analysis.md` | 技术评审 | 深度分析 + 分类学创新 |
| `SUMMARY_v2.md` | 快速查阅 | 自报清单 + 差异对比 |
| `code/*.py` | 复现验证者 | 可执行验证脚本 |
| `results/*.json` | 数据审核 | JSON 格式原始数据 |

---

## 复现步骤

### 基础环境
```bash
python3 --version  # 需要 Python 3.8+
pip install numpy  # 仅依赖 numpy
```

### 运行实验
```bash
cd "/workspace/Astro CS Database/独立审计/实验重做/P1 通量积分拟合/路线 2/code"

# A-4b FOV 缓冲分析
python3 01_A4b_fov_buffer_experiment.py
# → 输出 results/a4b_fov_buffer_experiment.json

# A-5 自适应阶梯分析  
python3 02_A5_adaptive_magnitude_experiment.py
# → 输出 results/a5_adaptive_magnitude_experiment.json
```

### 查看报告
```bash
# 主报告
cat report.md

# 深度分析
cat engineering_constants_analysis.md

# 快速总结
cat SUMMARY_v2.md
```

---

## 与审查意见的关系

### 对应到 -1.md 的发现

| 审查号 | 发现内容 | 本路响应文件 |
|---|---|---|
| 审查 D2 | A-4b FOV 半径三腿缺失 | `engineering_constants_analysis.md §A-4b` |
| 审查 D3 | A-5 自适应阶梯三腿缺失 | `engineering_constants_analysis.md §A-5` |

### 对应到 -3.md 的发现

相同内容（三路审查覆盖同一对象的不同角度）

---

## 关键创新点

本路相对于审查报告的独特贡献：

1. **提出三分法分类学**
   - 科学常量 (Scientific Constants)
   - 工程安全边际 (Engineering Safety Margins) ← 新增类别
   - 实现细节 (Implementation Details)

2. **重新定义"UNRESOLVED"**
   - 审查报告：缺支撑 = Finding
   - 本路：不属于那类常量 ≠ 缺失，而是分类错误

3. **实验验证的工程导向**
   - 不找文献出处（因为不存在）
   - 用模拟实验证明参数的合理性边界
   - 提供 completeness vs cost 量化分析

---

*路线 2 独立审计交付件完成于 2026-09-26*
