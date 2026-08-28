# API-004 执行日志 (2026-08-28, vm-bj)

## 输入
03_TASK_DETAILS API-004 行(Phase2 同上定义;明确 drizzle/UPM/rejection/integration 数据所有权和 thread budget;验收=doc-symbol-signature+不得共享隐藏全局状态); 现存 phase2 头文件(11 个 .h, 签名权威); API-001/003 模板。

## 动作
1. 新建 docs/api/PHASE2_API_V1.md(API-P2-001..012): §1 所有权图七对象(Coverage/ControlObservation+frame_id_cache/UPM Model/CandidateStack/PixelStack-Result/async_io 队列)逐个 创建/持有/释放/传递方式(p2_upm_close 为真实释放函数, 修正初稿臆造的 upm_model_free); §2 逐函数登记 14 行全名化(reentrant/threadsafe/internal_parallel/取消点/test ID 五字段; acr 相关行标注配置守卫 V5 非 cpu/auto 拒); §3 thread budget 绑定(sampler=1 串行 reference/blocks=预算/行带=预算/async=1/Σ≤全局 budget+stage 事件携带 workers 实际值); §4 错误码映射(rc→acs_status 表, UNDERDETERMINED=ACS_OK 语义); 无隐藏全局状态声明(唯一模块级=g_model_floor+logger)。
2. 机器门 tests/api/test_p2_api.py 6 用例: 21 符号头文件实跑核对/文档登记双向完备/所有权七对象/禁隐藏全局/预算绑定/错误映射。
3. 过程修正: 首轮 checker 实跑暴露文档缺漏(全名化写法 _cached/_ex/_build_geo/stats_mad 不成子串)与臆造符号(upm_model_free)——全部以头文件实跑核对修正, 机器门即防臆造机制生效。

## 验证
- 21/21 符号头文件双向核对通过。
- 全量回归 unittest **70/70 OK**(新增 6)。

## 产物
docs/api/PHASE2_API_V1.md; tests/api/test_p2_api.py; 本日志。

## PASS 判定
drizzle/UPM/rejection/integration 数据所有权逐对象冻结(创建/持有/释放/传递); thread budget 绑定显式; 逐函数并发合同+test ID 齐; 无隐藏全局状态; doc-symbol 实跑一致。API-004 = PASS。
