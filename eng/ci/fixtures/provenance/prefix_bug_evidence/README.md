# 修复前坏产物证据（PROV-SEMANTIC-FAKE）

这些 `p1_phot.json` 是 **FIX-P1 竞态缺陷** 的**修复前**输出，由
`reports/RELEASE-02/p1-phot-fix2.md` 与 `run/RELEASE-02/p1-phot-fix2/EVIDENCE.json` 记录。

保留为**受控红例 fixture**（`CHK-PROVENANCE-CONSISTENCY --self-test` 可用），
**不是**当前产物的样本 —— 原始大产物已从 `run/` 清除（否则门禁会对 bug 的历史输出永久报红）。

特征：`photometry_applied=true` 但 `photscale_detail` 缺失/为空，无法区分真实拟合标度与 NO_DATA 占位 1.0。
根因：`phot` 节点按文件约定读 `p1_wcs.json` 却未声明依赖边 ⇒ 与 `wcs` 并发 ⇒ 读不到时回退 config WCS ⇒
CRVAL=(0,0)/CD=0 ⇒ 锥形搜索落 (0°,0°) ⇒ 0 颗 Gaia 匹配 ⇒ NO_DATA 返回占位 scale=1.0 且 rc=0。
