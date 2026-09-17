# 分片 C_ALG_IMPL_a（ROOT-004 旧 bug 清单按最新权威重定版）

- 分片名 `C_ALG_IMPL_a`｜分配 35 条（V10-N-01..V10-N-08；类别全为 C_ALG_IMPL；33×P1 + 2×P2）
- 产物 `reports/PROJECT-GOVERNANCE-01/root-scan/shards/C_ALG_IMPL_a.psv`（表头 1 + 数据 35 = 36 行，10 列，无列内 `|`/换行）
- 四态计数 **OPEN 35｜RESOLVED 0｜VOID 0｜UNVERIFIABLE 0**｜UNVERIFIABLE 清单：空｜日志 `run/PROJECT-GOVERNANCE-01/ROOT-004/logs/shards/C_ALG_IMPL_a.log`
- 基线：任务书所述 2c328348 在开工时已过期；复核期间 HEAD 连续前进至 1d66845d（HEAD=main=origin/main），全部证据按最终 HEAD 重跑，未沿用任何旧行号/旧路径。

## ID 覆盖与格式自证（本会话实跑）

- 命令：`awk -F'|' 'NR>1{print $1}' C_ALG_IMPL_a.psv > /tmp/pids.txt` ＋ `awk -F'\t' 'NR>1{print $1}' _assign/C_ALG_IMPL_a.tsv > /tmp/aids.txt` ＋ `diff -q /tmp/aids.txt /tmp/pids.txt`
- 输出：`assign=35 psv=35` ／ `diff: 逐位一致 (exit 0)`（首末条 V10-N-01 … V10-N-08，无缺失无多余）
- 命令：`awk -F'|' 'NR>1 && NF!=10' C_ALG_IMPL_a.psv | wc -l` ＋ 词表校验 ＋ 归属词表校验
- 输出：`NF!=10: 0` ／ `结论词表外: 0` ／ `归属词表内: 35/35` ／ `证据列以「命令：」开头且含「输出：」: 35/35`
- 归属分布：P1-001 9｜AIO-001 5｜CI-001 5｜PKG-001 4｜OBS-001 4｜P1-002 2｜CPU-001 2｜RT-001 2｜MOD-001 1｜INT-001 1
- GAP 关系：仅 V10-N-04、V10-N-07 记「与 GAP-013 重复」（I/O 所有权分散、无唯一 AIO），其余 33 条「无」

## 站点迁移（旧路径 → 现树路径，均本会话实测）

lib/healpix_db/healpix_drizzle → lib/algorithms/drizzle/healpix_drizzle；lib/astro_image_io → lib/infrastructure/aio；lib/drizzle·lib/hips → lib/algorithms/drizzle/{,hips/}src；lib/gaia_xpsd_client·lib/backend_host → lib/infrastructure/{gaia_xpsd_client,benchmark/backend_host}；lib/plate_solve → lib/algorithms/platesolve；lib/snr_estimator → lib/algorithms/noise_snr（该 TU 仍 untracked、未入编译图）；lib/core/module_adapters.cpp 与 runtime/runtime.cpp → lib/infrastructure/scheduler/src/；lib/orchestrator → lib/infrastructure/pipeline/orchestrator；healpix_browser_qt → lib/infrastructure/hips_browser/。cli/resource_gate.h·cli/monitor.h 未迁移，但 CLI-001 使 commands.cpp 行号整体前移（原判 843-853/912/946 → 现 745-753/795/806/846），V21-N-15/16/17、W3-R2-006 已按现树重定位。

## 原判据被现树否证的子项（第 10 列逐条标「订正」，未据此改判四态）

1. W1-N-01：Gaia 硬顶实为每文件 collector（gaia_client.c:2099/2101/2124），原「跨文件全局硬顶」机制不成立；但 n_ret 是跨文件求和、与每文件上限同型比较仍会误判 capped，且公开头 ipv_types.h:258-260 仍写「整数倍」旧口径与 :487 实现互斥 ⇒ 仍 OPEN。
2. W3-R2-002：resample band worker 已有取消安全点（module_adapters.cpp:5611）；残余为 astrocs_host_state_set_cancel 全仓零调用 ⇒ host->cancel->is_cancelled 恒 false，范围收窄但 OPEN。
3. V21-N-05：无构建树一腿已按 GAP-027 改 fail-closed（:98-103）；残余为子进程退出码从不读取、|| true 拉平 ⇒ OPEN。
4. V21-N-07：tools/assemble_audit.py 已按 RETIRE-001 显式退役（打印 ASSEMBLE_AUDIT_RETIRED + exit 2），且 package_audit/build_v19r{2,3,4} 在 ci/checks.json 引用数为 0；三份脚本 rc 仍不判定 ⇒ 降级为遗留处置，仍 OPEN。
5. V21-N-12：loader 侧非空哈希分支已在 secure_loader.c:499 实现；残余为清单哈希全 null + module_registry.c:734 双非空短路 + :780 build_id 传 "" + verify_install_tree 无非空分支 + 测试反向钉 null ⇒ OPEN。
6. W6-N-02：注册表已由 135 道平铺门改为 38 聚合门 + 147 step，validate_registry.py 的 R12 名额已被「steps 派发结构」占用（prereq→waiver 绑定仍无实现）；按新口径重算（27/147 声明，ctest 51 步零声明）核心仍成立 ⇒ OPEN。

## 异常

- SHARD_BRIEF §5 给出的 REBASE.md 路径片段在仓内不存在，实际口径文件在仓库根 `问题扫描/REBASE.md`。
- 4 条引文（V10-N-06、V21-N-05、V7-N-04、V10-N-07）原文含 C 逻辑或两句号，与 PSV 分隔符冲突，第 6 列内以「∥」转写并在该列注明该约定；其余引文逐字未改。
- 需真机或外部条件才能判定的子项（W3-R2-001 Windows 双失败实跑、W6-N-03 cmake --install 交付树、W6-N-05 dumpbin 导出集、W3-R2-006 TSan、W1-N-04 入库后实跑、V21-N-12 打包期是否真填哈希）在第 10 列显式声明「不另判」，判定只建立在可复跑的静态证据上；无一条因此被推定 UNVERIFIABLE，也未为凑数强判。
- 本分片未做任何修复、未做 git 写、未读 FATDUCK_ACCESS.md、未改仓库根条目与他人分片产物；除本分片日志外未写 run/。
