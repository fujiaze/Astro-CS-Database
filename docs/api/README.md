# api

本目录存放对外接口的版本化合同：CLI 协议、公共 ABI、manifest 校验接口与三阶段模块 API。

## 职责边界

- 放：v1 版本的接口合同正文（命令/事件 schema、函数签名与语义）。
- 不放：数据对象语义（在 docs/contracts/）；架构总述（在 docs/architecture/）；实现说明（在 docs/modules/ 与 docs/plugins/）。

## 内容

- `CLI_PROTOCOL_V1.md` —— CLI 输入输出协议与事件流 schema，绑定唯一命令树与退出码。
- `COMMON_ABI_V1.md` —— 公共 C ABI v1 合同。
- `MANIFEST_VERIFY_V1.md` —— manifest 校验接口合同。
- `PHASE1_API_V1.md` —— normalize 阶段模块 API 合同。
- `PHASE2_API_V1.md` —— mosaic 阶段模块 API 合同。
- `PHASE3_API_V1.md` —— export 阶段模块 API 合同。

## 上游

上游：docs/ASTROCS_DESIGN.md §4（normalize）、§5（mosaic）、§6（export）、§7.1（命令树）、§7.2（配置、事件与退出码）、§8.4（模块与 ABI）、§10（I/O 与原子产品）。
