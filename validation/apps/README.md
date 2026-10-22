# apps 验证入口（独立于 src/ 内部头）

本目录只调用公开 API / `acsd-contract` 进程协议，不 include `src/` 的内部头，
也不从 `examples/*.expected-model.json` 反推输入或放宽容差。

## 合同示例实跑（阶段 A 出口）

```text
validation/apps/run_contract_examples.sh [输出目录]
```

脚本按顺序真跑 `examples/u01.request.json`、`examples/external-array.request.json`、
`examples/to-fits.request.json`，把真实 response、退出码、请求/response/期望模型的
SHA-256 写进 `validation/apps/evidence/`，并由 `compare_examples.py` 给出逐例数值差异。
失败的尝试保留在 `evidence/attempts/`，不覆盖成功结果。

已实跑结果（`evidence/comparison.json`）：

| case | 期望 | 实跑 | 差异 |
|---|---|---|---|
| u01 | 方差 4.328125 L²/sr² | 4.328125 | 0.0（精确相等） |
| external-array | [3, 7] | [3.0, 7.0] | [0.0, 0.0] |
| to-fits | FitsPixel1(1.25, 3.5) | (1.25, 3.5) | (0.0, 0.0) |

`u01` 的 `numeric_enclosure` 实跑为 `null`（底座 `quadratic_form` 未给出数值包围），
期望夹具写的是零宽包围 `{4.328125, 4.328125}`；方差值本身逐位相等。该差异已报给 Lead。

## 产品与 CLI 验证

见 `validation/apps/README.md` 后半段与 `evidence/` 下的 `product_*.json`、`cli_*.json`。
