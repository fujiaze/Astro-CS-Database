#!/usr/bin/env python3
"""PERF-DRZ: 修正 RELEASE-02 stage_cpu.py 的时间区间对齐缺陷。

缺陷 (原始 run/RELEASE-02/L4-rebuild/stage_cpu.py):
  探针中 *阶段级* 事件 (name in {calibrate,drizzle,wcs,...}) 的 ts_utc 是
  **阶段结束时刻**, wall_us 才是阶段耗时。原脚本把 ts_utc 当作阶段**开始**
  时刻 (a = t - t0; b = a + w), 于是 drizzle 阶段被采样到"阶段结束后"的
  空窗/空闲尾段 -> cpu_pct ~121 -> 错误地得出 "drizzle 平均 1.21 核"。

证据 (END 语义):
  probe: drizzle.frame(1) end=...086.761 wall=35.663  -> start=...051.098
         drizzle.frame(2) end=...127.516 wall=40.755  -> start=...086.761
         drizzle (stage)  end=...127.516 wall=76.418  -> start=...051.098
  两帧首尾相接且 sum(frame wall)=stage wall, 且 frame2.start==frame1.end。
  events.jsonl stage_start = 15:20:32Z; calibrate end 15:20:34.387 wall 2.366
  -> start 15:20:32.021 = 进程起点。END 语义成立。

本脚本用正确窗口 [t_end-w, t_end] 重算, 并与原脚本口径并列对比。
用法: python3 run/RELEASE-02/perf-drz/stage_cpu_fixed.py
"""
import json, csv, os, glob, collections, datetime

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
L4 = os.path.join('/workspace/Astro CS Database', 'run/RELEASE-02/L4-rebuild')
STAGE_NAMES = ('calibrate', 'cosmetic', 'star_psf', 'wcs', 'noise',
               'drizzle', 'writer', 'photometry')


def pts(s):
    s = s.strip().replace('Z', '')
    for f in ('%Y-%m-%dT%H:%M:%S.%f', '%Y-%m-%dT%H:%M:%S'):
        try:
            return datetime.datetime.strptime(s, f).timestamp()
        except ValueError:
            pass
    return None


def load_probe():
    ev = []
    for line in open(os.path.join(L4, 'probe.jsonl'), encoding='utf-8', errors='replace'):
        line = line.strip()
        if not line.startswith('{'):
            continue
        try:
            j = json.loads(line)
        except Exception:
            continue
        t = pts(j.get('ts_utc', ''))
        if t is None:
            continue
        ev.append({'t': t, 'name': j.get('name'), 'w': j.get('wall_us', 0) / 1e6,
                   'frame': j.get('frame_key', ''), 'scope': j.get('scope', '')})
    ev.sort(key=lambda e: e['t'])
    return ev


def load_series(path):
    s = []
    for r in csv.DictReader(open(path, encoding='utf-8')):
        try:
            s.append((float(r['elapsed_seconds']), float(r.get('cpu_pct') or 0)))
        except Exception:
            pass
    return s


def mean_cpu(series, a, b):
    seg = [c for e, c in series if a <= e <= b]
    return (sum(seg) / len(seg)) if seg else None


def main():
    ev = load_probe()
    # 每个配置: 以阶段级 calibrate 事件 (END, wall) 反推进程起点 T0
    groups = []
    cur = None
    for e in ev:
        if e['name'] == 'calibrate' and e['scope'] == 'phase1':
            if cur:
                groups.append(cur)
            cur = {'t0': e['t'] - e['w'], 'events': []}
        if cur is not None:
            cur['events'].append(e)
    if cur:
        groups.append(cur)

    cfgdirs = sorted(glob.glob(os.path.join(L4, 'norm/*/')))
    print('configs(probe)=%d  norm_dirs=%d' % (len(groups), len(cfgdirs)))
    print()
    # 只比较能对齐的配置
    agg_fix = collections.defaultdict(lambda: [0.0, 0.0, 0])   # stage -> [wall, cpu_wall, n]
    agg_bug = collections.defaultdict(lambda: [0.0, 0.0, 0])
    for g, cd in zip(groups, cfgdirs):
        series = load_series(os.path.join(cd, 'resource_timeseries.csv'))
        if not series:
            continue
        for e in g['events']:
            if e['name'] not in STAGE_NAMES or e['scope'] != 'phase1':
                continue
            w = e['w']
            # 正确: END 语义 -> [end-w, end] 相对 T0
            end = e['t'] - g['t0']
            a, b = end - w, end
            c = mean_cpu(series, a, b)
            agg_fix[e['name']][0] += w
            agg_fix[e['name']][1] += (c if c is not None else 0.0) * w
            agg_fix[e['name']][2] += 1
            # 原脚本口径: 把 END 当 START -> [end, end+w]; 空窗按 cpu=0 计入
            c2 = mean_cpu(series, end, end + w)
            agg_bug[e['name']][0] += w
            agg_bug[e['name']][1] += (c2 if c2 is not None else 0.0) * w
            agg_bug[e['name']][2] += 1

    print('%-12s %12s %12s | %12s %12s' %
          ('stage', 'wall_s', 'FIXED cores', 'wall_s', 'BUG cores'))
    tot_f = tot_b = 0.0
    for n in sorted(agg_fix, key=lambda k: -agg_fix[k][0]):
        w, cw, k = agg_fix[n]
        wb, cwb, kb = agg_bug.get(n, [0, 0, 0])
        tot_f += w
        tot_b += wb
        print('%-12s %12.1f %12.2f | %12.1f %12.2f' %
              (n, w, (cw / w / 100.0 if w else 0), wb, (cwb / wb / 100.0 if wb else 0)))
    print('%-12s %12.1f %12s | %12.1f' % ('TOTAL', tot_f, '', tot_b))

    # drizzle 逐帧 FIXED 核数
    print()
    print('=== drizzle.frame 逐帧 (FIXED, END 语义) ===')
    rows = []
    for g, cd in zip(groups, cfgdirs):
        series = load_series(os.path.join(cd, 'resource_timeseries.csv'))
        if not series:
            continue
        for e in g['events']:
            if e['name'] != 'drizzle.frame':
                continue
            end = e['t'] - g['t0']
            c = mean_cpu(series, end - e['w'], end)
            rows.append((os.path.basename(cd.rstrip('/')), e['frame'], e['w'], c))
    cs = [r[3] for r in rows if r[3] is not None]
    for r in rows[:6]:
        print('  %-12s wall=%6.2fs cores=%s' % (r[0], r[2], ('%.2f' % (r[3]/100.0)) if r[3] is not None else 'n/a'))
    print('  ... n=%d frames; cores mean=%.2f min=%.2f max=%.2f' %
          (len(cs), sum(cs) / len(cs) / 100.0, min(cs) / 100.0, max(cs) / 100.0))


if __name__ == '__main__':
    main()
