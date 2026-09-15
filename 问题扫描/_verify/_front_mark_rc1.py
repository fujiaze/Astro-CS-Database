
import csv
fp="问题扫描/账本/FIX_LEDGER.csv"
rows=list(csv.DictReader(open(fp,encoding="utf-8-sig")))
hdr=list(rows[0].keys())
if "verified_note" not in hdr: hdr.append("verified_note")
NOTE={
"M8-F-003":("FIXED","adaeb531 两机制复算消失（四处 skipTest 改 self.fail 加 linux_build_root_graph.sh 补 install 载荷与 .so）；残余 :16 类级 skipUnless(cmake/g++) 仍可整类静默跳属他条域；regression_test 锁名应订正为 test_01..test_05"),
"V7-N-09":("NOT_A_DEFECT","判否负清单元条目（抽查 4/9 仍成立）故保留免重报价值不作缺陷；priority 原为空已补 P3"),
"V12-N-16":("STILL","收窄至 kLn10 两站（snr_science.cpp:33 与 noise_model.cpp:34，位数还不一致）；sqrt(10) 与 MAG_PER_LN10 腿三向复核零命中须订正原描述"),
"M5b-E-05":("STILL","剥 AGENTS 子句（base 时已对齐）；RELEASE_STANDARD.md:19 站点灭失该文件仅 12 行"),
"M2a-C-6":("STILL","加注 configs/stage1.template.json 站点灭失（从未入库）"),
"M3b-I-01":("STILL","加注 theta 不可达守卫子项已由 SDET-ANGLE-001 修复"),
"M1a-G-001":("STILL","加反证注 p3_writer.json:6069 coordinate_frame 基线前已在故原句应收窄"),
"M2a-G-1":("STILL","较报告更差的新证：build.yml 的 standards-registry 步骤已消失且全 workflow 零引用"),
"L28c-E-002":("STILL","docs/algorithm.md 第3节在但挂 ARCHIVED 失实横幅"),
"M6b-E-001":("STILL","新证 TRACEABILITY-CODE 门现验 artifacts/prerelease_v5 旧表即验错对象；可交比 ID 现 6 个（CSV 被删至 63 行）而 TEST 一致 0/6"),
"M5b-G-07":("STILL","两腿原样：门验 docs/review 不等于 docs/owner，且宪章 §12.1 点名的 PHASE_OVERVIEW 全仓不存在"),
"M9-A-1":("STILL","修复未固化：判据未收口、去向未记、DISP 零登记（001-008 无此项）"),
"M2b-F-01":("STILL","百万点 oracle 数据与生成器缺位、未注册 ctest（仅 r9b_neighbors 在册）、1.2 乘 px 对 1e-12 数量级差、极点 continue 全原样，P0 维持"),
"V19-N-01":("STILL","check_ctest_registration.py:65 只认 add_test(NAME 且 :178 的 C4 反向锁死补声明；全仓 gtest_discover_tests 逐字仍 40 处双零覆盖"),
"V11-N-10":("STILL","五镜像在 checks 与 workflows 与 ctest 注册面 0 命中且无布局对账工具，零采集必漂的统计关系未被打破"),
}
n=0
for r in rows:
    k=r["id"]
    if k in NOTE:
        st,nt=NOTE[k]; r["verified_state"]=st; r["verified_by"]="RQS-RC1"; r["verified_date"]="2026-09-15"; r["verified_note"]=nt; n+=1
    if k=="V7-N-09": r["priority"]="P3"
with open(fp,"w",encoding="utf-8-sig",newline="") as f:
    w=csv.DictWriter(f,fieldnames=hdr); w.writeheader(); w.writerows(rows)
print("写入注记",n,"列数",len(hdr))

