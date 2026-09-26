#!/usr/bin/env python3
"""
幻觉锚汇总分析（第③路独立查证）

目的：汇总所有已确认的文档 - 代码不一致案例 (幻觉锚)
方法：综合前述实验发现 + 额外代码核验
单次运行时间：≤5 分钟 CPU
运行命令：python3 code/illusion_anchors_summary.py
结果落点：results/illusion_anchors_summary.json
"""

import json
from pathlib import Path

print("="*80)
print("幻觉锚汇总分析 — 文档 vs 代码一致性审计")
print("="*80)
print()

# ============================================================================
# 1. 已确认幻觉锚列表
# ============================================================================

print("【已确认幻觉锚完整列表】")
print("-"*80)

illusion_anchors = [
    {
        "id": "IA-001",
        "category": "A-4b FOV 半径三项常数",
        "claim_in_doc": "无条件钳位 min(max(fov, 1.0), 10.0)",
        "actual_code": "条件钳位:仅在 fov≤0 或 fov≥30 时钳位到 [1.0, 10.0]",
        "code_location": "lib/algorithms/photometry/cpp/src/frame_photometry_fit.cpp:167-173",
        "severity": "S2 级",
        "verified_by": "code/A4b_fov_radius_sensitivity.py",
        "test_case": "fov=0.5°时被实际代码保留 (0.5), 但文档声称应钳位到 1.0"
    },
    {
        "id": "IA-002",
        "category": "A-5 锥形搜索四项常数",
        "claim_in_doc": "自适应阶梯{12,13,14,15,16}、早停阈 2000、循环上限 5 有科学断言支撑",
        "actual_code": "工程经验参数，无文献/推导支撑",
        "code_location": "lib/algorithms/photometry/cpp/src/pc_api.cpp:978, 1001",
        "severity": "C5 S2 级",
        "verified_by": "code/A5_cone_search_constants.py",
        "test_case": "三腿状态均为缺失或部分存在 → 登记错误为'科学断言'"
    }
]

print(f"当前已确认幻觉锚数量：{len(illusion_anchors)}")
print()

for ia in illusion_anchors:
    print(f"--- {ia['id']} ---")
    print(f"类别：{ia['category']}")
    print(f"文档主张:{ia['claim_in_doc']}")
    print(f"实际代码:{ia['actual_code']}")
    print(f"位置:{ia['code_location']}")
    print(f"严重度:{ia['severity']}")
    print(f"验证脚本:{ia['verified_by']}")
    print(f"测试案例:{ia['test_case']}")
    print()

# ============================================================================
# 2. 额外核查其他潜在锚点
# ============================================================================

print("\n【额外核查其他潜在锚点】")
print("-"*80)

potential_hallucinations = []

# 核查 C-6 max_stars=5000
c6_findings = """
C-6 max_stars=5000:
- 文档主张："max_stars=5000 是 Gaia 查询最优阈值"
- 代码事实:"在 photometry 模块中未发现显式定义"
- 可能性:"可能存在于 gaia_client* 或其他配置文件中"
- 结论:"待进一步定位—暂不标记为幻觉锚"
"""

print(c6_findings)

# 核查 C-7 spatial_gain_order
c7_findings = """
C-7 spatial_gain_order≤2:
- 文档主张:"order>2 时信噪比不足，基于实测数据"
- 代码事实:"frame_photometry_fit.h:74 定义 order=0，使用处检查 order>0"
- 实测证据:"本实验证明 order3+ 时 SNR 衰减显著"
- 结论:"非幻觉——文档与实测一致，但三腿状态为部分支持"
"""

print(c7_findings)

# 核查 G-12/G-13 sigma_floor/sigma_ceiling
g12_g13_findings = """
G-12/G-13 sigma_floor/sigma_ceiling:
- 文档主张:"判据已在代码中生效"
- 代码事实:"photometry 模块中完全未找到相关实现"
- 覆盖范围:"仅 coverage 模块有类似机制，不属于 P1 链"
- 结论:"若文档声称'已生效',则属于 S2 级幻觉锚;否则为'待实现功能'"
"""

print(g12_g13_findings)

# ============================================================================
# 3. 幻觉锚分类统计
# ============================================================================

print("\n【幻觉锚分类统计】")
print("-"*80)

categories = {}
severity_counts = {"S1": 0, "S2": 0, "C5 S2": 0}

for ia in illusion_anchors:
    cat = ia["category"]
    categories[cat] = categories.get(cat, 0) + 1
    
    sev = ia["severity"]
    if sev in severity_counts:
        severity_counts[sev] += 1

print("按类别统计:")
for cat, count in categories.items():
    print(f"- {cat}: {count}")

print("\n按严重度统计:")
for sev, count in severity_counts.items():
    if count > 0:
        print(f"- {sev}: {count}")

