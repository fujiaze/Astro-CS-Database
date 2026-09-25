# -*- coding: utf-8 -*-
# 第②层复核 V402 / 派单三问之③：D4 三个数（机械层 20,595 · 判读层 354 · 主表 410）自洽性
# 只读 CSV 文本，不 import 仓库内任何模块（cwd 已切到 独立审计/复算件/V402，避开仓库根 csv.py 遮蔽）
import csv, io, os, sys
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')
B = r"产出/"

def recs(rel):
    with open(os.path.join(B, rel.replace('/', os.sep)), encoding='utf-8-sig', newline='') as f:
        r = list(csv.reader(f))
    return (r[0] if r else []), (r[1:] if r else [])

def wc_lines(rel):
    with open(os.path.join(B, rel.replace('/', os.sep)), encoding='utf-8', newline='') as f:
        return f.read().count('\n') + 1

MECH = 'raw/AUD-402-常数台账.csv'
SIDE = 'raw/AUD-402-常数台账.旁表-无名值与测试钉值.csv'
MAIN = '独立审计/02_科学/常数公式算法总台账.csv'

for rel in (MECH, SIDE, MAIN):
    h, rows = recs(rel)
    print("文件 %s" % rel)
    print("   物理行数(wc 口径) = %d ；csv 记录数(含表头) = %d ；数据行 = %d" % (wc_lines(rel), len(rows) + 1, len(rows)))
    print("   列数=%d 列头=%s" % (len(h), h[:8]))
    bad = [i for i, r in enumerate(rows, 2) if len(r) != len(h)]
    print("   列数不等于表头的行: %d 处 %s" % (len(bad), bad[:5]))

print("\n=== 旁表 与 机械层主表 的键重合（键 = 前2列，即 (符号/键, 位置)） ===")
mh, mrows = recs(MECH)
sh, srows = recs(SIDE)
def k2(rows):
    return [tuple(r[:2]) for r in rows]
km, ks = set(k2(mrows)), set(k2(srows))
print("   主表键(唯一)=%d 旁表键(唯一)=%d 旁表键∩主表键=%d 旁表键−主表键=%d" % (len(km), len(ks), len(ks & km), len(ks - km)))
print("   主表数据行=%d 旁表数据行=%d 二者并集(行集合)=%d" % (
    len(mrows), len(srows), len(set(map(tuple, mrows)) | set(map(tuple, srows)))))
print("   旁表独有键样例: %s" % sorted(ks - km)[:5])
print("   整行原文交集条数=%d" % len(set(map(tuple, mrows)) & set(map(tuple, srows))))

print("\n=== 判读层 ↔ 机械层主表 ↔ D4 主表 的映射 ===")
J = ['raw/AUD-402-判读-A1.csv', 'raw/AUD-402-判读-A2.csv', 'raw/AUD-402-判读-A3.csv', 'raw/AUD-402-判读-BD1.csv']
jrows = []
for rel in J:
    h, rows = recs(rel)
    jrows += rows
    print("   %-30s 数据行=%4d 表头=%s" % (rel, len(rows), h))
print("   判读层合计数据行 = %d" % len(jrows))
jk = set(tuple(r[:2]) for r in jrows)
print("   判读层唯一 (符号/键,位置) = %d" % len(jk))
mk = set(k2(mrows))
print("   判读层键 ∩ 机械层键 = %d ；判读层键 − 机械层键 = %d" % (len(jk & mk), len(jk - mk)))
print("   判读层样例键: %s" % [list(x) for x in list(jk)[:3]])
print("   机械层样例键: %s" % [list(x) for x in list(mk)[:3]])

print("\n=== D4 主表(410 主张) 的分母核对 ===")
dh, drows = recs(MAIN)
print("   数据行=%d 列数=%d" % (len(drows), len(dh)))
print("   列头=%s" % dh)
if '类别' in dh:
    i = dh.index('类别')
    from collections import Counter
    print("   类别分布: %s" % Counter(r[i] for r in drows if len(r) > i))
bk = [c for c in ('对象', '类型') if c in dh]
# 与机械层/判读层的可加性：看主表是否含"机械行号"列
print("   含机械层行指针的列: %s" % [c for c in dh if '机械' in c or '位置' in c or '行' in c])

print("\n=== 台账自报四桶算术（读者件 §1.2） ===")
buckets = {'测试钉值': 12205, '具名未判': 4757, '无名行内': 3221, '具名已判': 412}
s = sum(buckets.values())
print("   四桶合计 = %d ；读者件主张机械层主表 = 20595 ；差 = %d" % (s, s - 20595))
print("   不逐条判读三桶合计 = %d ；412/20595 = %.4f%% ；354 判读行→412 机械行 平均 %.2f 行/判读" % (
    s - 412, 412 / 20595 * 100, 412 / 354))
