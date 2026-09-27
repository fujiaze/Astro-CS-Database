# contracts

本目录是机器校验的合同 schema 唯一事实源：门禁与检查器直接读取这里的 JSON Schema 与登记表。

## 职责边界

- 放：机读 schema（schemas/）、数据与工件合同的机读形态（data/）、配置与模块合同（config/）、阶段块流合同（block_flow/）、合同提案存放位（proposals/）、资源门合同（resource_gate_v1.json）。
- 不放：合同条款正文与说明（docs/contracts/，与本目录 schema 双向对应）；科学公式（docs/science/、docs/algorithms/）。
- 合同字段发生变化时，本目录 schema、docs/contracts/ 说明与相应检查器同步更新。

## 内容

- schemas/ —— 通用 schema：事件、run manifest、三阶段配置、性能门、管道块、投影登记、版本、追溯矩阵等（含 unified/ 子目录）。
- data/ —— 数据对象合同：artifact manifest、制品类型登记表、阶段间产品交换矩阵与 schema、AIO ABI 合同、统一对象兼容映射、阶段间不确定性排异溯源件。
- config/ —— 配置与模块合同：CLI 模块清单、CLI 自检、模块 DLL 合同、模块生命周期合同。
- block_flow/ —— 阶段块流登记（stage_block_flow.json）与一致性偏差文件（conformance_deviations.json）。
- proposals/ —— 合同提案存放位。
- resource_gate_v1.json —— 资源门合同。

## 上游

上游：docs/ASTROCS_DESIGN.md §8.4（eng/contracts：机器校验的合同 schema 唯一事实源）；docs/contracts/README.md（合同说明与 schema 的对应关系）。