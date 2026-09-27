# hipsform

HiPS 产品落盘形态检查器：核对裸 HiPS 目录与 zstd 归档包两形态的命名、布局、索引与哈希口径。

## 职责边界

- 放：形态解析、索引 schema 校验、归档容器布局、索引不变式、两形态哈希一致性与解压后 FITS 合法性检查。
- 不放：合同与设计正文（在 `docs/contracts/HIPS_STORAGE_FORM_CONTRACT.md`、`docs/design/PRODUCT_STORAGE_FORM.md`）、schema 机器事实源（在 `eng/contracts/schemas/`）、HiPS 产品本身。

## 内容

- `check_hips_storage_form.py` —— A–H 八组检查：锚存在、索引 schema 分派、形态互斥与命名、容器逐成员帧与逐字节还原、coverage 与 tiles 集合一致、两形态 tree_hash 相同、逐瓦片 FITS 校验；`--self-test` 每项配可执行正负例。仅用 stdlib + libzstd + tar/zstd，不运行仓内构建产物。
- `__pycache__/` —— Python 字节码缓存，不入库。

## 上游

- 注册于 `eng/ci/checks.json`：`CHK-HIPS-STORAGE-FORM`。
- 检查项条目见 `docs/ci/01_CHECKS.md`，门禁分级见 `docs/ci/03_GATES.md`。
