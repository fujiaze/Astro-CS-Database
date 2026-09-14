# -*- coding: utf-8 -*-
"""V15 只读验证器：
 (1) rt005 test_kernel_ids_authoritative 是否恒真（正则交替名集合 == 白名单集合）
 (2) evidence/v6_1_rework/TASK_LEDGER.csv 里 P3-006 行的真实状态列（对照 test_05 只断 ID 在册）
 (3) core_pipeline_test.cpp VALID 常量是否自含 bilinear（②自证）
 (4) test_cpu_profile.py 的探针源是谁写的（②自证判定）
"""
import re, json, csv, os
ROOT="/workspace/Astro CS Database"
t=open(os.path.join(ROOT,"tests/runtime/test_rt005_plan_estimator.py"),encoding="utf-8").read()
m=re.search(r'_KNOWN_KERNELS = \{(.*?)\n\}', t, re.S)
known=set(re.findall(r'"([^"]+)"', m.group(1)))
i=t.index("def test_kernel_ids_authoritative")
body=t[i:i+900]
rx_blob=re.sub(r"[\r\n\s'\"]+","", body[body.index("re.finditer"):body.index("self.assertIn")])
alt=set(re.findall(r'([a-z][a-z0-9\-]*\|[a-z0-9\-]+|[a-z][a-z0-9\-]+)', rx_blob.replace("r(","").replace(")","")))
# 更稳：直接从原始多行字符串里抠交替
seg = body[body.index('r"((') if 'r"((' in body else body.index('r"'):]
seg2 = seg[:seg.index('text')].replace('r','').replace('"','').replace('(','').replace(')','')
alt2 = set(x for x in re.split(r'[|]', re.sub(r'[^a-z0-9|\-]','', seg2)) if x)
print("(1) 白名单集合大小:", len(known))
print("(1) 正则交替集合大小:", len(alt2), sorted(alt2)==sorted(known) and "与白名单完全相同 ⇒ assertIn 恒真" or "与白名单不同")
print("(1) 差集 known-alt2:", known-alt2, " alt2-known:", alt2-known)
print("     权威 .inc 里的真实 id 数与集合:")
inc=open(os.path.join(ROOT,"lib/backend_host/backend_table.inc"),encoding="utf-8").read()
real=set(re.findall(r'ACS_KERNEL_ENTRY\("[^"]+",\s*"([^"]+)"', inc))
print("     .inc 实测 id:", len(real), " 与白名单差集 real-known:", real-known, " known-real:", known-real)

rows=list(csv.reader(open(os.path.join(ROOT,"evidence/v6_1_rework/TASK_LEDGER.csv"),encoding="utf-8")))
hdr=rows[0]
print("\n(2) TASK_LEDGER 表头:", hdr)
for r_ in rows[1:]:
    if r_ and r_[0]=="P3-006":
        print("    P3-006 行:", dict(zip(hdr,r_)))
        break

c=open(os.path.join(ROOT,"tests/unit/core_pipeline_test.cpp"),encoding="utf-8").read()
j=c.index("static const char* VALID")
print("\n(3) VALID 常量原文:", c[j:j+520].split(";")[0][:520])

p=open(os.path.join(ROOT,"tests/backend/test_cpu_profile.py"),encoding="utf-8").read()
k=p.index("def test_08") if "def test_08" in p else p.index("src_path")
print("\n(4) test_cpu_profile 探针写入片段:")
print(p[p.index("src_path")-1200:p.index("avail=2 backend=baseline workers=2")+60][-2600:])
PY2=None
