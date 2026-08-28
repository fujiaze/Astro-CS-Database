# BENCH-004 执行日志 (2026-08-28, vm-bj)

## 输入
03_TASK_DETAILS BENCH-004 行(median/MAD/p05/p95、噪声裕量、逐 kernel 选择、profile hash/失效/fallback;验收=profile schema/mutation/stale/AVX512 slower tests+无 profile baseline 多线程); 06 §4-6; cpu_profile.schema.json(控制包)。

## 动作
1. bench_harness 增补: select_with_noise_margin(收益<margin_rel → 保守路径; margin=0 纯最快; OK-only 结构性)+no_profile_policy(06 §6: 只 baseline/workers=有效 affinity 可用≥2 不得退 1/reason=no_valid_profile)。
2. tests/backend/profile_gen_main.cpp: cpu_profile.json 生成器——hardware 子集(cpu_profile.schema 同构, fingerprint=sha256(vendor|family|model|stepping|feature_bits|xcr0|avail))+build(version/commit 40hex/cli_sha256/abi/backend_sha256)+memory_benchmark+逐 kernel(kernel_id/version/precision/size_class/backend_id/workers/block_size/oracle_status/measurements{median/mad/p05/p95/correctness_hash})+verdict; calibration medium 走完整 harness 流程(Oracle→预热→9 样本)。
3. tools/validate_cpu_profile.py: schema 最小校验(required/additionalProperties=false/pattern)+失效判定(ISA state 变化→STALE/affinity 收缩→STALE/commit 变化→STALE/指纹重算不一致→STALE)+oracle_status=fail→FAIL(正确性优先)。
4. tests/backend/test_cpu_profile.py 7 测试: schema 有效+统计序(Verified via 实际生成 profile)/ISA state mutation→STALE/affinity 收缩→STALE+commit→STALE/oracle fail→FAIL/**AVX512-slower 实测**(driz_accum 变体更慢→不选, 与 ISA-001 台账一致)/无 profile 策略(workers=affinity 不退 1)/噪声裕量(边际 +6.4%→保守 baseline; 裕量 0→纯最快; 大收益错误→排除)。

## 验证
- 全量回归 unittest **147/147 OK**(新增 7)。
- profile 由被测 CLI 的 hardware inspect 同源实现生成(fixture 真实性); 校验器 VALID/STALE/SCHEMA_FAIL 三态实测。

## 限制与遗留
- profile 生成器目前覆盖 calibration medium 单 kernel(quick 形态); full 模式全矩阵(12 kernel×size×precision)随 CODE-P* 科学接线扩展——生成器结构已支持逐 kernel 数组。
- 失效规则中 kernel_version/precision contract 字段在案(schema), 其触发测试随 f64 kernel 落地补充。

## 产物
lib/backend_host/bench_harness.{h,cpp} 增补; tests/backend/{profile_gen_main.cpp,test_cpu_profile.py}; tools/validate_cpu_profile.py; 本日志。

## PASS 判定
median/MAD/p05/p95+噪声裕量选择+逐 kernel 选择结构+profile hash/失效判定/fallback 策略全实现; schema/mutation/stale/AVX512-slower/无 profile 多线程 7 测试全过。BENCH-004 = PASS。
