# CAT-GAIA-TEST 最终结果（SA-CATG-T）

日期：2026-02-19（本机时区）
状态：**完成 — ctest 9/9 PASS**（gaia_cat_* 全部，0 failed）

## 验收关键词覆盖
- unit：I1-I4 + I5 入口（gaia_cat_unit，含 spectrum_params rc==1、for_solver 独立期望集与 bitwise float mag）
- property：P1-P4（gaia_cat_properties；P2 修复 UAF：cold/warm 释放移至 P2 后）
- oracle：tests/oracle/gaia_oracle.py（4 层独立比对 O1-O4，绝不调用被测符号；O1 以冻结容差 5e-10 判定，容忍跨 libm 末位 ulp）
- negative：N1-N5（坏魔数/三类截断/坏压缩标签/create-OOM/查询注入——N5 每迭代新建 client 规避 query_cache 短路，地址簿对账零泄漏）
- performance：I5 200000 星全天空 cold 查询 ≈0.038 s（perf.json）

## CMake 注册（tests/unit/CMakeLists.txt 追加块）
- gaia_xpsd_fixture_gen + add_custom_command 构建期生成 fixture（clean 目录删 truncate.xpsd；truncdb 仅留 truncate.xpsd）
- gaia_cat_test / gaia_cat_polar_ref（-DGAIA_POLAR_PRUNE_DISABLED）
- add_test：gaia_cat_{unit,properties,worker,negative,truncate,performance,dump,polar_ref_dump,oracle}（oracle 挂 Python3，DEPENDS dump 两测试）

## KI 登记（未修生产代码）
- KI-1：query_cache.out_mag 为 float（gaia_client.c:123）→ 缓存命中 mag 非双向 bitwise（ra/dec bitwise）
- KI-2：cone_search_with_spectrum 混合 DB 时 DR3 星光谱区段不 memcpy（gaia_client.c:2045）→ 非 SP 星光谱未初始化

## 变更文件
- tests/unit/gaia_cat_test.c（七模式测试主体；地址簿泄漏检测钩子）
- tests/unit/gaia_xpsd_fixture_gen.c（strdup 修 manifest id；_GNU_SOURCE；零星 dec 网格；manifest 增 proj/cx/cy/x0/y0 期望重算域）
- tests/unit/CMakeLists.txt（追加注册块）
- tests/oracle/gaia_oracle.py（新建）

## 产物/日志
- run/local/agent_catgaia_test/：gaia_cat_test、gaia_cat_polar_ref、gaia_xpsd_fixture_gen、inc/、cat/{clean,truncdb,work}、impl.json、polar_ref.json、perf.json、ctest-gaia.log、cmake-{config,build}.log、diag* 诊断
- 构建验证目录：run/local/catgaia-build（ctest -R gaia_cat → 9/9 Passed）

## Git
未执行任何 git add/commit/push（按任务纪律，SubAgent 不提交）。
