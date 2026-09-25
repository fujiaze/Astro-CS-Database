# -*- coding: utf-8 -*-
"""D4 finish —— master-table patcher (coverage reconciliation only).

Ops (all deterministic, no re-judgement):
  A  fill column 16 「多侧默认值预筛」 from raw/D4-多侧默认值全表.csv (+ 候选 tier)
  B  side backfill for the 148 conflict-marked rows: every concrete site named in
     the row's own prose cells (出处/冲突标记/有效有限域/适用域/精度要求) and in the
     matching judgement-layer rows, resolved to a tracked full path, is appended
     to 位置集合 marked 「补侧」
  C  待确认 rows: conservative direction / impact backfilled from the judgement
     layer's 备注 or 处置 text; rows where neither the judgement layer nor the
     review layer gives one are marked 「缺输入」 and counted
  D  append the layer-2 (复核) entries absent from the master (11 rows, curated
     in ROWS_L2 below, every cell copied from a cited review document)

Usage:  python t1_patch_master.py --dry-run   |   python t1_patch_master.py
"""
import io, os, re, sys, csv, json, collections

sys.stdout.reconfigure(encoding='utf-8')
ROOT = r'产出/'
MASTER = ROOT + r'\独立审计/02_科学\常数公式算法总台账.csv'
RAW = ROOT + r'\raw'
WORK = ROOT + r'\复算\d4finish'
SEP = '；'
ABSENT = ('', '该侧无此项', '—', '-', '缺', '无', 'N/A', 'null')

SITE = re.compile(
    r'((?:[\w.\-]+/)*[\w.\-]+\.(?:h|hpp|hh|inl|cpp|cc|c|json|jsonl|md|py|sh|cmake|txt|yaml|yml|csv|toml|in))'
    r'(?:[:：](\d+)(?:-(\d+))?)?')
FULL = re.compile(r'^[\w.\-]+(/[\w.\-.]+)+/')   # crude: has at least one slash


def rd(p, keep_ragged=False):
    rows = list(csv.reader(io.open(p, encoding='utf-8-sig', newline='')))
    return rows


def read_master():
    rows = rd(MASTER)
    hdr = rows[0]
    body = [r + [''] * (16 - len(r)) if len(r) < 16 else r for r in rows[1:] if len(r) > 1]
    return hdr, body


def norm_key(s):
    s = (s or '').strip()
    s = re.sub(r'^\(无名\)·', '', s)
    s = re.split(r'[（(]', s, 1)[0]
    s = re.sub(r'[\s:：/\\]*[\w./\-]+\.(h|hpp|cpp|cc|json|md|py|yaml|yml|csv|txt|jsonl)\b.*$',
               '', s, flags=re.I)
    return s.strip().strip('·').lower()


# ---------------------------------------------------------------- basename map
def build_path_map():
    m = collections.defaultdict(set)
    srcs = [RAW + r'\AUD-402-常数台账.csv', RAW + r'\AUD-402-常数台账.旁表-无名值与测试钉值.csv']
    for f in srcs:
        for r in rd(f)[1:]:
            for g in SITE.finditer(' '.join(r[:3])):
                if '/' in g.group(1):
                    m[g.group(1).split('/')[-1]].add(g.group(1))
    hdr, body = read_master()
    for r in body:
        for g in SITE.finditer(' '.join(r)):
            if '/' in g.group(1):
                m[g.group(1).split('/')[-1]].add(g.group(1))
    for f in ('AUD-402-判读-A1.csv', 'AUD-402-判读-A2.csv', 'AUD-402-判读-A3.csv',
              'AUD-402-判读-BD1.csv'):
        for r in rd(RAW + '\\' + f)[1:]:
            for g in SITE.finditer(' '.join(r)):
                if '/' in g.group(1):
                    m[g.group(1).split('/')[-1]].add(g.group(1))
    return m


VALPAT = re.compile(r'[=＝≈:：]\s*[-+]?\d|[-+]?\d\.(?:0|[1-9]\d*)\b|\d+(?:\.\d+)?(?:e-?\d+)?\s*(?:px|s|ADU|mag|dex|%)')
SIDEPAT = re.compile(r'唯一数值源|唯一家|缺键兜底|兜底|默认|声明值|出厂模板|骨架|登记册|schema|契约')


