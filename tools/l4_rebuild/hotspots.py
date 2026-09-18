#!/usr/bin/env python3
"""L4 热点分析：读辅助计时器与仓库自带遥测，排序热点并指出单线程瓶颈。

只做诊断，不改科学参数。无 perf 时以 wall/CPU 利用率/worker 均衡为代理指标。
"""
import csv, json, os, sys, glob

L4 = 'run/RELEASE-02/L4-rebuild'

def load_timings():
    p = os.path.join(L4, 'timings.csv')
    if not os.path.exists(p): return []
    return list(csv.DictReader(open(p, encoding='utf-8')))

def load_telemetry(tag):
    out = {}
    for f in ('resource_summary.json', 'worker_balance.csv', 'alloc_report.json'):
        p = os.path.join(L4, 'logs', tag + '.' + f)
        if os.path.exists(p):
            try:
                out[f] = json.load(open(p, encoding='utf-8')) if f.endswith('.json') else open(p, encoding='utf-8').read()
            except Exception as e: out[f] = 'unreadable: %s' % e
    return out

def main():
    rows = load_timings()
    if not rows:
        print('no timings yet'); return 2
    tot = 0.0; ranked = []
    for r in rows:
        try: w = float(r.get('wall_s') or 0)
        except ValueError: w = 0.0
        tot += w
        u = r.get('user_s') or ''; s = r.get('sys_s') or ''
        cpu = None
        try:
            if u and s and w > 0: cpu = (float(u) + float(s)) / w
        except ValueError: pass
        ranked.append((w, r.get('step'), r.get('rc'), cpu, r.get('maxrss_kb')))
    ranked.sort(reverse=True)
    print('=== 热点排序（按 wall time）===')
    print('%-26s %10s %5s %8s %12s' % ('step', 'wall_s', 'rc', 'cpu_util', 'maxrss_kb'))
    for w, step, rc, cpu, rss in ranked:
        cu = ('%.2f' % cpu) if cpu is not None else 'n/a'
        print('%-26s %10.1f %5s %8s %12s' % (step, w, rc, cu, rss or 'n/a'))
    print('TOTAL wall_s = %.1f' % tot)
    print()
    print('=== 单线程嫌疑（cpu_util 明显 < 1.0 的步骤）===')
    sus = [(w, st, cu) for w, st, rc, cu, _ in ranked if cu is not None and cu < 1.0]
    if not sus: print('  (none flagged; 若 cpu_util 缺失请检查 /usr/bin/time 是否可用)')
    for w, st, cu in sus:
        print('  %-26s wall=%.1fs cpu_util=%.2f  <-- 疑单线程/串行瓶颈' % (st, w, cu))
    print()
    print('=== 已知嫌疑点（HUB-A 上报）===')
    print('  n=2 逐像素 31x31 邻域中位数/MAD：粗估 ~1000-1200s（单线程）')
    print('  -> 若 mosaic 步骤 cpu_util 低且 wall 高，即确认；优化方向须不改变科学语义')
    print()
    print('=== 遥测文件位置 ===')
    for p in sorted(glob.glob(os.path.join(L4, 'logs', '*.resource_summary.json')))[:3]:
        print(' ', p)
    return 0

if __name__ == '__main__':
    sys.exit(main())