# check_config_contracts 夹具

> 本目录是 `eng/tools/quality/contracts/check_config_contracts.py` 的**历史夹具**。
> 该检查器判据已反转（权重模式键出现即拒绝），自检面为内置的 `--self-test`
> （临时树正例/负例 + fail-closed，7 例），**不再读取本目录**；本目录目前无任何
> 调用方（orphaned）。

按 A44 口径，本目录内容的正确含义是：

- `valid_min.json` —— 合法最小配置，**不得**携带 `weight_mode` /
  `legacy_allow_weight_fallback`（二者已删除，出现即 fail-closed 拒绝）；
- `invalid_enum_mismatch.json` —— 负例：携带已删除键 ⇒ 必须被拒绝；
- `invalid_default_mismatch.json` —— 负例：`integration` 为空 ⇒ 缺必需键。

权威依据：`docs/ASTROCS_DESIGN.md` §3.1:171/175；`docs/science/PSF_SIGNAL_WEIGHT.md`
§4:62/72；`docs/contracts/CONFIG_CONTRACT.md`（`CON-CONFIG-CONTRACTS` 行）。
