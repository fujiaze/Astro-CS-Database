import io, os, re

R = r"独立审计/证据"
D = r"独立审计/复算件/d4merge"

# (file, start, end, label)
SPANS = [
    ("AUD-201-测光核验.md", 7, 27, "AUD201 §1 链路口径与公式"),
    ("AUD-201-测光核验.md", 57, 67, "AUD201-002 λ因子"),
    ("AUD-201-测光核验.md", 157, 179, "AUD201-008 六条测光常数死键"),
    ("AUD-201-测光核验.md", 209, 259, "AUD201-011/012/013"),
    ("AUD-201-测光核验.md", 293, 334, "AUD201-016/017"),
    ("复核-AUD201.md", 255, 359, "复核AUD201 V4 常数出处/回链"),
    ("AUD-202-SNR核验.md", 21, 37, "AUD202 §1 链路口径与公式"),
    ("AUD-202-SNR核验.md", 147, 193, "AUD202-005/006"),
    ("AUD-202-SNR核验.md", 221, 254, "AUD202-008"),
    ("AUD-202-SNR核验.md", 311, 365, "AUD202-011/012"),
    ("复核-AUD202.md", 22, 123, "复核AUD202 V1 权重退化"),
    ("复核-AUD202.md", 201, 292, "复核AUD202 V3 两篇FROZEN打架"),
    ("复核-AUD202-补.md", 403, 414, "复核AUD202-补 §10 改判一览"),
    ("AUD-203-天光无缝核验.md", 44, 64, "AUD203-03/04"),
    ("AUD-203-天光无缝核验.md", 104, 145, "AUD203-08/09/10"),
    ("复核-AUD203.md", 101, 242, "复核AUD203 V3/V4/V5/汇总"),
    ("AUD-204-面积交叠核验.md", 8, 26, "AUD204 §1 口径与公式"),
    ("AUD-204-面积交叠核验.md", 91, 112, "AUD204-06"),
    ("AUD-204-面积交叠核验.md", 144, 208, "AUD204-09/10/11/12"),
    ("复核-AUD204.md", 130, 223, "复核AUD204 W2"),
    ("复核-AUD204.md", 223, 327, "复核AUD204 W3"),
    ("复核-AUD204.md", 433, 462, "复核AUD204 进P0P1"),
    ("复核-合同层.md", 17, 111, "复核合同层 W1 pixel_area_power"),
    ("复核-测光默认阶数.md", 1, 228, "复核测光默认阶数 全文"),
    ("复核-结果层与收口层.md", 122, 238, "复核结果层 R3/R4"),
]

out = io.open(os.path.join(D, "digest.txt"), "w", encoding="utf-8")
for f, a, b, lab in SPANS:
    p = os.path.join(R, f)
    lines = io.open(p, encoding="utf-8", errors="replace").read().split("\n")
    out.write("\n\n=============== %s  [%s %d-%d] ===============\n" % (lab, f, a, b))
    for i in range(a - 1, min(b, len(lines))):
        out.write(lines[i] + "\n")
out.close()
print("ok")
