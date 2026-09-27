# abi

本目录存放 ABI 子域的架构合同。

## 职责边界

- 放：模块加载与 ABI 边界相关的合同。
- 不放：公共 API 合同正文（在 docs/api/COMMON_ABI_V1.md）；C ABI 编码规范（在 docs/standards/C_ABI_STANDARD.md）；模块清单（在 docs/modules/）。

## 内容

- `ABI_003_SECURE_LOADER.md` —— 安全模块加载器（Secure Module Loader）合同。

## 上游

上游：docs/ASTROCS_DESIGN.md §8.4（模块与 ABI）、§11（双平台发行与安装）。
