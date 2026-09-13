# M7 · C_DOC_CODE_GAP（文档与实现差异）· P1

> **档位声明**：本文件内全部条目的 **类别与优先级由本行标题承载**（协议 §3 的「一类别×优先级一档」）；条目正文只在**偏离本档级别**时显式标注改档及理由（如 M7-A-101 记 P0→P1、M7-A-201 记 P1→P2 并撤核心结论、M7-A-001 记 P1→P0、M7-I-202 记 P1→P2 且改类 A_SCI_DEF→I_DOC_HYGIENE）。逐条四态判定与编号映射见 \`问题扫描/_merge/M7.md\` §2；每条均含 位置/权威依据/证据或证据出处/问题说明/影响/建议处置/置信度/related/四态判定 九项。

## M7-C-101 产品 manifest 的溯源面只有 SCI-* 没有 ALG-*/TST-* ⇒ 算法级溯源在产品出口处断裂（L21-008）
- 类别: C_DOC_CODE_GAP · 优先级: P1 · 权威依据 宪章 §4.3（溯源）、§12.3-1（五层等价）、§11.1（追溯闭环）
- 位置（本代理复核后**重锚**：L21 原引 `docs/interfaces/data/DATA-004_PRODUCT_PROVENANCE.md` 在本仓**不存在**）
  - `contracts/data/artifact_manifest.schema.json::producer.science_contract_ids`（复核时 :86，正则只收 `^SCI-…$`）
  - `runtime/artifact_store/artifact_manifest_validator.py::producer 允许键集`（复核时 :196：`{"module_id","module_build_id","entry","science_contract_ids"}`）
  - `runtime/artifact_store/production_store.py::sci_ids 透传`（复核时 :611/:620，注释「science_ids 从 manifest producer.science_contract_ids 透传（SCI-*）」）
  - 示例面：`contracts/data/examples/*.example.json`（:21/:14 全部只填 `SCI-*`）；对照 `docs/TRACEABILITY.csv（旁证件 docs/traceability/TRACEABILITY_MATRIX.csv）`（ALG-*/TST-* 在册但产品无落点）
- 证据摘录（当前树逐字）
  > allowed = {"module_id", "module_build_id", "entry", "science_contract_ids"}
  > sci_ids = prod.get("science_contract_ids")
- 问题说明 生产者元数据只能声明科学合同 ID；算法条目（ALG-*）与测试条目（TST-*）在产品级溯源链上无字段落点 ⇒ 一个 tile/mosaic 产物无法凭自身元数据回答「用了哪一版算法合同」，只能靠 `module_build_id` 反查构建记录。宪章 §12.3-1 的五层等价在产品出口只剩 SCI 一层可机器核。
- 影响 追溯闭环在产品层断裂（与 M6b-E-001「追溯双头」是不同事实：那条讲台账两处定义，本条讲产品字段缺位）。
- 处置 ①schema 增可选 `algorithm_contract_ids[^ALG-…]`（或把 `entry` 取值域约束为 ALG-*）；②validator 允许键集与 DATA-004 文档同步；③是否纳入 digest 输入面须 owner 裁决（改参与哈希的字段即改产品指纹）。
- 置信度 高（四处文件本代理复核在位）
- related L21-007（交换面最小平面集，同族）、M6b-E-001（追溯双头主题，related 不并档）、L16-015、`_merge/M7.md` §锚点重定位表
- 四态判定 **仍成立**（原锚不存在，已按当前树重锚后成立）
