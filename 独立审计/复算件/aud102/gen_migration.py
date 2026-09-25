#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""AUD-102 D2 交付物之二：从 D1 台账机械生成《迁移合并清单》逐份动作表（360 行全覆盖）"""
import io, os, re, sys
from collections import OrderedDict

sys.stdout.reconfigure(encoding='utf-8')
OUT = r"独立审计/复算件/aud102"
DOC = r"独立审计/01_文档\迁移合并清单.md"
INV = r"独立审计/批次清单"
FIELDS = ['batch', 'path', 'lines', 'title', 'role', 'topic', 'up', 'down', 'auth', 'dup',
          'meta', 'viol', 'dangle', 'conflict', 'disp']

# ---------- 读台账记录（同一路径取字段最长的那条） ----------
rec = {}
for ln in io.open(os.path.join(OUT, 'records3.tsv'), encoding='utf-8'):
    p = ln.rstrip('\n').split('\t')
    p += [''] * (len(FIELDS) - len(p))
    r = dict(zip(FIELDS, p))
    path = r['path']
    if path.startswith('tasks/'):
        path = '工程控制/RELEASE-05/' + path
    cur = rec.get(path)
    score = lambda x: len((x or {}).get('disp', '')) + len((x or {}).get('dup', '')) + len((x or {}).get('conflict', ''))
    if cur is None or score(r) > score(cur):
        rec[path] = r

grade = {}
for l in io.open(os.path.join(OUT, 'evidence_field.tsv'), encoding='utf-8'):
    t = l.rstrip('\n').split('\t')
    if t[0] != 'path':
        grade[t[0]] = t[2]

uni = [l.strip() for l in io.open(os.path.join(INV, '_union.txt'), encoding='utf-8') if l.strip()]
TRACKED = set(x.strip().replace(chr(92), '/') for x in io.open(os.path.join(INV, '_all_docs_universe.txt'), encoding='utf-8', errors='replace') if x.strip())
import subprocess
try:
    _o = subprocess.run(['git', '-C', r'F:/Astro dev/Astro CS Normalization Database', '-c', 'core.quotePath=false', 'ls-files'],
                        capture_output=True, text=True, encoding='utf-8').stdout
    TRACKED |= {x.replace(chr(92), '/') for x in _o.split()}
except Exception:
    pass
print('tracked set size:', len(TRACKED))

# ---------- 应角色格子（与《目标文档架构》§3 一致） ----------
def target_role(p):
    d = os.path.dirname(p)
    b = os.path.basename(p)
    if d == '':
        return {'ASTROCS_DESIGN.md': '顶层决策（第 1 层）',
                'AGENTS.md': '标准与检查（干活手册）',
                'ENGINEERING_SPEC.md': '标准与检查（工程规范）',
                'ACCEPTANCE_SPEC.md': '负责人面（验收规范）',
                'CONTROL_PACK_SPEC.md': '标准与检查（任务包治理）',
                'README.md': '入口与导航',
                'DEPENDENCIES.md': '操作与运营（依赖锁定）',
                'FATDUCK_ACCESS.md': '操作与运营（节点接入）',
                'memory.md': '过程记录（不入正式文档层）'}.get(b, '操作与运营')
    if d == 'docs':
        return {'GLOSSARY.md': '术语与词表', 'KNOWN_LIMITATIONS.md': '限制与未决登记',
                'VERSIONING.md': '标准与检查（版本治理）', 'DEVELOPER_GUIDE.md': '操作与运营（开发上手）',
                'TROUBLESHOOTING.md': '操作与运营（并入 diagnostics）',
                'DOCUMENT_INDEX.yaml': '索引与登记面', 'TRACEABILITY.csv': '追溯登记（并入 traceability）'}.get(b, '待判')
    m = {'docs/science': '科学定义', 'docs/algorithms': '算法推导', 'docs/architecture': '架构设计',
         'docs/architecture/abi': '架构设计（接口面）', 'docs/architecture/cpu': '架构设计',
         'docs/architecture/observability': '架构设计', 'docs/design': '架构设计（详细设计）',
         'docs/interfaces': '架构设计（接口面）', 'docs/interfaces/data': '架构设计（接口面）',
         'docs/interfaces/io': '架构设计（接口面）', 'docs/api': '架构设计（接口面）→ 迁 interfaces',
         'docs/contracts': '合同说明', 'docs/standards': '标准与检查',
         'docs/standards/checks': '第四层代码（不属文档层）', 'docs/ci': '标准与检查（机器门）',
         'docs/modules': '模块工作细节（手写页折入 plugins）', 'docs/modules/registry': '模块工作细节（生成面）',
         'docs/plugins': '模块工作细节（正本目录）', 'docs/plugins/algorithms_phase1': '模块工作细节（正本目录）',
         'docs/plugins/algorithms_phase2': '模块工作细节（正本目录）', 'docs/plugins/algorithms_phase3': '模块工作细节（正本目录）',
         'docs/plugins/infrastructure': '模块工作细节（正本目录）',
         'docs/research': '研究与佐证', 'docs/references': '研究与佐证',
         'docs/development': '操作与运营', 'docs/operations': '操作与运营',
         'docs/performance': '操作与运营（性能）', 'docs/diagnostics': '操作与运营（诊断）',
         'docs/traceability': '追溯登记', 'docs/owner': '负责人面', 'docs/quality': '标准与检查（阈值）+ 证据快照',
         'docs/audit': '索引与登记面 → 出库证据层', 'docs/browser': '模块工作细节（折入 plugins/infrastructure）',
         'docs/validation': '标准与检查（判据）+ 证据矩阵', 'docs/validation/v6': '证据矩阵 → 出库证据层',
         'docs/algorithms/anchors': '算法推导（锚合同）+ 第四层脚本'}
    for k in sorted(m, key=len, reverse=True):
        if d == k or d.startswith(k + '/'):
            return m[k]
    if d.startswith('实验'):
        return '实验单元（旁挂佐证）'
    if d.startswith('工程控制'):
        return '任务包与过程记录（收口后出库）'
    return '待判'

