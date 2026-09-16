# 分片 C_DOC_CODE_GAP_P1a（ROOT-004 旧 bug 清单按最新权威订正 · 分片执行）

- 分片名：C_DOC_CODE_GAP_P1a｜原类别 C_DOC_CODE_GAP｜原优先级 P1｜分配 32 条（M1a-C-005 .. M4-C-05）
- 产物：reports/PROJECT-GOVERNANCE-01/root-scan/shards/C_DOC_CODE_GAP_P1a.psv（表头 + 32 行；10 列 PSV，列内无竖线与换行）
- 四态计数：OPEN=29｜RESOLVED=3（M2a-C-2、M2a-C-7、M3-C-010）｜VOID=0｜UNVERIFIABLE=0
- UNVERIFIABLE 清单：无（0 条；32 条均按当前树重新取证）

## ID 覆盖自证（命令 + 输出）
命令：python3 读 .psv 与 _assign/C_DOC_CODE_GAP_P1a.tsv，校验表头逐字、每行 split("|") 长度==10、ID 集合与顺序、结论计数（脚本全文见 run/PROJECT-GOVERNANCE-01/ROOT-004/logs/shards/C_DOC_CODE_GAP_P1a.log）。
输出（逐字）：
```
header_ok: True
data_rows: 32
bad_field_counts: []
unique_ids: 32
assign_rows: 32
order_and_set_equal: True
state_counts: {'OPEN': 29, 'RESOLVED': 3}
owner_missing: []
```

## 异常（不阻塞判定）
1. 基线漂移：简报写 HEAD=main=ecf6ad6f，实测 HEAD=main=2c328348（ecf6ad6f 之后两个 ROOT-004 提交）；本分片全部按 2c328348 当前树取证。
2. 行锚漂移普遍：finding 行号与当前树不符（例 M2a-C-2 drizzle_engine 1137→1164、M3-C-011 star_matcher 552→615、M3b-C-03 sdet_api 667/1943→703/1979、M2a-C-6 module_adapters 2234→2967）；一律按内容/符号重定位。
3. 账本与树不一致：M2a-C-6 引的 configs/stage1.template.json 现位于 lib/orchestrator/configs/stage1.template.json（pixfrac 0.8 仍在），账本 RQS-RC1 记「从未入库」。
4. M3-C-005 附加事实：p1_session 通道在 Linux 交付 exe 未链入（finding 内【E2】实测），不影响「会话通道缺退化校验」的静态判定。
5. 纪律：FATDUCK_ACCESS.md 未 read/未打印；零修复、零 git 写；只写本 .psv/.md 与 run/ 日志。

## 最重要的 3 条 OPEN
1. M2a-C-10 — registry 登记的 astrocs.calibrated_frame.v1 过不了自家 type_id 正则（python 实测 False），登记即死锁、一经使用必被词法拒。
2. M2a-C-4 / M2a-C-5 — drizzle 与 Gaia 模块 README/module.yaml 的「无源码/未建 DLL/无 target/无测试/无 plan-cancel」与构建事实（SHARED target、七项 ctest、lease/cancel 注入）相反。
3. M4-C-04 — SCI 无条件断言「单帧区 harmonic continuation 填」，而 CLI 会话 λs=0 且不调用 p2_upm_build_geo（无 nodes），延拓在生产装配不发生。
