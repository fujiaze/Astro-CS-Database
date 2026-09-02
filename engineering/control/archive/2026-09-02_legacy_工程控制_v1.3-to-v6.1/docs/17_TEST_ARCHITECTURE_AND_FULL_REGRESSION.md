# 测试架构与统一全量入口

建立一个跨平台测试入口（可由 Python 调度，但所有子进程有超时），按层执行：

1. contract/schema；
2. unit/component；
3. PlateSolve 710；
4. T1–T4 校准代表矩阵；
5. Stage1 全 TestData；
6. 银心 32 HISS；
7. gradient injection/real mosaic；
8. HCSD round-trip；
9. browser backend/unit；
10. browser performance/visual smoke。

输出统一 JUnit/JSON/Markdown；缓存已成功的昂贵步骤时必须绑定输入 hash、commit、config hash 和工具版本。