def cur_role(p, r):
    if not r:
        return '台账未记录'
    s = re.sub(r'\s+', ' ', r.get('role', '')).strip()
    s = re.split(r'（|⇒|实际|应属|错放', s)[0].strip(' ，,;。')
    return (s or '台账未标注')[:28]

ACT = [('KEEP', '保留'), ('EDIT', '就地整改'), ('MERGE', '合并'), ('DEL', '删除'), ('MOVE', '迁移'),
       ('SPLIT', '拆分'), ('SINK', '下沉'), ('ESC', '上呈'), ('TBD', '待定案')]

CONTENT_LEVEL = re.compile(r'^(掉)?\s*(\d|3[0-9]|[0-9]+)\s*[–\-~至到]\s*[0-9]+\s*行|^[\s]*(本|该)?(行|段|节|表|列|字段|注释|声明|锚|编号|句|两行|内容|清单|块|末句|其中)')

def acts(d):
    """区分文件级动作与就地整改：动作词后紧跟"第NN–NN行/字段/节…"者属就地整改，不改文件归属"""
    a = []
    for k, c in (('保留', 'KEEP'), ('合并', 'MERGE'), ('并入', 'MERGE'), ('删除', 'DEL'), ('移除', 'DEL'),
                 ('迁往', 'MOVE'), ('迁移', 'MOVE'), ('迁入', 'MOVE'), ('下沉', 'SINK'), ('拆分', 'SPLIT'),
                 ('待定', 'TBD'), ('待证', 'TBD'), ('上呈', 'ESC')):
        for m in re.finditer(k, d or ''):
            tail = (d or '')[m.end():m.end() + 14]
            if c in ('DEL', 'MERGE', 'MOVE', 'SINK', 'SPLIT') and CONTENT_LEVEL.match(tail.lstrip('（(')):
                if 'EDIT' not in a:
                    a.append('EDIT')
                continue
            if c not in a:
                a.append(c)
    return a

TARGET = re.compile(r'(?:并入|并往|合并入|吸收进|迁入|迁往|移到|挪至|改指|上收(?:至到)?|下沉(?:至到)?|出库到?|落|迁移|迁)\s*[`「]?\s*([\w\u4e00-\u9fff.\-\*/]+\.(?:md|ya?ml|csv|json|txt)|[\w\u4e00-\u9fff.\-]+/[\w\u4e00-\u9fff.\-\*/]+/)')
BASIS = re.compile(r'标准\s*01\s*§(\d)')
COUNTER = re.compile(r'[\w\u4e00-\u9fff.\-\*/]+/[\w\u4e00-\u9fff.\-\*/]+\.(?:md|ya?ml|csv|json|txt)')

