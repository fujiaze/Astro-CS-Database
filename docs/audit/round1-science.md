# 第一轮独立科学审核（未通过）

审核者：/root/independent_science_review，未参与编写，只读，不读旧Git源，不修改文件。完整读五份foundation、六份rebuild及声明，按初稿commit及round1-input-hashes.json审核。

| ID/级别 | 原位置、失败构型、影响 | 最小修订 |
|---|---|---|
| S1 P1 | SYSTEM N2、CatalogueProvider：只有cone query不能访问全离线quad索引，SolverRequest无搜索半径 | 公开分页quad/metadata/搜索圆域 |
| S2 P1 | SYSTEM N1 G：G=[1,−1]是1×2，R_native是2×2，R_native G维度不成立；方阵也不证明科学量正确 | 物理重组定义R_out G，或原生直接投影，signed无物理重建估计量须拒绝；同步科学说明 |
| S3 P1 | EXECUTION非线性停止：精确初始化残差/梯度/步全0，likelihood不能再降，却判nonconverged | 线搜索前识别驻点零步；非驻点才要求Armijo |
| S4 P1 | detector footprint强制WCS而无头先校准再盲解 | 定义pre-WCS与绑定操作 |
| S5 P2 | 多份逐样本string身份及拥有型footprints使4096²超预算，不能spill | 字典/规则网格与分块例外 |
| S6 P2 | PSF.parameters无轴序，Moffat椭圆度量、Airy半径、谱插值不唯一 | 精确密度/归一/尾公式、shape/单位；首期明确achromatic |

实际追踪：观测→校准→PSF/Gaia→α；几何→R→signal/noise/kernel；同数据cov→sky；权/LOO；Export→持久化→重开；资源/精度→独立真值。检查单独未决sliver、已有正背景加±1e12、负分子、共享A/Ω、极点spill；未发现新的几何公式错误。三帧共享母版、空间holdout仍共根、共线控制、异响应伪边均有正确条件/失败路线。确认unknown→equal、mixed非BLUE、非负QP、N<4不LOO及选择后cov限制。

独立重算U01得到4.328125。未执行科学算法，不能据静态审核声称生产功能通过。原结论：主链大体闭合，但上述阻碍未修前不宜直接交给新编码Agent。
