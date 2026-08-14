# Round 2 — Semantic / API 一致性（V17）

日期：2026-08-14 ｜ 范围：接口命名、状态机、语义单路径、config/schema/docs

## 检查项与结论

1. **weight 命名分离**：integrate.h 明确 `stack.support_x_snr2.v1` /
   `stack.equal.v1`（积分权）vs `upm.robust_control_weight.v1`（UPM 控制
   点权）；config `snr_weight_mode=snr2_normalized` 仅 UPM 段（已注释
   说明不混名）。PASS
2. **rejection status 契约**：P2RejectStatus 含 INVALID_METHOD=6 /
   INTERNAL_ERROR=7；Stage2/ACR 只接受 OK/UNDERDETERMINED（stage2.cpp
   hard fail；ACR kernel 返回错误→ throw/fallback）；CLI --plan 同契约。
   PASS
3. **integration 显式状态**：OK / NO_CANDIDATES / ALL_REJECTED /
   ZERO_VALID_WEIGHT / INVALID_INPUT；`n_used` 保留仅诊断，不再驱动
   状态判断（V17StatusesExplicit）。PASS
4. **support 单 reducer**：`p2_integrate_pixel` 计算 max(accepted support)；
   Stage2 CPU/ACR 与 large_scale 两遍路径全部消费 `pr.support`（rg 验证
   stage2.cpp 无第二处 max/mean）。PASS
5. **profile 版本化**：canonical=wbpp_2_9_1；wbpp_current 在 parser
   规范化；diagnostics 序列化 wbpp_2_9_1（real16 重跑诊断
   reject_profile=wbpp_2_9_1）。PASS
6. **normalization canonical**：astrocs_median_center_v1 /
   astrocs_median_scale_v1（alias 规范化；real16 诊断序列化
   astrocs_median_center_v1）。PASS
7. **large_scale 独立政策**：`integration.rejection.large_scale`
   （enabled/min_structure_pixels/low|high_grow_radius_pixels）；
   与 auto_policy/normalization 三政策分开；PIXINSIGHT_EXACT=
   NOT_CLAIMED。PASS
8. **legacy config aliases 删除**：parser 对 low/high/max_iterations/
   min_samples 返回硬错误（提示 migrate_stage2_config.py）；schema 无
   这些键；template 无。PASS
9. **API 文档一致性**：`tools/api_doc_consistency.py` 10 项 machine check
   全 PASS（deleted API absent / semantic IDs / status enums / wbpp alias
   / observed 命名 / freeze version / schema↔parser / defaults↔template）。
   PASS
10. **duplicate production science path = 0**：semantic_path_inventory.csv
    每行一个 canonical 实现；legacy 行分类 ARCHIVED_NOT_BUILT；
    no_legacy_production_reference PASS。PASS

## 语义漂移风险

- wbpp_current：已降到 alias，未来 WBPP 安装版本变化不影响冻结语义；
- CLI --plan 与 stage2 同默认/同 normalization（修复后一致）；
- config 一致性检查覆盖 large_scale 四字段（4/4）。

```text
ROUND2=PASS
```
