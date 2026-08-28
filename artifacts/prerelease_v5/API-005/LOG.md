# API-005 执行日志 (2026-08-28, vm-bj)

## 输入
03_TASK_DETAILS API-005 行(Phase3 request/result/HiPS source/WCS output/sampler/coverage/FITS;拒绝未支持科学模式;验收=doc-symbol-signature+每 API 直接 test ID+输入不明确返回确定错误); SCI-P3-001; ALG-P3-001..004; ARCH-P3-001; API-002(manifest/final 事件)。

## 动作
1. 新建 docs/api/PHASE3_API_V1.md(API-P3-001..006, Phase3 施工规格): §1 p3_session 五函数生命周期(同 p1/p2 同构); §2 request 十一字段表逐字段约束(projection 仅 TAN/sampler 默认 bilinear/parity east_left 默认/bitpix -32|-64/max_tiles 可降不可升); §3 result JSON(run_id/sha256/order_sel_used/provenance{missing_tiles}/coverage_stats); §4 显式拒绝清单逐行→错误码(UNSUPPORTED/PARAM; tile 缺失与 NaN 显式"非错误"走 coverage 语义, 禁猜测); §5 逐 claim 追溯(ALG-P3-002/003/004+SCI-P3 §9a 同源); §6 机器门。
2. 机器门 tests/api/test_p3_api.py 6 用例: 生命周期/request 字段/**拒绝清单与 SCI-P3 科学合同同源交叉核对**(多通道/lossy/int+BLANK/flux 双侧必须出现)/missing 非错误语义/默认值冻结/追溯锚全名。
3. 修正: 追溯锚 "ALG-P3-002/003" 斜杠写法不成子串→全名化(与 API-004 同教训)。

## 验证
- 全量回归 unittest **76/76 OK**(新增 6)。

## 产物
docs/api/PHASE3_API_V1.md; tests/api/test_p3_api.py; 本日志。

## PASS 判定
request/result/HiPS source/WCS output/sampler/coverage/FITS 全字段定义; 未支持科学模式逐条显式拒(与 SCI-P3 同源机器核对); 输入不明确→确定错误; 每 API 直接 test/锚点 ID。API-005 = PASS → API 段(001..005)闭合。
