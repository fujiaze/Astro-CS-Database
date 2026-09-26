#!/usr/bin/env python3
"""audit-sentinel.py（持久版）——哨兵：每 60s 查队列+活跃；
里程碑（双双为空）exit 0；每 18 分钟心跳 exit 42。"""
import json, pathlib, sys, time

BASE = pathlib.Path('/workspace/Astro CS Database/独立审计/运行时')
STATE = BASE / 'state.json'
QUEUE = BASE / 'queue.tsv'
HEARTBEAT = 18 * 60
start = time.time()

def counts():
    active = 0
    if STATE.exists():
        try:
            s = json.loads(STATE.read_text(encoding='utf-8'))
            active = len(s.get('active', {}))
        except Exception:
            active = -1
    pending = 0
    if QUEUE.exists():
        pending = len([l for l in QUEUE.read_text(encoding='utf-8').splitlines()
                       if l.strip() and not l.startswith('#')])
    return active, pending

while True:
    a, p = counts()
    if a == 0 and p == 0:
        print('MILESTONE: all queues drained, no active runners')
        sys.exit(0)
    if time.time() - start >= HEARTBEAT:
        print('HEARTBEAT: active=%d pending=%d' % (a, p))
        sys.exit(42)
    time.sleep(60)