PATHSHAPE = re.compile(r'^(?:[\w一-鿿.\-]+/)+[\w一-鿿.\-*]+(?:\.(?:md|ya?ml|csv|json|txt))?$')

def tag(v, self_path):
    if not PATHSHAPE.match(v) or os.path.basename(v) == os.path.basename(self_path) or '本文件' in v or '本文' in v:
        return None
    if v.endswith('/') or '*' in v:
        return '→ ' + v + '（目录级落位）'
    if v in TRACKED:
        return '→ ' + v + '（去向实存）'
    if v.startswith('artifacts/evidence/') or v.startswith('docs/standards/') or v.startswith('docs/interfaces/'):
        return '→ ' + v + '（去向待立：目标目录/文件尚不存在，须先立再迁）'
    return '→ ' + v + '（去向不存在：台账所指目标不在跟踪集，先核再迁）'

def target_of(p, blob, A):
    if not ({'MERGE', 'DEL', 'MOVE', 'SPLIT', 'SINK'} & set(A)):
        return ''
    for m in TARGET.finditer(blob or ''):
        r = tag(m.group(1).strip('`'), p)
        if r:
            return r
    for v in COUNTER.findall(blob or ''):
        r = tag(v, p)
        if r:
            return r + '（取自"重复或重叠对象"字段，台账未写明去向）'
    return ''

def basis_of(d, conf, A):
    m = BASIS.search((d or '') + ' ' + (conf or ''))
    if m:
        core = '标准 01 §' + m.group(1)
    else:
        core = {'SINK': '§1（细节下沉）', 'MERGE': '§2（同主题一份正本）', 'DEL': '§2 末条（过程台账不落正式层）',
                'MOVE': '§2（角色与目录一一对应）', 'SPLIT': '§2 + §3（主题拆分需上位锚）',
                'ESC': '§1（与上位冲突未消）', 'KEEP': '§3/§4（就地整改：指针与写法）'}.get(
            next((c for c in ('MERGE', 'DEL', 'MOVE', 'SPLIT', 'SINK', 'ESC', 'KEEP') if c in A), 'KEEP'), '§3')
    if any(k in (d or '') for k in ('日期', '任务编号', '元信息块', '历史叙事', '无上游', '缺上游')):
        core += '、§4'
    return core

def precondition(p, A, r):
    pre = []
    if {'MOVE', 'MERGE', 'DEL', 'SPLIT'} & set(A):
        pre.append('公共前置：先按《索引重建规格》落定新落位与角色轴，再动文件')
    if 'MERGE' in A:
        pre.append('去向正本的上游行号—标题配对已校验通过')
    if 'SPLIT' in A:
        pre.append('先补最高设计对应节要点（拆出主题的上位锚）')
    if 'DEL' in A:
        pre.append('引用者清零：先把指向本文的指针改指去向，再删体')
    if 'ESC' in A:
        pre.append('负责人裁决在案，缺裁决不执行')
    if not pre and 'EDIT' in A:
        pre.append('无（内容级整改，可单独提交）')
    return '；'.join(pre) if pre else '无'

def risk(p, r):
    dn = (r or {}).get('down', '') if r else ''
    m = re.search(r'(\d+)\s*(?:个|处|份|条)?', dn)
    who = '台账记下游引用 ' + m.group(1) + ' 处' if m else '台账未给下游计数'
    if os.path.basename(p).endswith(('.csv', '.json', '.yaml')):
        who += '；机器件被门或脚本读取，删改前先核对读取方'
    return who + '，删/并后断链风险面 = 该清单'

