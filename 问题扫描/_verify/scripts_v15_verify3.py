# -*- coding: utf-8 -*-
import re, os
ROOT="/workspace/Astro CS Database"
p=os.path.join(ROOT,"lib/core/src/plan_estimator.cpp")
t=open(p,encoding="utf-8").read()
ids=re.findall(r'"([a-z][a-z0-9]*-[a-z0-9\-]+)"', t)
print("plan_estimator.cpp 里所有 kebab 字面量:", sorted(set(ids)))
# 也扫 header
h=open(os.path.join(ROOT,"include/astrocs/core/plan_estimator.h"),encoding="utf-8").read()
print("\n头里的常量:", re.findall(r'constexpr[^;]+;', h)[:12])
print("\n头里 kebab:", sorted(set(re.findall(r'"([a-z][a-z0-9]*-[a-z0-9\-]+)"', h))))
PY4=None
