## M2a-A-3 by_coords 的最近邻/平手/重复命中/半径域界判据在 SCI/ALG/DATA 全无定义，仅存在实现事实，且平手判定依赖遍历序

- 类别: A_SCI_DEF
- 优先级: P1
- 来源: L10-018
- 位置: docs/algorithms/GAIA_QUERY.md::§1/§2 全节、::§3.1 execute 段；docs/contracts/DATA_SEMANTICS.md::§8.2 out_match_idx 行；lib/gaia_xpsd_client/src/gaia_client.h::query_spectrum_by_coords 注释；lib/gaia_xpsd_client/src/gaia_client.c::gaia_client_query_spectrum_by_coords（best_ang_dist 判定处）；tests/unit/gaia_adapter_test.c::D5
- 证据摘录（逐字，复核时点现文）:
  > （GAIA_QUERY.md:21-22）本模块不做自行消化（proper motion）、不做 SIP/像素投影（plate_solve 侧）、**不做匹配求解**。；（:184-185）`execute()`：一次锥形查询（4 个搜索入口之一）；一个 execute 只完成本模块查询，不做匹配/积分。
  > （gaia_client.h:17）query_spectrum_by_coords — 按坐标**匹配**光谱查询（**最近邻**）…
  > （gaia_client.c:2266）if (ang_dist < best_ang_dist) {   // 严格小于 ⇒ 平手取遍历先见者
  > （DATA_SEMANTICS.md:113）| out_match_idx | int32 | 坐标序 | −1 = 该坐标未匹配 |   // 仅此一行，无半径合法域/开闭性/去重/多坐标命中同星规则
- 权威依据: ALG-GAIA-001 §2「离散公式（源码逐条对应）」；宪章 §5.3（域界/误差/单位须明确）、§7.3（不支持语义须显式拒绝）、§13.1（边界与极端参数测试必备）；DATA-GAIA-001
- 问题说明: `query_spectrum_by_coords` 是 module.yaml `node_operations` 登记的五个 op 之一，但其匹配语义完全只存在于代码：① 平手（同角距两星）由"遍历先见者胜"决定，而遍历序随文件枚举序/collector 序漂移 → 非确定性面；② 查询半径的合法域与开闭性未定义（负半径仅有实现事实注释，无合同条款）；③ 多坐标命中同一星是否去重、行序如何随 matched_count 压缩，均无声明（后者的产品后果见 M2a-C-1）；④ ALG 一面写"不做匹配求解"，一面 op 名与 README:17 都称"匹配（最近邻）"，术语互斥。测试面 D5 用 find_star 预筛保证全命中，等于不测判据本身。
- 影响: 跨阶段消费按各自假设解释同一输出（HISS/测光参考星选择）；平手与非确定序使"同输入同输出"在跨目录布局下不成立，与 module.yaml:57 `determinism: fixed_reduction_order` 冲突（见 M2a-I-1）。
- 建议处置: ALG-GAIA-001 增设 by_coords 判据小节（最近邻定义、平手决胜规则须可指定且稳定、半径合法域与开闭、多坐标→同星去重策略、`-1` 语义与数组长度），并补平手/半径边界/部分未命中三类注册测试（与 M2a-C-1、M2a-F-3 同批）。
- 置信度: 高
- related: M2a-C-1、M2a-F-3、M2a-I-1
