# traceability

追溯矩阵的机器校验与 CSV 视图生成：八层链 SCI→ALG→DATA/API/ARCH→MOD/SRC→TEST→EVIDENCE 全量核对。

## 职责边界

- 放：矩阵 JSON 的结构、ID、链断裂与悬空锚校验器，及其同构 CSV 视图生成器。
- 不放：矩阵正文与规范（在 `docs/traceability/`）、检查项注册表（在 `eng/ci/checks.json`）。

## 内容

- `check_traceability_matrix.py` —— C1–C8 判据：文件在位且被 Git 跟踪、schema 合规、空单元格禁通过、ID 格式与唯一、链断裂、VERIFIED 锚可解析、authority 覆盖登记；git 面不可用时显式降级并 fail-closed。
- `gen_traceability_csv.py` —— 由权威 JSON 矩阵生成稳定排序的 CSV 视图，供人工比对与 diff。
- `__pycache__/` —— Python 字节码缓存，不入库。

## 上游

- 注册于 `eng/ci/checks.json`：`CHK-CONTRACT-TEST` 的步骤 `TRACEABILITY-MATRIX`（`python3 eng/tools/traceability/check_traceability_matrix.py`）。
- 检查项条目见 `docs/engineering/01_CHECKS.md`，门禁分级见 `docs/engineering/03_GATES.md`。
