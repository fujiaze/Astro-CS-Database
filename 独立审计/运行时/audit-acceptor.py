#!/usr/bin/env python3
"""audit-acceptor.py（持久版）——Qoder 自验收环。
对守护记入 done 的成稿派 Qoder 验收员：VERDICT: PASS 收档；REDO 重排（≤2 次，须有 prompt）。
"""
import json, os, pathlib, subprocess, time

BASE = pathlib.Path('/workspace/Astro CS Database/独立审计/运行时')
STATE = BASE / 'state.json'
QUEUE = BASE / 'queue.tsv'
ACCEPT = BASE / '验收'; ACCEPT.mkdir(exist_ok=True)
REPO = '/workspace/Astro CS Database'
MAX_ACCEPT = 4

def load():
    return json.loads(STATE.read_text(encoding='utf-8'))

def save(s):
    STATE.write_text(json.dumps(s, ensure_ascii=False, indent=1), encoding='utf-8')

def pid_alive(pid):
    try:
        os.kill(int(pid), 0)
        return True
    except (OSError, TypeError, ValueError):
        return False

def accept_running():
    n = 0
    for f in ACCEPT.glob('*-verdict.md.running'):
        try:
            pid = int(f.read_text().strip())
            if pid_alive(pid):
                n += 1
            else:
                f.unlink()
        except (OSError, ValueError):
            f.unlink()
    return n

def spawn_verifier(name, rec):
    outfile = rec.get('outfile', '')
    vlog = ACCEPT / (name.replace('/', '_') + '-verdict.md')
    guard = pathlib.Path(str(vlog) + '.running')
    inner = ("cd '%s' && export PATH=/workspace/.local/bin:$PATH && "
             "qodercli -m Qwen3.8-Flash --permission-mode accept_edits -p "
             "'你是 ACSD 独立审计的验收员。请验收执行路 %s 的成稿是否完整合格。"
             "验收对象文件：%s（若该精确路径不存在，在同目录找去掉空格后的同名文件）。"
             "验收判据：1) 分配任务是否全覆盖（对照其文内自报覆盖率，必须 100%%）；"
             "2) 是否烂尾（结尾出现 接下来会写/下一步建议/待完成 等未完成语气即不合格）；"
             "3) PROGRESS 行是否满档（如有）；"
             "4) 抽验 2-3 个 文件:行 引用锚是否真实（打开核对，防文档幻觉）；"
             "5) 结论不得为空壳骨架；6) 实验类成稿须含 文献核验记录、可复现代码清单与结果数据、"
             "链条位置一节。"
             "输出：只写一个文件 %s，第一行必须是 VERDICT: PASS 或 VERDICT: REDO: <一句话原因>；"
             "第二行起给三条以内证据。仓库只读，除该验收文件外不得写任何文件。'"
             " > %s 2>&1") % (REPO, name, outfile, vlog,
                              ACCEPT / (name.replace('/', '_') + '.verifylog'))
    p = subprocess.Popen(['bash', '-c', inner], start_new_session=True,
                         stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    guard.write_text(str(p.pid))
    rec['verdict_file'] = str(vlog)
    rec['verdict_pid'] = p.pid
    print('verifier spawned:', name, flush=True)

def collect(name, rec):
    vf = pathlib.Path(rec.get('verdict_file', ''))
    g = pathlib.Path(str(vf) + '.running')
    try:
        if pid_alive(g.read_text().strip()):
            return
    except (OSError, ValueError):
        pass
    try:
        g.unlink()
    except OSError:
        pass
    txt = vf.read_text(encoding='utf-8', errors='replace') if vf.exists() else ''
    head = txt.lstrip()[:200]
    if head.startswith('VERDICT: PASS'):
        rec['verdict'] = 'PASS'
        print('accepted:', name, flush=True)
    elif head.startswith('VERDICT: REDO') or not txt:
        reason = head.replace('VERDICT: REDO:', '').strip()[:120]
        n = rec.get('retries', 0)
        if rec.get('prompt'):
            with QUEUE.open('a', encoding='utf-8') as f:
                f.write(chr(9).join([name, rec['prompt'], rec.get('flags') or '',
                                     rec['outfile'], rec.get('marker', 'auto')]) + chr(10))
            rec['retries'] = n + 1
            rec['verdict'] = 'REDO: ' + reason
            print('redo queued:', name, '|', reason, flush=True)
        else:
            rec['verdict'] = ('REDO-GIVEUP: ' + reason) if head.startswith('VERDICT: REDO') else 'VERIFIER-SILENT'
            print('giveup/silent:', name, flush=True)
    else:
        rec['verdict'] = 'UNPARSED'
        print('verdict unparsed:', name, '|', head[:80], flush=True)

def loop():
    while True:
        try:
            s = load()
            for name, rec in list(s['done'].items()):
                if rec.get('verdict') is not None or rec.get('verdict_pid'):
                    continue
                if 'outfile' not in rec:
                    rec['verdict'] = 'SKIP-NOFILE'
                    continue
                if accept_running() >= MAX_ACCEPT:
                    break
                spawn_verifier(name, rec)
                save(s)
            for name, rec in list(s['done'].items()):
                if rec.get('verdict_pid') and rec.get('verdict') is None:
                    collect(name, rec)
            save(s)
        except Exception as e:
            print('acceptor error:', repr(e), flush=True)
        time.sleep(30)

if __name__ == '__main__':
    loop()