def prose_value_sites(text, pmap, stats):
    """every full-path site in prose, tagged by whether a value/side word sits next
    to it (the six-side model counts the documentation and contract faces as sides,
    so authority anchors are added too -- but labelled differently)."""
    out = []
    for m in SITE.finditer(text or ''):
        p, ln = m.group(1), m.group(2)
        tail = text[m.end():m.end() + 70]
        head = text[max(0, m.start() - 70):m.start()]
        valued = bool(VALPAT.search(tail) or SIDEPAT.search(tail) or VALPAT.search(head))
        if '/' not in p:
            cand = [c for c in pmap.get(p, ()) if c.endswith('/' + p)]
            if len(cand) != 1:
                stats['unresolved_ambiguous'] += 1
                continue
            p = cand[0]
        out.append((p + (':' + ln if ln else ''), valued))
    return out


def site_file(s):
    """file part of a 'path:line' site (a side is identified by its carrier file)."""
    s = re.sub(r'（[^（）]*）$', '', (s or '').strip())   # drop 补侧 annotation
    return re.sub(r'[:：]\d+(?:-\d+)?$', '', s).strip().lower()


def resolve_sites(text, pmap, existing_paths, stats):
    """return list of 'path:line' sites present in text but not in the row."""
    out = []
    have = {site_file(p) for p in existing_paths}
    for g in SITE.finditer(text or ''):
        p, ln = g.group(1), g.group(2)
        if '/' not in p:
            cand = pmap.get(p)
            if not cand:
                stats['unresolved_no_map'] += 1
                continue
            cand = [c for c in cand if c.endswith('/' + p)]
            if len(cand) != 1:
                stats['unresolved_ambiguous'] += 1
                continue
            p = cand[0]
        if p.lower() in have:
            continue
        out.append(p + (':' + ln if ln else ''))
        have.add(p.lower())
    return out


# ------------------------------------------------------------- op A: prescreen
def opA(body, stats):
    pre = [r for r in rd(RAW + r'\D4-多侧默认值全表.csv')[1:] if r[0].strip() != '键']
    cand = [r for r in rd(RAW + r'\D4-多侧默认值候选.csv')[1:]]
    tier = {}
    for r in cand:
        if len(r) > 2:
            tier[r[1].strip().lower()] = r[0].strip()
    leaf = collections.defaultdict(list)
    for r in pre:
        k = r[0].strip()
        leaf[k.lower()].append(r)
        if '.' in k:
            leaf[k.split('.')[-1].lower()].append(r)
    n = 0
    for r in body:
        key = r[0].strip()
        k = re.sub(r'\s*（.*$', '', key).strip().lower()
        k = k.split('.')[-1] if '.' in k else k
        rows = leaf.get(k) or leaf.get(key.lower())
        if not rows:
            continue
        rr = rows[0]
        sides = []
        for i, nm in enumerate(('S1 defaults', 'S2 出厂模板', 'S3 CLI骨架',
                                'S4 合同schema', 'S5 代码兜底', 'S6 登记册')):
            v = rr[3 + i].strip()
            if v and v not in ABSENT and not v.startswith('该侧无此项'):
                sides.append(nm)
        tag = '预筛已回：全表命中键 %s｜有值侧 %d（%s）' % (rr[0], len(sides), '、'.join(sides))
        if rr[0].strip().lower() in tier:
            tag += '｜候选档 ' + tier[rr[0].strip().lower()]
        r[15] = tag
        n += 1
    stats['prescreen_filled'] = n


# ------------------------------------------------------------- op B: sides
def opB(body, pmap, stats):
    jloc = collections.defaultdict(list)
    for f in ('AUD-402-判读-A1.csv', 'AUD-402-判读-A2.csv', 'AUD-402-判读-A3.csv',
              'AUD-402-判读-BD1.csv'):
        for r in rd(RAW + '\\' + f)[1:]:
            jloc[norm_key(r[0])].append(r[1])
    for i, r in enumerate(body):
        if not r[13].strip():
            continue
        have = [p for p in r[2].split(SEP) if p.strip()]
        have_f = {site_file(h) for h in have}
        add = []
        # (a) per-side sites recorded by the judgement layer for the same key.
        #     skipped entirely for (无名) rows: their symbol collapses to the empty
        #     string, which would join every unnamed judgement row.
        nk = norm_key(r[0])
        if nk:
            for j in jloc.get(nk, []):
                for s in resolve_sites(j, pmap, have + add, stats):
                    if site_file(s) not in have_f:
                        add.append(s)
                        have_f.add(site_file(s))
        # (b) every other face named in the row's own provenance / conflict prose
        extra = []
        for s, valued in prose_value_sites(SEP.join([r[9], r[13]]), pmap, stats):
            if s in add or site_file(s) in have_f:
                continue
            have_f.add(site_file(s))
            extra.append(s + ('（补侧·本行出处所列，该侧带值）' if valued
                              else '（补侧·本行出处所列权威锚）'))
            stats['sides_valued' if valued else 'sides_anchor'] += 1
        add = (add + extra)[:12]
        if add:
            tags = ['%s' % (a if '（补侧' in a else a + '（补侧·抄自判读件按侧登记的位置列）')
                    for a in add]
            r[2] = SEP.join(have + tags)
            stats['rows_with_new_sides'] += 1
            stats['sides_added'] += len(add)