lines = []
n = 0
for p in uni:
    r = rec.get(p)
    d = re.sub(r'\s+', ' ', (r or {}).get('disp', ''))
    conf = re.sub(r'\s+', ' ', (r or {}).get('conflict', ''))
    dupf = re.sub(r'\s+', ' ', (r or {}).get('dup', ''))
    blob = ' ；'.join([d, dupf, conf])
    A = acts(d)
    n += 1
    if not A:
        A = ['KEEP'] if r else ['TBD']
    if r is None:
        act_txt = '待定案'
        tgt = '缺：台账无逐份记录（本份在批次清单内未见登记）'
        bas = '标准 01 §5 第 1 项（逐份登记）未满足'
        pre = '先补 D1 台账条目'
        gr = 'E0'
    else:
        act_txt = '/'.join(dict.fromkeys([t for c, t in ACT if c in A]))
        g = grade.get(p, '')
        gr = {'E1': 'E1 双段', 'E2': 'E2 单段', 'E3': 'E3 只点名', 'E4': 'E4 未点名对方', 'E5': 'E5 无举证'}.get(g, g or '未评（非删并类动作）')
        if {'MERGE', 'DEL'} & set(A) and g != 'E1':
            act_txt = act_txt.replace('合并', '合并(待定案)').replace('删除', '删除(待定案)')
            tgt = '待定案：缺两处实际段落对照（举证级 ' + (g or '无') + '）'
        else:
            t = target_of(p, blob, A)
            if t:
                tgt = t
                if {'MERGE', 'DEL'} & set(A) and len(dupf) > 24:
                    tgt += '（同主题对方：' + dupf[:56] + '…）'
            elif 'EDIT' in A:
                m = re.search(r'[（(]([^）()]{6,120})[）)]', d)
                tgt = '无（文件不动）；就地整改：' + (m.group(1)[:70] if m else d[:70])
            elif {'MERGE', 'DEL', 'MOVE', 'SPLIT', 'SINK'} & set(A):
                tgt = '缺：台账未点名去向；执行前按《目标文档架构》§3 落位表指定，并在索引草稿登记新路径'
            else:
                m = re.search(r'[（(]([^）()]{6,120})[）)]', d)
                tgt = '无（保留原位）' + ('；' + m.group(1)[:70] if m else '')
        bas = basis_of(d, conf, A)
        pre = precondition(p, A, r)
    lines.append('| `%s` | %s | %s | %s | %s | %s | %s | %s | %s |' % (
        p, cur_role(p, r), target_role(p), act_txt, tgt, bas, pre, risk(p, r), gr))

head = """# 迁移合并清单

逐份动作表：覆盖被审文档集全部 %d 份（含根治理文档、`docs/**`、`实验/**`、`工程控制/RELEASE-05/**`）。一行一份，每行可独立执行、独立核对。

读法与判据：

- **现角色** = D1 台账对该份的角色判定原文首句；**应角色** = 《目标文档架构》§3 角色表给出的格子（含补全格）。两者不等即为角色错放，须按"动作"列执行迁移。
- **动作**取值：保留 / 合并 / 删除 / 迁移 / 拆分 / 下沉（内容下移一层）/ 上呈（须负责人裁决）/ 待定案。台账给出"合并/删除"却未贴两处实际段落的，一律降级为 `合并(待定案)`、`删除(待定案)`，并在去向列写明缺什么举证——不把推断升格为结论。
- **举证级**（判据 `E-MERGE-DEL-2`，取该份登记的 `重复或重叠对象`+`与上位冲突`+`处置建议` 三字段为限，不借邻行内容）：
  - `E1 双段`：点名对方文档，且字段内出现两处以上可定位引证（`「原文」`、`:行号`、`~~划掉~~`、反引号原文）；
  - `E2 单段`：点名对方但引证不足两处；`E3 只点名`：无引证；`E4`：有引证未点名对方；`E5`：两者皆无；`E0`：台账无逐份记录。
  - 只有 E1 直接可执行；E2–E5 由执行节点补对照段落后才升级为可执行。
- **依据** 一律指向标准 01 的条款号（§1 权威链 / §2 角色与正本唯一 / §3 双向索引 / §4 写法 / §5 产出）。
- **前置** 是该行的硬顺序：不满足前置就执行会制造新的悬空引用。索引重建（《索引重建规格.md》）是所有迁移、合并、删除行的公共前置。
- **风险** 给出台账记录的下游引用计数与删改后的断链面；机器件（csv/json/yaml）额外提示读取方。

""" % len(uni)

with io.open(DOC, 'w', encoding='utf-8') as w:
    w.write(head)
    w.write('| 对象路径 | 现角色 | 应角色 | 动作 | 合并来源→去向 / 迁移目标 | 依据 | 前置 | 风险 | 举证 |\n')
    w.write('|---|---|---|---|---|---|---|---|---|\n')
    for l in lines:
        w.write(l + '\n')
    w.write('\n<!-- PROGRESS: 逐份动作表 1/1 单元（%d 行）已铺完；加深进行中 -->\n' % len(lines))
print('rows:', len(lines), 'bytes:', os.path.getsize(DOC))
