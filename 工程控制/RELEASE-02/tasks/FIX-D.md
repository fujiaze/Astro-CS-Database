# FIX-D CLI / export 合同

## 目标

闭合 P0-14：官方 export JSON 模板（`config/templates/export.phase_config.json`，符合 `contracts/schemas/phase_config_export.schema.json`）被 CLI 拒绝（rc=2），CLI 实际接受另一套字段（`source.hips_dir/center/scale_deg_per_px`）。

## 修复内容

1. 以 config_registry「唯一事实源」为准对齐：模板、schema、CLI 解析三方一致；官方模板直接可用；
2. 排查 normalize/mosaic 模板是否存在同类不一致，一并对齐；
3. 三命令的 `--json` 模板生成、模板校验、运行前三级预检（绿 correct / 橙 optimize / 红 error + yes 确认，-y 跳过确认，error 强制阻断，-force 仅用于缺校准帧等场景）做合同级测试；
3. help 直接输入即详细帮助；benchmark 无参、结果写安装目录并自动读取（若 RELEASE-01 审计中这两项有偏差一并核）。

## 验收门

- 三个官方模板逐一通过 CLI 校验并可运行（合成数据）；
- 故意制造暗场时间超容差（橙）、滤镜不匹配/文件缺失（红），预检颜色与阻断行为符合合同；-y 与 -force 语义有负例测试；
- 模板-schema-CLI 一致性纳入 CI 检查（无现成检查器则按 §3a 新增，带 --self-test 与负例）。