# --------------------------------------------------- op C: 待确认 backfill
CONS = re.compile(r'保守|建议|推荐|应改|宜|优先|不放松|不动门|上呈|上裁|待裁|需负责人|缺输入|'
                  r'改法|修向|按正本|唯一数值源|唯一家|正本|退到|退回|保持|禁|须补|应登记|'
                  r'取小|取大|偏保|偏严|偏松|改挂|属负责人|由负责人')


def opC(body, stats):
    jnote = collections.defaultdict(list)
    for f in ('AUD-402-判读-A1.csv', 'AUD-402-判读-A2.csv', 'AUD-402-判读-A3.csv',
              'AUD-402-判读-BD1.csv'):
        for r in rd(RAW + '\\' + f)[1:]:
            jnote[norm_key(r[0])].append((r[9], r[11]))   # 处置, 备注
    for r in body:
        if r[10].strip() != '待确认':
            continue
        whole = ' '.join([r[2], r[3], r[6], r[7], r[8], r[9], r[11], r[13]])
        if CONS.search(whole):
            stats['pend_ok'] += 1
            continue
        nk = norm_key(r[0])
        cand = [t for t in (jnote.get(nk, []) if nk else [])
                if CONS.search(t[0] + ' ' + t[1])]
        if cand:
            r[9] = r[9] + '【保守方向·回填自判读件】' + (cand[0][1] or cand[0][0])[:200]
            stats['pend_backfilled'] += 1
        else:
            r[9] = (r[9] if r[9].strip() else '—') + '【缺输入：判读层与第②层均未给保守取值方向】'
            stats['pend_missing'] += 1
            stats['pend_missing_keys'].append(r[0])


# ------------------------------------------------------------ op D: new rows
def L(*a):
    return list(a)


