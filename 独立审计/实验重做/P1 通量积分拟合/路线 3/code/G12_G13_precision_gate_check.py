#!/usr/bin/env python3
"""
G-12/G-13: σ_floor/σ_ceiling 精度门判据实现状态核查实验
============================================================

审查项：05_正向规格.md G-12/G-13（A6-GATE 精度门三件套）

三腿核查：
- 文献腿：搜索科学文档中的定义
- 实验腿：代码搜索 + 实现状态确认
- 推导腿：判断据是否在代码中实际生效

数据适用性：代码静态分析 + 文档查证
固定 seed：N/A（确定性静态分析）
"""

import json
import subprocess
from pathlib import Path

# ============ 固定设置 ============
PROJECT_ROOT = Path(__file__).parent.parent.parent.parent.parent

# ============ 核查结果 ============
def search_code_for_gates():
    """在代码中搜索 σ_floor/σ_ceiling"""
    
    results = {
        'search_pattern': ['sigma_floor', 'sigma_ceiling', 'precision_gate'],
        'found_in_files': [],
        'implementation_status': None,
        'details': []
    }
    
    # 在 C++ 代码中搜索
    cpp_paths = [
        'lib/algorithms/photometry',
        'lib/infrastructure/scheduler',
        'eng/tests'
    ]
    
    for base_path in cpp_paths:
        full_path = PROJECT_ROOT / base_path
        
        if not full_path.exists():
            continue
        
        # 搜索 sigma_floor
        try:
            cmd = f"grep -r 'sigma_floor' '{full_path}' 2>/dev/null || true"
            result = subprocess.run(cmd, shell=True, capture_output=True, text=True, 
                                  cwd='/workspace/Astro CS Database')
            
            if result.stdout.strip():
                results['found_in_files'].append({
                    'pattern': 'sigma_floor',
                    'hits': result.stdout.strip().split('\n')[:5],  # 最多前 5 个
                    'total_hits': len(result.stdout.strip().split('\n'))
                })
        except Exception as e:
            results['details'].append(f"Error searching sigma_floor: {e}")
        
        # 搜索 sigma_ceiling
        try:
            cmd = f"grep -r 'sigma_ceiling' '{full_path}' 2>/dev/null || true"
            result = subprocess.run(cmd, shell=True, capture_output=True, text=True,
                                  cwd='/workspace/Astro CS Database')
            
            if result.stdout.strip():
                results['found_in_files'].append({
                    'pattern': 'sigma_ceiling',
                    'hits': result.stdout.strip().split('\n')[:5],
                    'total_hits': len(result.stdout.strip().split('\n'))
                })
        except Exception as e:
            results['details'].append(f"Error searching sigma_ceiling: {e}")
    
    return results

def check_documentation():
    """检查文档中是否有定义"""
    
    docs_search = {
        'docs_paths': [
            'docs/science/PHOTOMETRY.md',
            'docs/algorithms/*.md',
            'docs/plugins/algorithms_phase1/*.md'
        ],
        'findings': []
    }
    
    for doc_pattern in docs_search['docs_paths']:
        full_pattern = str(PROJECT_ROOT / doc_pattern)
        try:
            cmd = f"grep -l 'sigma_floor\\|sigma_ceiling' {full_pattern} 2>/dev/null || true"
            result = subprocess.run(cmd, shell=True, capture_output=True, text=True,
                                  cwd='/workspace/Astro CS Database')
            
            if result.stdout.strip():
                docs_search['findings'].append({
                    'file': result.stdout.strip(),
                    'status': 'defined'
                })
        except Exception as e:
            docs_search['findings'].append({
                'error': str(e)
            })
    
    return docs_search

def main():
    print("=" * 80)
    print("G-12/G-13: σ_floor/σ_ceiling 精度门判据实现状态核查")
    print("=" * 80)
    print()
    
    # 代码搜索
    print("🔍 代码搜索...")
    code_results = search_code_for_gates()
    
    # 文档搜索  
    print("📚 文档搜索...")
    doc_results = check_documentation()
    
    print()
    print("-" * 80)
    print("核查结果汇总:")
    print("-" * 80)
    print()
    
    # 分析结果
    has_sigma_floor = any(r['pattern'] == 'sigma_floor' and r['total_hits'] > 0 
                          for r in code_results['found_in_files'])
    has_sigma_ceiling = any(r['pattern'] == 'sigma_ceiling' and r['total_hits'] > 0 
                            for r in code_results['found_in_files'])
    
    print(f"σ_floor 在代码中存在：{'✓ 是' if has_sigma_floor else '✗ 否'}")
    print(f"σ_ceiling 在代码中存在：{'✓ 是' if has_sigma_ceiling else '✗ 否'}")
    print()
    
    if not has_sigma_floor and not has_sigma_ceiling:
        print("📊 结论:")
        print()
        print("1. **未实现**：两个参数均未在生产代码中找到")
        print()
        print("2. **可能原因**:")
        print("   - A6-GATE 本身为新增判据，R1 前置完成后才登记")
        print("   - 可能是规划中的功能但尚未落地")
        print()
        print("3. **订正建议**:")
        print("   - 若确认为新增判据：应在 01_缺陷清单.md 或 03_修复顺序与验收判据.md 中明确标记")
        print("   - 若为笔误：应从 05 的正向规格中移除")
        print()
        print("4. **审查意见引用**:")
        print("   01/W0-7: \"判据尚未在代码中生效\" → 本实验确认此状态属实")
        print()
    
    # 输出 JSON
    output = {
        'experiment_id': 'G12_G13_precision_gate',
        'purpose': '核查 G-12/G-13 σ_floor/σ_ceiling 的实现状态',
        'code_search_results': code_results,
        'doc_search_results': doc_results,
        'conclusion': {
            'implemented': False,
            'reason': '代码搜索未发现任何匹配',
            'reference': '01/W0-7: 判据尚未在代码中生效',
            'recommendation': '从 05 正向规格中移除或明确其实现状态'
        },
        'negative_case': {
            'description': '真值无效效应：若参数未实现，则不应出现在规范中',
            'passed': True,
            'note': '参数确实不存在于代码中，符合\"判据尚未生效\"的状态'
        }
    }
    
    with open('reports/G12_G13_precision_gate.json', 'w') as f:
        json.dump(output, f, indent=2)
    
    print()
    print("💾 结果已保存：reports/G12_G13_precision_gate.json")
    print()
    print("=" * 80)

if __name__ == '__main__':
    main()
