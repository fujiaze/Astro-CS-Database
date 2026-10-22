# 第一轮独立合同审核（未通过）

审核者：/root/independent_contract_review，未参与编写，只读当前新树，不读旧源码或Git历史，不修改文件。输入为[初稿commit](https://github.com/fujiaze/Astro-CS-Database/commit/1247ea06c69a3b23c45283a83fd0d7d3f36c538a)，[七份规范hash](round1-input-hashes.json)。完整阅读六份rebuild、声明、schema、examples，按需核对观测/Gaia基础。

| ID/级别 | 原位置、失败构型与影响 | 要求的最小修订 |
|---|---|---|
| C1 P1 | contracts.hpp CatalogueProvider、SYSTEM N2：空centers盲解，无quad查询/尺度/亮度，XP coefficients无basis；盲解和标度需私有渠道 | 分页catalogue/quad、metadata/XP basis/brightness和显式搜索域 |
| C2 P1 | DetectorFootprint.wcs、Raw/Native和OBJECTS Ref非空：无WCS图先校准却必须先有WCS足迹 | detector-only初态和求解后绑定 |
| C3 P1 | Raw.sources/Index.sources/footprints vector：4096²仅四string身份就约2GiB，无法流式 | 共享身份前缀+规则ID、参数化footprint与分块例外 |
| C4 P1 | Overlaps/Snapshot：f=.63 full退出后重开，无f/order/WCS/primitive载荷，hash不能再生 | 具名GeometryRecipe/PrimitiveTable/Constraints及完整输入 |
| C5 P1 | SupportMap/贡献块：左半支持和右半支持面积相同但无子单元地址，贡献块无编码 | dyadic地址/状态和明确列式贡献schema |
| C6 P1 | OBJECTS solve四数组：Z/U0/预条件器/Policy shape与数学域不确定，不能独立恢复sky K/P | 具名SolveSpec、数学映射/转置、标准子action |
| C7 P1 | API按参数名wire，但make_covariance/quadratic_form等签名无参数名 | 全签名命名并生成完整typed operation schema |
| C8 P2 | Native kernel/response与Photo frames的连接不清 | 显式模型绑定revision和完整frame/response/prediction排序 |

审核实际构造U01：R为2×3、24列整数F使FFᵀ=C，NoiseRoot.factor=F、NoiseModel.influence=R、make_covariance→quadratic_form(t)。独立得RCRᵀ=[[3.75,2],[2,10.3125]]，t=[1,−.5]给4.328125。共享root冲突、unknown非零影响及PSD零根规则在数学层可正确区分；尚无算法实跑。

审核认可同数据PCPᵀ、同R模板、全局sky屏障、条件权/选择、同leaf合并、Export量区分、GeometryRevision传递失效和事务租约的文字规则；没有把文字完整误当载荷可构造。原结论：存在P1编码阻碍，不能交付实现。
