#!/usr/bin/env python3
"""audit-monitor.py v5（持久版）——执行路守护：判活＋完成标记＋队列补位。
用法：AUDIT_MAX_CONC=16 python3 -u audit-monitor.py run
"""
import json, os, pathlib, signal, subprocess, sys, time

BASE = pathlib.Path('/workspace/Astro CS Database/独立审计/运行时')
STATE = BASE / 'state.json'
QUEUE = BASE / 'queue.tsv'
LOGDIR = BASE / 'logs'; LOGDIR.mkdir(exist_ok=True)
MLOG = BASE / 'monitor.log'
PROMPTS = BASE / 'prompts'
REPO = '/workspace/Astro CS Database'
MAX_CONC = int(os.environ.get('AUDIT_MAX_CONC', '9'))
STALL_MIN = 30
LOOP = 30

def log(msg):
    line = time.strftime('%H:%M:%S ') + str(msg)
    print(line, flush=True)
    with MLOG.open('a', encoding='utf-8') as f:
        f.write(line + chr(10))

def load():
    if STATE.exists():
        return json.loads(STATE.read_text(encoding='utf-8'))
    return {"active": {}, "done": {}}

def save(s):
    STATE.write_text(json.dumps(s, ensure_ascii=False, indent=1), encoding='utf-8')

def file_stat(p):
    try:
        st = pathlib.Path(p).stat()
        return st.st_size, st.st_mtime
    except OSError:
        pass
    pth = pathlib.Path(p)
    parts = [x for x in pth.parts]
    # 逐级空格归一化匹配（模型爱在目录/文件名里插空格）
    cur = pathlib.Path(parts[0])
    ok = True
    for seg in parts[1:]:
        want = seg.replace(' ', '')
        found = None
        try:
            for cand in cur.iterdir():
                if cand.name.replace(' ', '') == want:
                    found = cand
                    break
        except OSError:
            ok = False
            break
        if not found:
            ok = False
            break
        cur = found
    if ok:
        try:
            st = cur.stat()
            return st.st_size, st.st_mtime
        except OSError:
            pass
    return None, None

def resolve(p):
    """把带空格变体的路径解析为真实存在的路径；不存在返回 None"""
    size, _ = file_stat(p)
    if size is None:
        return None
    pth = pathlib.Path(p)
    cur = pathlib.Path(pth.parts[0])
    for seg in pth.parts[1:]:
        want = seg.replace(' ', '')
        for cand in cur.iterdir():
            if cand.name.replace(' ', '') == want:
                cur = cand
                break
    return cur

def full_text(p):
    rp = resolve(p)
    if rp is None:
        return None
    try:
        return rp.read_text(encoding='utf-8', errors='replace')
    except OSError:
        return None

def prog_complete(txt, marker):
    if not txt:
        return False
    tail = txt[-5000:]
    if marker == '文末自报':
        return '文末自报' in tail
    if marker == '覆盖率自报':
        return '覆盖率自报' in tail
    return ('文末自报' in tail) or ('覆盖率自报' in tail)

def launch(name, prompt, flags, outfile, marker='auto'):
    logf = LOGDIR / (name.replace('/', '_') + '.out')
    q = chr(34)
    inner = (("cd '%s' && export PATH=/workspace/.local/bin:$PATH && "
             "qodercli -m Qwen3.8-Flash %s -p " + q + "$(cat %s)" + q + " > '%s' 2>&1")
             % (REPO, flags, prompt, logf))
    p = subprocess.Popen(['bash', '-c', inner], start_new_session=True,
                         stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    PROC[p.pid] = p
    s = load()
    s['active'][name] = {"outfile": outfile, "marker": marker, "pid": p.pid,
                         "prompt": prompt, "flags": flags,
                         "last_size": 0, "last_mtime": None,
                         "last_change": time.time(), "started": time.time()}
    save(s)
    log('launched %s pid %s' % (name, p.pid))

PROC = {}

def _pid_dead(pid):
    try:
        os.kill(int(pid), 0)
    except (ProcessLookupError, PermissionError, TypeError, ValueError):
        return True
    try:
        stat = pathlib.Path('/proc/%d/stat' % int(pid)).read_text()
        return stat[stat.rindex(')') + 2:stat.rindex(')') + 3] == 'Z'
    except (OSError, ValueError):
        return True

def probe():
    s = load()
    exited = []
    for name, r in s['active'].items():
        pid = r.get('pid')
        p = PROC.get(pid)
        if p is not None and p.poll() is not None:
            exited.append(name)
        elif _pid_dead(pid):
            exited.append(name)
    for name in exited:
        r = s['active'][name]
        txt = full_text(r['outfile'])
        if txt and prog_complete(txt, r.get('marker', 'auto')):
            r['status'] = 'done'
            s['done'][name] = s['active'].pop(name)
            log('runner done: ' + name)
        else:
            n = r.get('retries', 0)
            del s['active'][name]
            r['status'] = 'exited-incomplete'
            s['done'][name] = r
            log('exited-incomplete: %s size=%s' % (name, len(txt) if txt else 0))
            if r.get('prompt'):
                with QUEUE.open('a', encoding='utf-8') as f:
                    f.write(chr(9).join([name, r['prompt'], r.get('flags') or '',
                                         r['outfile'], r.get('marker', 'auto')]) + chr(10))
                r['retries'] = n + 1
    for name, r in list(s['active'].items()):
        size, mt = file_stat(r['outfile'])
        if size != r.get('last_size'):
            r['last_size'], r['last_change'] = size, time.time()
        else:
            idle = (time.time() - r.get('last_change', time.time())) / 60
            if idle > STALL_MIN:
                log('%s idle %.0fmin' % (name, idle))
        if size:
            txt = full_text(r['outfile'])
            if txt and prog_complete(txt, r.get('marker', 'auto')):
                r['status'] = 'done'
                s['done'][name] = s['active'].pop(name)
                log('runner done(marker): ' + name)
    save(s)
    free = MAX_CONC - len(s['active'])
    if free > 0 and QUEUE.exists():
        lines = [l for l in QUEUE.read_text(encoding='utf-8').splitlines()
                 if l.strip() and not l.startswith('#')]
        take, rest = lines[:free], lines[free:]
        for line in take:
            parts = line.split(chr(9))
            if len(parts) < 4:
                log('bad queue line: ' + line[:40])
                continue
            name, prompt, flags, outfile = parts[0], parts[1], parts[2], parts[3]
            marker = parts[4] if len(parts) > 4 else 'auto'
            prev = s['done'].get(name)
            if name in s['active'] or (prev is not None and prev.get('status') in ('done', 'already-done')):
                continue
            if name in s['done']:
                del s['done'][name]
            txt = full_text(outfile)
            if txt and prog_complete(txt, marker):
                s['done'][name] = {"status": "already-done", "outfile": outfile}
                log('queue skip(done): ' + name)
                continue
            launch(name, prompt, flags, outfile, marker)
        QUEUE.write_text(chr(10).join(rest) + (chr(10) if rest else ''), encoding='utf-8')
        if take:
            log('dispatched %d, remaining %d' % (len(take), len(rest)))
    log('active=%d done=%d' % (len(s['active']), len(s['done'])))

if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1] == 'run':
        log('monitor v5 up, conc=%d' % MAX_CONC)
        while True:
            try:
                probe()
            except Exception as e:
                log('probe error: %r' % (e,))
            time.sleep(LOOP)
    else:
        probe()
