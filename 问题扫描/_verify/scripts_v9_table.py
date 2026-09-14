import json, os, re, collections
NL=chr(10)
reg=json.load(open('ci/checks.json',encoding='utf-8'))['checks']
KF=json.load(open('ci/known_failures.json',encoding='utf-8'))
kfid={f.get('check_id') or f.get('target') for f in KF.get('failures',[])}
def g2re(g):
    out=''; i=0
    while i<len(g):
        if g.startswith('**/',i): out+='(?:.*/)?'; i+=3
        elif g.startswith('/**',i): out+='(/.*)?'; i+=3
        elif g.startswith('**',i): out+='.*'; i+=2
        elif g=='*': out+='[^/]*'; i+=1
        elif g=='?': out+='.'; i+=1
        else: out+=re.escape(g[i]); i+=1
    return re.compile('^'+out+'$')
def gmatch(path,pats):
    path=path.lstrip('./')
    return any(g2re(p).match(path) for p in pats)
SELF=('--selftest','--selfcheck','--list','--legacy')
MECH={
 'TRACEABILITY':'A1 checker 无条件 return 0 (tools/quality/check_traceability.py:159)',
 'PRODUCTION-GRAPH':'A1 只跑 --selftest；真实 IR/trace 比对在登记面零载体',
 'ISA-LEAK-SELFTEST':'A1 只跑 --selftest；真实模式需 --binary，CI 从不扫交付二进制',
 'PROD-REACH-SELFTEST':'A1 只跑 --selftest；真实模式需 --binary/--compile_commands，零载体',
 'SERIAL-HEAVY-SELFTEST':'A1 只跑 --selftest（真面由 NO-SERIAL-HEAVY 承担）',
 'LOG-CONTRACT-SELFCHECK':'A1 只跑 --selfcheck（无样本即 PASS 语义）',
 'API-DOCS':'E5 run.py EMPTY_OUTPUT_SILENCE_EXEMPT：0 输出记 PASS',
 'UNIT-CLOSURE':'E5 run.py EMPTY_OUTPUT_SILENCE_EXEMPT：0 输出记 PASS',
 'UT-CPU-AVX512':'A2 waivable + 缺 AVX-512F 即 exit 77 → SKIPPED(waivable)',
 'CON-DOC-SYMBOLS':'E4 符号三向 substring + 白名单/启发式跳过；known_files=repo.rglob 含未跟踪影子树',
 'CON-CONFIG-CONTRACTS':'E4 字面量子串断言（weight_mode = 2 / 错误消息原文）',
 'CON-BUILD-GRAPH':'E4 字面量子串断言（add_library(phase2 等）',
 'CON-TRACEABILITY':'E4 关键词 substring 断言（keyword in 拼接 requirement_id 串）',
 'CON-TEST-CONTRACTS':'E4 synthetic_gate 兜底 + TST 数>=5 阈值',
}
MUSTRED={
 'CON-TRACEABILITY':'必红：PSF/REJ 关键词零命中（2 findings）',
 'TRACEABILITY-MATRIX':'必红：CSV 带 BOM→表头不等；剥 BOM 后仍 5 行与 JSON 发散',
 'DOC-LINE-ANCHORS':'必红：C4 13 条 BINDING_VIOLATION（本机 48 errors／干净检出 13）',
 'CON-API-CONTRACTS':'必红：API_CONTRACTS.csv:371 estimate_mag_lim_by_density 头文件已无声明',
 'CON-COMMENTS':'必红：synthetic_gate.cpp:5488 注释含 V19R2 且无「冻结」',
 'CON-FULL-INTEGRATION':'连带必红：聚合 10 个 checker，trace/api/comments 已 FAIL',
 'KNOWN-FAILURES-BASELINE-CHECK':'连带必红：上述红门不在基线→fail-closed 新增失败',
 'UT-BACKEND':'实测红（48ceee59 exit1）且基线已无条目；现状需运行期确认',
}
rows=[]; n_ck=0; n_ci=0; miss=[]
for c in reg:
    cid=c['id']; cmd=' '.join(map(str,c['command']))
    chk=[a for a in map(str,c['command']) if a.endswith('.py')]
    for a in map(str,c['command']):
        if a.endswith('.py') and not os.path.exists(a): miss.append((cid,a))
    cp=c['changed_paths']
    cov_ck=gmatch(chk[0],cp) if chk else True
    cov_ci=gmatch('ci/checks.json',cp)
    if not cov_ck: n_ck+=1
    if not cov_ci: n_ci+=1
    mech=MECH.get(cid,'')
    if not mech:
        if any(s in cmd for s in SELF): mech='A1 只跑自检/清单模式'
        elif c['waivable']: mech='A2 红被 waiver 吸收'
        elif cid.startswith(('UT-','CTEST-','BUILD-','WIN-','DEEP-','LINUX-','API-')): mech='需运行期（未静态复算）'
        else: mech='静态可红（未逐一复算）'
    rows.append((cid, '/'.join(p[0] for p in c['profiles']), c['platform'], 'Y' if c['waivable'] else 'N',
                 'Y' if cid in kfid else 'N', 'Y' if cov_ck else 'N', 'Y' if cov_ci else 'N',
                 MUSTRED.get(cid,'永不红' if cid in MECH else ''), mech))
hdr='| 门 id | profiles | plat | 可豁免 | 在基线 | paths覆盖checker | paths覆盖checks.json | 静态判定 | 失效机制/备注 |'
lines=[hdr,'|---|---|---|---|---|---|---|---|---|']
for r in rows: lines.append('| '+(' | '.join(str(x) if x!='' else '—' for x in r))+' |')
head=[
 '# V9 附表：ci/checks.json 逐门五判据表（机器生成，只读复算）','',
 '* 口径：python 解析 ci/checks.json；时点 HEAD 2f03dd89，UTC 2026-09-14T17:01:40Z（北京 2026-09-15 01:01）。门数 %d（本会话开始时 121，期间前台新增 9 门）。'%len(reg),
 '* 生成脚本 问题扫描/_verify/scripts_v9_table.py（只读）。「paths覆盖checker」= 该门自身 checker 路径是否落在其 changed_paths 内（N：改坏 checker 不触发该门）；「paths覆盖checks.json」= 登记面改动是否触发该门。',
 '* 「静态判定」只填有复算直证或实测直证的行；其余留「—」。profiles 缩写 P=fast L=linux-main W=windows-main D=linux-deep。','',
]
open('问题扫描/_verify/V9_gates_table.md','w',encoding='utf-8').write(NL.join(head+lines)+NL)
print('rows',len(rows),'checker-not-covered',n_ck,'registry-not-covered',n_ci)
print('missing checker files:',miss)
print('paths NOT covering own checker:', [r[0] for r in rows if r[5]=='N'])
print('paths NOT covering ci/checks.json count:', sum(1 for r in rows if r[6]=='N'))
