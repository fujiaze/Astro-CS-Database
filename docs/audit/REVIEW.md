# 独立完整性审核与处理

初稿提交：1247ea06c69a3b23c45283a83fd0d7d3f36c538a。两位首轮审核者未参与设计、未编辑文件、未读旧源。[合同报告](round1-contract.md)、[科学报告](round1-science.md)保留未通过结论与具体构型。输入hash保留，不回写为通过。

| 首轮问题 | 已实施修订与位置 |
|---|---|
| C1/S1 catalogue/XP缺能力 | 声明CatalogueMetadata/Columns/Index、分页query/quad/lookup、build/make_provider；SYSTEM公开索引算法与basis来源 |
| C2/S4 WCS前置循环 | FootprintSet允许detector-only；detector_footprint/bind_wcs/bind_observation_model；消费边界校验 |
| C3/S5 常驻元数据 | SourcePartition规则/块索引，FootprintSet矩形模板+FootprintExceptions列式例外，inline限4096 |
| C4/C5 几何/支持闭包 | GeometryRecipe/PrimitiveTable/Constraints/OverlapTable，Snapshot source_inputs；SupportMap五列dyadic地址与状态 |
| C6 solve载荷 | SolveSpec具名Z/W/尺度/Policy/秩与精确square/LS映射及真转置，无私有分解blob |
| C7 driver参数 | 所有free参数命名；从唯一声明导出scientific/CLI schema，包含每operation参数、真实对象字段及类型 |
| C8 pipeline连接 | bind操作；Photo frames/responses/predictions严格同序，删除未定义reference_frames |
| S2 G方向 | 物理重组R_out G，signed linear_view保GCGᵀ但不造物理Drizzle；原生路径R_native；观测科学说明同步澄清 |
| S3 驻点停止 | EXECUTION明确先判驻点，零步可收敛，非驻点才Armijo |
| S6 PSF不唯一 | SYSTEM冻结Gaussian/Moffat/Airy密度、参数轴/单位/尾、空间Basis；首期achromatic单谱段PRF；evaluate_psf唯一积分 |

另补齐Unit.linear_power、detector_frame坐标身份、误差记录强引用，及真正U01请求/真值模型。数学定义未被旧实现约束；首轮发现不是旧源码回归失败。

## 后续独立复核与最终状态

第二轮输入冻结为 commit 11ae71c20ad8f692cfeb0b24a0a5143765869402（tree d32255b5edc812f7a703763d5501b0dd1cd4d2c7），[hash](round2-input-hashes.json)。该轮仍未通过，具体反例、影响和修订见[第二轮报告](round2-review.md)，不以最终通过改写原结论。

最终由原合同审核者、原科学审核者及全新完整性审核者（均未参与设计、只读、不读旧源；全新审核者不读既有审核结论）检查修订后的科学→算法→对象→API→资源→持久化→独立验证连接。又发现并修订 counts 数值误差跨 measure→photometry 丢失的问题，再次复核后关闭。见[最终逐项结论](final-review.md)和[实际输入hash](final-input-hashes.json)。

**状态 READY_FOR_OWNER_REVIEW：三位独立审核者均未发现尚存 P0/P1 编码阻碍，认为可交给不了解旧 ACSD 的编码 Agent，仅据公开规范开始完整实现。** 这是设计可实施性判断；不等于科学算法、产品重开、性能或真实数据已经实跑通过。负责人尚未正式接受设计；本分支无科学实现，不开始全面编码。

[最终工程验证记录](VALIDATION.md)只报告声明编译、schema/输入闭包/链接和清理检查；没有用旧测试证明新实现。