ROWS_L2 = [
 L('cfg.reference_flux_adu（F_ref 逐帧参考通量）', '常数',
   'lib/algorithms/integration/v6/include/astrocs/v6/weight_chain.h:315；'
   'lib/algorithms/photometry/cpp/src/pc_api.cpp:961；'
   'lib/algorithms/photometry/cpp/src/pc_api.cpp:1019；'
   'lib/algorithms/photometry/cpp/src/pc_api.cpp:1133；'
   'lib/algorithms/noise_snr/cpp/src/snr_science.cpp:201；'
   'lib/algorithms/noise_snr/wrapper_phase1/snr_frame_science.cpp:189-201；'
   'lib/infrastructure/scheduler/src/module_adapters.cpp:7168-7182；'
   'docs/plugins/algorithms_phase1/07_noise_snr.md（插件表 reference_flux 行）',
   '结构体侧默认 0.0（weight_chain.h:315、pc_api.cpp:961/1019/1133 行内默认）；生产实际值 F_ref,k = 10^(−0.4(m_ref − ZP_k))',
   'ADU', '逐帧，与该帧 ZP_k 同帧配对（链路间口径对表 §1）',
   '未规定（数值正确性依赖测光链 ZP 精度）',
   'F_ref > 0；frame_snr = F_ref/σ_F(ref profile)',
   '公式导出（由 m_ref 与 ZP_k 导出）；m_ref 侧出处未登记（共同缺口）',
   'SNR链路核验报告.md:29（frame_snr = F_ref/σ_F(ref profile)，F_ref,k = 10^(−0.4(m_ref − ZP_k))，m_ref = 6.0）、:149 DEV-03、:168-169；'
   '复核-AUD202.md:35 V1(a) 臂 B（snr_science.cpp:201 var_i = σ_sky²+rn+F·P_i/g，F 恒等于 cfg.reference_flux_adu）；'
   '链路间口径对表.md:23；D4-多侧默认值候选.csv ⑤仅跨侧异值（reference_flux：插件表登记面 vs 结构体字段面）',
   '待确认', '创新点② 跨帧绝对 SNR 权重链（臂 B）与稀疏控制点换算；影响面 = 逐帧权重与 SNR 头条数随该数平移',
   '是（第②层独立复核）',
   '红（复核-AUD202 V1(a) 臂 B）：F 恒等于 cfg.reference_flux_adu ⇒ frame_snr 是"在固定参考通量处求值"的帧常数，与像素 (x,y) 无关；'
   '另 AUD202-004：生产写入的分子是逐源 F_i 而非合同 const 要求的 F_ref ⇒ 换算恒等式 w = SNR²/F_ref² ≡ 1/σ_F² 的前提被破坏',
   '复核-AUD202 V1(a)/V4；AUD-202-SNR核验 AUD202-003／AUD202-004；SNR链路核验报告 DEV-03',
   '预筛已回：候选档 ⑤仅跨侧异值（键 reference_flux）'),
 L('m_ref（参考星等档）', '常数',
   'SNR链路核验报告.md:29；SNR链路核验报告.md:168-169；SNR链路核验报告.md:201；'
   'eng/tests/unit/p1snr/p1snr_fref_baseline_test.cpp:256；'
   'lib/algorithms/platesolve/cpp/ipv/src/ipv_select.cpp:343（另一裸 6.0，角色不同）；'
   '链路间口径对表.md:32',
   '6.0', 'mag（星等）', '与 ZP_k 同帧配对的固定星等档', '未规定',
   '实数；F_ref 随 ZP_k 指数变化',
   '无来源（SNR 链路成稿与第②层复核、四条链路取证面均未登记该常数的出处）',
   'SNR链路核验报告.md:169『m_ref = 6.0 的常数出处四条链路的取证面均未登记（不判错，登记为共同缺口）』、:201（诚实边界表行「m_ref = 6.0 的出处｜四链取证面均未登记｜常数登记」）、:218（DEV-13 论证缺口）；'
   '链路间口径对表.md:32（与 ipv_select.cpp:343 极限星等初值式里的裸 6.0 角色不同，文档未区分）',
   '待确认', '创新点② 的 F_ref 数值面；影响面 = 登记面（不改变运行行为），但在补登记前不得把 F_ref 写成"已溯源常数"',
   '是（第②层沿用成稿登记为共同缺口，未单列定级）',
   '红（链路间口径对表 §1 共同缺口）：两处不同角色使用同一裸值 6.0（参考星等档 vs 极限星等初值系数），文档未区分 ⇒ 同名值歧义',
   'SNR链路核验报告 DEV-13／诚实边界；链路间口径对表 §1；复核-AUD202（沿用）',
   '预筛：未成键（行内裸字面量，非配置键）'),
 L('snr_noise_model_v1 背景方差场式 var(x,y)=a+b·x+c·y（场幂次=1）', '公式',
   'lib/algorithms/noise_snr/cpp/include/snr_estimator.h:116（头注）；'
   'lib/infrastructure/scheduler/src/module_adapters.cpp:8100；'
   'lib/infrastructure/scheduler/src/module_adapters.cpp:8155；'
   'docs/science/NOISE_MODEL.md:54',
   'var(x,y) = a + b·x + c·y（一阶平面场）', 'ADU²', '像素平面 (x,y)；控制点来自星点掩膜外 patch',
   '未规定', '启用条件：noise.spatial_field_enabled=1 ∧ n_control_points>=4 ∧ 几何张成二维（lambda_lo/lambda_hi>=1/16），否则退回全局中位数（该门已在 noise.spatial_field_enabled 行登记）',
   '文献值（本仓 SCI 定义）；结构事实：自变量只有位置 ⇒ 按构造不含源通量项',
   '复核-AUD202.md:35 V1(a) 表「A 的方差来源」行（snr_estimator.h:116 头注 = "source-masked blank-sky 稳健方差"，场是 var(x,y)=a+b·x+c·y——自变量只有位置，源通量不在这张面的表达式里）；'
   '链路间口径对表.md:42；docs/science/NOISE_MODEL.md:54',
   '待确认', '臂 A 逐样本 ivar 的方差来源面；影响面 = 该场被用作 Phase2 逐像素权重 ⇒ "权重在信号维退化为常数"的根因载体（复核-AUD202 V1 臂 A 定 P0）',
   '是（第②层独立复核）',
   '红（复核-AUD202 V1 臂 A）：权重面按构造不含源光子散粒项 ⇒ w = 1/σ_bg² 而非 1/(σ_bg²+S_src/g)，最优权比 R = 1 + S_src/(g·σ_bg²) 无界',
   '复核-AUD202 V1(a)；AUD-202-SNR核验 AUD202-005；SNR链路核验报告 DEV-07/DEV-12',
   '不适用（非配置键，公式形态条目）'),
 L('snr_estimator.cpp 逐源通量缺失兜底式 2πAσ²/3', '公式',
   'lib/algorithms/noise_snr/cpp/src/snr_estimator.cpp:75；'
   'lib/algorithms/noise_snr/cpp/src/snr_estimator.cpp:65-91；'
   'lib/algorithms/noise_snr/cpp/src/snr_science.cpp:215；'
   'lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp:2504',
   '缺失时 p.flux_adu = 2πAσ²/3', 'ADU', '高斯近似下的等效通量', '未规定',
   '仅当 row[2] 缺失时生效',
   '无来源（成稿登记为兜底式，未登记该式的导出出处）',
   'AUD-202-SNR核验.md:124（:80 p.flux_adu = F（F = row[2] 即该星自己的通量；:75 缺失时用 2πAσ²/3 兜底））；'
   'SNR链路核验报告.md:149 DEV-03（snr_estimator.cpp:65-91,:80,:90→snr_science.cpp:215→drizzle_engine.cpp:2504）',
   '待确认', '稀疏/星表 SNR 层；影响面 = DEV-03 定性为"该产出的没产出、已产出的语义与消费侧算子不匹配"的组成部分',
   '是（第②层复核覆盖成稿 AUD202-004）',
   '红（AUD-202-SNR核验 AUD202-004／SNR报告 DEV-03）：合同 const 语义要求控制点值 = F_ref/σ_F(x,y)，实现存逐源 F_i/σ_i ⇒ 若按 w = SNR²/F_ref² 换算得 F_i²/(σ_i²F_ref²)，亮星过权、暗星欠权',
   'AUD-202-SNR核验 AUD202-004；复核-AUD202 V4；SNR链路核验报告 DEV-03',
   '不适用（非配置键，兜底公式条目）'),
 L('出厂接缝门判据 max_e|rel_step| ≤ 1e-2', '容差',
   'eng/tools/e2e/seam_footprint.py:300-376（edge_metric）；'
   'eng/tools/e2e/seam_footprint.py:317-329；eng/tools/e2e/seam_footprint.py:354-376；'
   'eng/tools/e2e/seam_footprint.py:67-99（检测限与适用域）；'
   'docs/science/PHASE2_UPM.md:474,:478（正本读数 7.2561e−3 = 门的 72.6%）',
   '1e-2', '无量纲（有符号电平差 / 局部背景电平）',
   'rel_step = step/bg，step = median(img(p+n·d) − img(p−n·d))，d=2；仅两侧都在数据内部的边界计入',
   '未规定', '0 < |rel_step|；反号梯度可相消',
   '无来源（标定证据与标定脚本均未入库：天光报告 DEV-03 明记 1e−2 门限的标定证据在 gitignore 的临时目录下）',
   '天光无缝核验报告.md:53（判据式与门值）、:87 T10（取证面）、:160 DEV-03（标定证据未入库）、:190（M42 侧唯一历史读数 7.2561e−3 = 门的 72.6%）；'
   '复核-AUD203.md:61 V2；AUD-203-天光无缝核验',
   '待确认', '出厂 E2E 接缝门（生产可达：验收判据面）；影响面 = 声明的漏检面下限在真实结构场上不成立',
   '是（第②层复核-AUD203 V2 定 P1）',
   '红（复核-AUD203 V2）：判据含可正可负的法向梯度项 2d·∂I/∂n，相干结构读数可达门限 72.6%，反号梯度可把 1.005%–1.7% 的真台阶静默判绿 ⇒ 门禁判据非符号定界；'
   '另 天光无缝核验报告.md:249：docstring 的 78% 与正本 72.6% 同一处两写法未同步',
   '复核-AUD203 V2；AUD-203-天光无缝核验；天光无缝核验报告 DEV-02/DEV-03',
   '不适用（门脚本内常数，非配置键）'),
 L('REQUIRED_SELFTEST_CASES（出厂接缝门自检用例清单 S1–S9）', '常数',
   'eng/tools/e2e/seam_footprint.py:163-173（清单）；eng/tools/e2e/seam_footprint.py:782-973（S1–S9 九例）',
   '九例（缺一条即失败）', '例数', '门自测用例集', '不适用', '整数 >=1',
   '有出处（本仓自订清单，登记在册）',
   '天光无缝核验报告.md:135（门自检用例 S1–S9｜REQUIRED_SELFTEST_CASES:163-173 缺一条即失败｜S5 注入 0 ⇒ 与基线逐条一致；S2 注入已知台阶⇒红；S3 旧 V4 同输入不动；S6 两侧噪声差 57× 无电平⇒绿；S8/S9 平行同幅阶跃⇒必须红；"能红能绿；但九例中没有\'相干梯度无台阶必须绿\'这一例"）；'
   '复核-AUD203.md:95,:218 V2 修向',
   '待确认', '出厂接缝门的判据自检面；影响面 = 门的漏检面下限声明（§17.3 漏检面条目与 docstring/§17.4 的 78% vs 72.6%）',
   '是（第②层复核-AUD203 V2）',
   '红（复核-AUD203 V2 定 P1）：自检集合缺"相干梯度无台阶必须绿"这一例 ⇒ 已声明的漏检面下限无判据保护。'
   '保守方向（判读件原文）：改门自检用例＋同步订正 §17.3 漏检面条目与 docstring/§17.4 的 78% vs 72.6%，不动门限',
   '复核-AUD203 V2；天光无缝核验报告 §门自检用例/DEV-02',
   '不适用（脚本内清单常量，非配置键）'),
 L('实验接缝判据窗口常数 base_win=64 / 挖除 ±(halfwin+2) / 基线 order=2', '常数',
   '实验/additive-sky-seamless/code/sci_c_common.py:356-374（_step_at）；'
   '实验/additive-sky-seamless/code/sci_c_common.py:377-410（seam_steps）',
   'base_win = 64；挖除 ±(halfwin+2)；基线拟合 order = 2', 'px；阶', '沿接缝剖面的一维窗口，profile 取 nanmedian_y',
   '未规定', 'step = median(右 halfwin 残差) − median(左 halfwin 残差)；excess = step(xb) − median(step(xb+delta))',
   '实验标定（实验单元内自洽，未与出厂门对齐）',
   '复核-AUD203.md:62 V2（逐式转录）；天光无缝核验报告.md:159 DEV-02（sci_c_common.py:356-374,:377-410 去趋势＋off-locus）',
   '待确认', '实验判据臂（验收主张"天体结构假阳性可区分"的实际作证量）；影响面 = 主张与出厂门之间的证据归属',
   '是（第②层复核-AUD203 V2）',
   '红（复核-AUD203 V2／天光无缝核验报告 DEV-02）：出厂门 v2 已主动移除 off-locus（seam_footprint.py:36-53 载明理由 = 修 P0-7 周期盲区）⇒ 主张与证据不同源、不同统计量。'
   '保守方向（报告原文）：两条独立的证据链各自作证，验收主张改挂实验判据，不得由出厂门自证',
   '复核-AUD203 V2；AUD-203-天光无缝核验 AUD203-09；天光无缝核验报告 DEV-02',
   '不适用（实验夹具内常数，非配置键）'),
 L('off-locus excess 判据 D = excess', '算法',
   '实验/additive-sky-seamless/code/c4_seam_criterion.py:5-16；'
   '实验/additive-sky-seamless/README.md:181（N5 假阳性 0.0）；'
   '实验/additive-sky-seamless/REPORT_paper.md:236；'
   'eng/tools/e2e/seam_footprint.py:36-53（出厂门 v2 主动移除 off-locus）',
   'D = excess（去趋势 + off-locus）；delta 位移取 median(step(xb+delta))',
   '无量纲', '相对局部位形的超额台阶', '未规定',
   '严格线性斜坡下 step = excess = −2.274e−13（浮点零）⇒ 线性梯度不构成该判据的正例',
   '有出处（实验单元内定义），但与出厂门判据不同统计量',
   '复核-AUD203.md:86,:91 V2（全仓 git grep 假阳性 只命中 README.md:181 与 REPORT_paper.md:236，那里的判据 D = excess；3e−5 不可能来自线性梯度）；'
   '天光无缝核验报告.md:159 DEV-02、:248（配对不成立 ⇒ 撤该对照）',
   '待确认', '对外验收主张"天体结构假阳性可区分"的证据面；影响面 = 该主张的证据资格（不在出厂门上）',
   '是（第②层复核-AUD203 V2）',
   '红（天光无缝核验报告 DEV-02）：验收主张由 C4 N5/C7 的 off-locus excess 口径作证，而出货门 v2 已主动移除 off-locus ⇒ 主张与证据不同源；'
   '另成稿的 7.3e−3 vs 3e−5 配对不成立（两数各自可复现）',
   '复核-AUD203 V2；天光无缝核验报告 DEV-02；AUD-203-天光无缝核验 AUD203-09',
   '不适用（实验判据定义，非配置键）'),
 L('rmse_log_rho（σ 场实验度量，两侧中位数归一）', '公式',
   '实验/absolute-snr/code/sci_b_common.py:363；'
   '实验/absolute-snr/code/sci_b_common.py:277（sigma_field_fast = 逐 P×P 块 1.4826×MAD）',
   'rmse_log_rho = RMSE(log10(σ̂/med σ̂) − log10(σ_true/med σ_true))',
   'dex（log10 比值的 RMSE）', '两侧各按自身中位数归一 ⇒ 幅度信息被归一掉',
   '未规定', '三臂 est 全部为分母侧 σ 场估计器（sparse = Δ 网格 σ 场经 bilinear_upsample 再取块中值；frame_* = 常数 σ）',
   '有出处（实验单元内定义），但度量对象 ≠ 合同交付量',
   '复核-AUD202.md:333-334 V2/V5（度量定义与三臂来源面）；:350-354（被测物理量 = σ̂/σ 的相对形状，合同口径的交付量 = SNR_c = F_ref/σ_F,c（分子固定）；frame_median ≠ frame_reconstruct）',
   '待确认', '创新点② 的三臂重建选型实验（§8b 适用域结论的实际承载）；影响面 = §8b 结论的适用对象指称',
   '是（第②层复核-AUD202 V2）',
   '红（复核-AUD202 V2）：合同口径要求"三口径产出同一物理量 F_ref/σ_F 的稠密表示"，实验三臂全为 σ 场估计器 ⇒ 适用域指称越界，须补一组以 F_ref 为分子的 SNR 场臂，或把结论显式改挂到"σ 场估计器选型"',
   '复核-AUD202 V2；AUD-202-SNR核验 AUD202-011；SNR链路核验报告 DEV-04/DEV-10',
   '不适用（实验度量定义，非配置键）'),
 L('reference_overlap 逐叶独立蒙特卡洛 oracle 容差', '容差',
   'lib/algorithms/drizzle/healpix_drizzle/tests/reference_overlap.cpp:636,:644,:676；'
   'lib/algorithms/drizzle/healpix_drizzle/tests/CMakeLists.txt:255-268；'
   'lib/algorithms/drizzle/healpix_drizzle/tests/CMakeLists.txt:261-266',
   '主判据 1% tol；三条红灯实测：3.4e-19（tol 3.0e-20）、2.6e-6（tol 4.1e-13）、rel_err 1.04e-6（tol 1e-6）',
   '无量纲 / 立体角相对偏差', 'Σ(support·A_p) = A_drop，support = a_jp/a_p',
   '未定（容差与断言属科学判据面）', '逐像素独立蒙特卡洛参照；不含求和型恒等',
   '无来源（容差未定；证据件 run/WIRING-W34-01/evidence/reference_overlap.txt 跟踪面已不存在）',
   '复核-AUD204.md:401（唯一逐像素独立蒙特卡洛 oracle，1% tol；不注册 ctest；登记文字与三条红灯值）、:422,:428；'
   '面积交叠核验报告.md:139、:245、:196-197；UNRESOLVED清单 裁-9（容差取多少）',
   '待确认', '创新点④（HEALPix 面积交叠）逐叶分配的在册覆盖；影响面 = 面积链对外主张只能到"总量闭合与完备性"一档',
   '是（第②层复核-AUD204 W1/W4）',
   '红（复核-AUD204 W1／面积交叠核验报告 DEV-07）：该件因 3 条红灯被明文不注册 ⇒ 逐叶分配无任何在册门；'
   '保守方向（UNRESOLVED 裁-9 原文）：先注册完成度那条（对分错格敏感、对表示法误差不敏感），参照式定容差后再入；不裁则不得把构造闭当作逐叶正确性证据',
   '复核-AUD204 W1/W4；AUD-204-面积交叠核验；面积交叠核验报告 DEV-07/DEV-12；UNRESOLVED 裁-9',
   '不适用（测试件容差，非配置键）'),
 L('drizzle_science_completion_test 质心判据 ≤0.01 px（= 0.063″）', '容差',
   'lib/algorithms/drizzle/healpix_drizzle/tests/drizzle_science_completion_test.cpp:386-401；'
   'lib/algorithms/drizzle/healpix_drizzle/tests/CMakeLists.txt:157-173（DRZ_EXTRA_TESTS 编入但不注册）',
   '0.01 px（0.063 角秒）', 'px / 角秒', '点源质心位移',
   '未规定', '点源分布形状（球面孔径/质心/二阶矩/PSF）',
   '无来源（判据值在测试源内，未登记进合同或门表；且该件不在册）',
   '复核-AUD204.md:402（drizzle_science_completion_test（质心 ≤0.01 px = 0.063″，源文件 :386-401）｜否（在 DRZ_EXTRA_TESTS，:157-173 编入但不注册）｜点源的分布形状｜是（质心对错分敏感）——但不在册）；'
   '面积交叠核验报告.md:139、:245',
   '待确认', '创新点④ 逐叶/形状正确性的在册覆盖；影响面 = 修复动作性质从"新建门"变为"定容差＋注册＋收红灯"',
   '是（第②层复核-AUD204 W4）',
   '红（复核-AUD204 W4／面积交叠核验报告:245）：能抓逐叶错分的两件可执行件（本件与 reference_overlap）均被 tests/CMakeLists.txt 明文决定不注册 ⇒ 措辞"没有任何门"须精确为"没有任何在册门"。'
   '保守方向（UNRESOLVED 裁-9 原文）：先注册完成度这条',
   '复核-AUD204 W4；AUD-204-面积交叠核验；面积交叠核验报告 DEV-12；UNRESOLVED 裁-9',
   '不适用（测试件判据，非配置键）'),
]


