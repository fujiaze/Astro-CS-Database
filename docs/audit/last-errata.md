# 最后四项订正与定向复核

范围仅为API正本、源码目录、科学审核状态、历史证据/当前可运行边界；基于63b94e4ecf3467b47a5026fe1fdcb72b6c54963a。没有改变科学公式、算法路线、公共声明、schema或生产实现，不新建设计体系。

| 项目 | 实际订正 |
|---|---|
| API正本 | 删除API前半伪Request、旧返回载荷/方法和策略值；统一现有RejectionPolicy/WeightPolicy/SupportMap::Kind，纠正bind_wcs.solution及DrizzleRequest.budget路径。五份基础删除重复struct/旧签名，仅保数学责任并指contracts.hpp。原点逆转换使用from_fits_center/from_fits_edge，盲解统一SolverRequest.regions |
| 源码目录 | SYSTEM删除core/services/interfaces旧方案及另一套模块命名表，唯一目录/责任/依赖表为src/*、apps/*、gui与validation；同步OBJECTS及Context实现位置引用 |
| 科学审核状态 | 五篇页首和final-review“科学审核状态”统一REVIEWED_FOR_IMPLEMENTATION；不重复追加通用科学审核前置。负责人启动、实现数值/产品/性能验收及真实数据验证仍分别未完成，非同行评议/形式化证明 |
| 历史与当前运行 | 五篇页首锁固定历史档案，INDEPENDENT_ORACLES两组研究命令逐处标HISTORICAL_CHECKOUT_ONLY和完整SHA；历史结果/旧探针不是新实现证据，当前可运行检查仅根README所列 |

独立合同审核Agent与原科学审核Agent均只读复核本次范围，没有参与订正。发现三处P2：SYSTEM/统计基础仍暗示启发式robust可请求；盲解段残留centers；统计基础残留reconstruct_scene_variance旧参数/返回。均按最小修订清除，保留Gaussian前置、δ/M、支持域及曲率等科学语义。修订后两位再次确认关闭：本次范围无剩余问题、无新P0/P1，必要科学责任未被删去。

实际tools/check_design.py通过（302定义、77操作、四份请求、链接/引用/块/清理检查），git diff --check通过；header、schemas、源码、构建及示例内容与基线相同，未因纯文档订正重复运行科学测试。审核者科学结论仍不代表算法实跑通过。

先前round1/round2/final哈希与失败结论不回写；本次实际输入见[last-errata-input-hashes](last-errata-input-hashes.json)。后续实现任务由新issue派发，规范以本分支订正提交冻结，issue只组织执行、不另定义科学类型。
