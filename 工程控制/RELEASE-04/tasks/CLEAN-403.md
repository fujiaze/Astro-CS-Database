# 任务：CLEAN-403 aio 唯一 I/O 棘轮收口

## 目标
生产路径文件 I/O 全部经 aio（最高设计 §10），收口 RELEASE-03 棘轮台账剩余的 47 处 PRODUCTION-RESIDUAL；测试桩 I/O 显式白名单；退役/休眠路径残留随代码清理归零。

## 权威依据
- ASTROCS_DESIGN.md §10（I/O 与原子产品）
- ENGINEERING_SPEC.md §10（机器一致性检查）
- GAP_AUDIT G2-5

## 改动范围（文件域，互斥）
- 允许改：47 处 PRODUCTION-RESIDUAL 涉及的生产源文件、aio 接口（如需补能力）、棘轮检查器与白名单
- 禁止改：测试文件中的测试桩（走白名单登记）、docs、其他 FIX 域

## 步骤
1. 重新运行棘轮台账扫描（以当时 main 为准），把 155 文件/1656 处残留按 PRODUCTION-RESIDUAL / TEST-HARNESS / DORMANT / RETIRED-PENDING 分类复核；
2. 47 处生产残留逐个改为经 aio（FITS/HiPS/manifest 读写、临时文件、原子发布）；aio 缺能力时先在 aio 补接口再改调用点；
3. 56 处 TEST-HARNESS 在检查器白名单显式登记（文件 + 函数 + 理由），未登记的测试直连 I/O 同样判红；
4. DORMANT/RETIRED-PENDING 与 CLEAN-401 联动：对应代码删除后残留自然归零；
5. 棘轮检查器 fail-closed：新增生产直连 I/O 判红，配 `--self-test` 红绿用例；
6. 全量构建 + 合成全链 + ctest 全绿，重点核对原子发布语义未被改坏（与 FIX-401 协同顺序：本任务先收口读路径，FIX-401 做 tile 原子写）。

## 验收门（可机器复跑）
- [ ] 棘轮扫描 PRODUCTION-RESIDUAL = 0；
- [ ] 白名单与实际 TEST-HARNESS 一一对应（机器比对，无未登记项）；
- [ ] 检查器对新增直连 I/O 负例判红、对白名单内测试桩判绿；
- [ ] 合成全链三阶段 rc=0、产品 manifest/哈希完整；
- [ ] `ci --all` 全绿。

## 禁止
- 不用白名单掩盖生产路径直连；
- 不改科学语义（只改 I/O 通道）。