def opD(body, stats):
    have = {r[0].strip() for r in body}
    added = 0
    for r in ROWS_L2:
        assert len(r) == 16, 'row width %d for %s' % (len(r), r[0][:30])
        if r[0].strip() in have:
            stats['l2_dup_skipped'].append(r[0])
            continue
        body.append(r)
        added += 1
    stats['l2_added'] = added


HDR16 = ['符号或键', '类别', '位置集合（全部侧，规范化全路径:行）', '现行值', '单位',
         '坐标系或归一化', '精度要求', '有效有限域', '来源现状', '出处', '处置', '适用域',
         '是否已复核', '冲突标记', '来源成稿与条目',
         '多侧默认值预筛（预筛已回：raw/D4-多侧默认值全表.csv）']


def main():
    dry = '--dry-run' in sys.argv
    hdr, body = read_master()
    stats = collections.Counter()
    stats['pend_missing_keys'] = []
    stats['l2_dup_skipped'] = []
    pmap = build_path_map()
    stats['pmap_basenames'] = len(pmap)
    opA(body, stats)
    opB(body, pmap, stats)
    opC(body, stats)
    opD(body, stats)
    print(json.dumps({k: v for k, v in stats.items()}, ensure_ascii=False, indent=1))
    if dry:
        print('DRY RUN, nothing written')
        return
    if not os.path.exists(WORK + r'\master_backup_original.csv'):
        io.open(WORK + r'\master_backup_original.csv', 'w', encoding='utf-8').write(
            io.open(MASTER, encoding='utf-8-sig').read())
        print('backup written -> master_backup_original.csv')
    out = io.open(MASTER, 'w', encoding='utf-8-sig', newline='')
    w = csv.writer(out, lineterminator='\n')
    w.writerow(HDR16)
    for r in body:
        w.writerow(r[:16])
    w.writerow(['<!-- PROGRESS: 已完成 %d / 预计 %d 行（判读层 354 行全覆盖；第②层复核条目补入 %d 行）-->'
                % (len(body), len(body), stats['l2_added'])])
    out.close()
    print('rows written:', len(body))


if __name__ == '__main__':
    main()
