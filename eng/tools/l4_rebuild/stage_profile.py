#!/usr/bin/env python3
"""Phase1 逐阶段性能剖析：把事件流(NODE_START/NODE_END)与 resource_timeseries.csv 按时间对齐。

产出：每个阶段的 wall / 占比 / CPU(单核%) / CPU(全机%) / RSS 峰值 / io_wait / workers。
只读观测，不改科学参数。

用法: stage_profile.py <events.jsonl> [resource_timeseries.csv]
"""
import csv, json, os, sys, datetime

def parse_ts(s):
    # utc_now_ms 形式，如 2026-09-18T20:01:02.123Z
    s = s.strip().replace('Z', '')
    for fmt in ('%Y-%m-%dT%H:%M:%S.%f', '%Y-%m-%dT%H:%M:%S'):
        try: return datetime.datetime.strptime(s, fmt).timestamp()
        except ValueError: pass
    return None

def load_events(p):
    evs = []
    for line in open(p, encoding='utf-8', errors='replace'):
        line = line.strip()
        if not line.startswith('{'): continue
        try: j = json.loads(line)
        except Exception: continue
        if 'node_id' not in j or 'type' not in j: continue
        evs.append(j)
    return evs

def stages_from_events(evs):
    starts, out = {}, []
    for j in evs:
        nid, ty = j.get('node_id'), j.get('type')
        t = parse_ts(j.get('ts_utc',''))
        if ty == 'NODE_START':
            starts[nid] = (t, j.get('module_id'))
        elif ty == 'NODE_END':
            st = starts.get(nid)
            wall = j.get('wall_ms')
            out.append({'node': nid, 'module': (st[1] if st else j.get('module_id')),
                        'start': (st[0] if st else None), 'end': t,
                        'wall_ms': float(wall) if wall is not None else None,
                        'status': j.get('status'), 'workers': j.get('workers'),
                        'granted': j.get('granted_workers'), 'provider': j.get('provider')})
    return out

def load_series(p):
    if not p or not os.path.exists(p): return []
    rows = []
    for r in csv.DictReader(open(p, encoding='utf-8')):
        try: rows.append({k: float(v) if v not in ('', None) else 0.0 for k, v in r.items() if k != 'stage'} | {'stage': r.get('stage','')})
        except Exception: pass
    return rows

def main():
    if len(sys.argv) < 2: print('usage: stage_profile.py <events.jsonl> [resource_timeseries.csv]'); return 2
    evs = load_events(sys.argv[1])
    st = stages_from_events(evs)
    if not st: print('no NODE_START/NODE_END events found in', sys.argv[1]); return 2
    t0 = min([s['start'] for s in st if s['start']] or [0])
    for s in st:
        s['rel_start'] = (s['start'] - t0) if s['start'] else None
        s['rel_end'] = (s['end'] - t0) if s['end'] else None
    series = load_series(sys.argv[2] if len(sys.argv) > 2 else None)
    for s in st:
        if s['rel_start'] is None or s['rel_end'] is None or not series: continue
        seg = [r for r in series if s['rel_start'] <= r.get('elapsed_seconds', 0) <= s['rel_end']]
        if seg:
            s['cpu_mean'] = sum(r.get('cpu_pct',0) for r in seg)/len(seg)
            s['cpu_peak'] = max(r.get('cpu_pct',0) for r in seg)
            s['rss_peak'] = max(r.get('rss_bytes',0) for r in seg)
            s['io_wait'] = sum(r.get('io_wait_pct',0) for r in seg)/len(seg)
            s['workers_mean'] = sum(r.get('active_workers',0) for r in seg)/len(seg)
            s['n_samples'] = len(seg)
    tot = sum(s['wall_ms'] for s in st if s['wall_ms']) or 1.0
    print('=== Phase1 逐阶段性能（按执行顺序）===')
    print('%-22s %10s %7s %9s %9s %11s %8s %8s' % ('stage(node)','wall_s','pct','cpu_mean%','cpu_peak%','rss_peak_MB','io_wait%','workers'))
    for s in st:
        w = (s['wall_ms'] or 0)/1000.0
        print('%-22s %10.2f %6.1f%% %9s %9s %11s %8s %8s' % (
            s['node'], w, 100.0*(s['wall_ms'] or 0)/tot,
            ('%.1f' % s['cpu_mean']) if 'cpu_mean' in s else 'n/a',
            ('%.1f' % s['cpu_peak']) if 'cpu_peak' in s else 'n/a',
            ('%.1f' % (s['rss_peak']/1048576)) if 'rss_peak' in s else 'n/a',
            ('%.2f' % s['io_wait']) if 'io_wait' in s else 'n/a',
            ('%.1f' % s['workers_mean']) if 'workers_mean' in s else 'n/a'))
    print('%-22s %10.2f' % ('TOTAL(events)', tot/1000.0))
    if series:
        print()
        print('注: cpu_mean 单位是「单核百分比」; 本机 16 核 => 1600%% 为满载。')
        print('    cpu_mean 远低于 1600%% 的阶段 = 并行度不足 / 单线程瓶颈，是优化首选目标。')
    return 0

if __name__ == '__main__':
    sys.exit(main())