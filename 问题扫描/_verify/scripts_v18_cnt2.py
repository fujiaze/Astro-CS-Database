
import json,re
ROOT="/workspace/Astro CS Database"
h=json.load(open(ROOT+"/问题扫描/_verify/scripts_v18_macros.json",encoding="utf-8"))
from collections import Counter
c=Counter(x["name"] for x in h)
for n in ["CHECK_ST","EXPECT_STR","EXPECT","ASSERT_TRUE","ASSERT_FALSE","TEST_CHECK","AIO_CHECK","RUNTIME_CHECK","VERIFY"]:
    print(n, c.get(n,0))
cs=[x for x in h if x["name"] in ("CHECK_ST","EXPECT_STR")]
print("CHECK_ST+EXPECT_STR files:",len(set(x['file'] for x in cs)))
for x in cs: print("   ",x["file"],x["line"])