# ============================================================================
# 4. 修正建议优先级排序
# ============================================================================

print("\n【修正建议优先级排序】")
print("-"*80)

correction_priorities = []

for ia in illusion_anchors:
    priority = {
        "anchor_id": ia["id"],
        "action": "订正 05 文档",
        "effort": "低 — 仅需删除/修改一行主张",
        "impact": "高 — 恢复文档可信度",
        "order": len(correction_priorities) + 1
    }
    correction_priorities.append(priority)
    
    print(f"{priority['order']}. {ia['id']} ({ia['category']})")
    print(f"   动作:{priority['action']}")
    print(f"   工作量:{priority['effort']}")
    print(f"   影响:{priority['impact']}")
    print()

# ============================================================================
# 5. 诚实边界与未决问题
# ============================================================================

print("\n【诚实边界与未决问题】")
print("-"*80)

unresolved_issues = [
    "G-12/G-13 是否属于幻觉锚取决于 05 文档的具体措辞",
    "C-6 max_stars=5000 需要进一步定位代码中的实际定义",
    "可能存在更多未发现的幻觉锚点（需全项目 grep 扫描）",
    "幻觉锚的分类标准是否充分？是否需要细化？"
]

for i, issue in enumerate(unresolved_issues, 1):
    print(f"{i}. {issue}")

# ============================================================================
# 6. 最终结论与建议处置方案
# ============================================================================

print("\n【幻觉锚分析最终结论】")
print("-"*80)

final_conclusion = """
幻觉锚汇总分析结论:

已确认案例:
1. IA-001 (A-4b FOV 半径三项):S2 级幻觉——"无条件钳位"vs"条件钳位"
2. IA-002 (A-5 锥形搜索四项):C5 S2 级幻觉——"科学断言"vs"经验参数"

待确认案例:
1. G-12/G-13 sigma_floor/sigma_ceiling:取决于文档措辞

性质判断:
- 所有已确认案例均属"文档夸大/误述"类型，非"根本性设计错误"
- 实际代码逻辑在工程上基本合理，只是文档描述不准确
- **修复策略应以订正文档为主，而非改代码**

建议处置方案:
1. 立即订正 05 文档:
   - A-4b:将"无条件钳位"改为"条件钳位 (只在 fov≤0 或≥30 时)"
   - A-5:将"科学断言"改为"经验参数，待标定"
   
2. 补充三腿标注:
   - 对所有常数值添加四件套完整性标注 (文献/实验/推导)
   - 无法补全的明确标记为"项目约定值"
   
3. 建立幻觉锚预防机制:
   - PR 模板强制要求"文档 - 代码一致性自证"
   - CI 增加幻觉锚检查规则 (doc_impl_consistency_check)

链条位置影响:
- 幻觉锚虽不影响功能正确性，但损害文档权威性与可维护性
- 长期累积将导致后续开发成本剧增
- **建议本次集中清理作为 α版本质量门之一**
"""

print(final_conclusion)

# ============================================================================
# 7. 保存 JSON 结果
# ============================================================================

results = {
    "experiment_id": "illusion_anchors_summary",
    "route": "独立审计路线 3",
    "review_claim_id": "综述类任务",
    "summary": {
        "total_verified_anchoors": len(illusion_anchors),
        "categories": categories,
        "severity_distribution": severity_counts
    },
    "verified_anchors": illusion_anchors,
    "unresolved_issues": unresolved_issues,
    "recommendations": {
        "immediate_fixes": [
            "订正 A-4b'无条件钳位'为'条件钳位'",
            "订正 A-5'科学断言'为'经验参数'",
            "核实 G-12/G-13 文档措辞并判定是否为幻觉锚"
        ],
        "long_term_prevention": [
            "PR 模板增加'文档 - 代码一致性自证'要求",
            "CI 增加幻觉锚检查器 (docs/code_consistency_check)",
            "对常数值强制要求四件套完整性标注"
        ]
    },
    "conclusion": final_conclusion.strip(),
    "references": [
        {"type": "experiment", "file": "code/A4b_fov_radius_sensitivity.py"},
        {"type": "experiment", "file": "code/A5_cone_search_constants.py"},
        {"type": "experiment", "file": "code/C6_max_stars_5000_analysis.py"},
        {"type": "experiment", "file": "code/C7_spatial_gain_order_analysis.py"}
    ]
}

output_dir = Path(__file__).parent / "results"
output_dir.mkdir(exist_ok=True)
result_file = output_dir / "illusion_anchors_summary.json"

with open(result_file, "w", encoding="utf-8") as f:
    json.dump(results, f, ensure_ascii=False, indent=2)

print("\n" + "="*80)
print(f"结果已保存至：{result_file}")
print("="*80)
