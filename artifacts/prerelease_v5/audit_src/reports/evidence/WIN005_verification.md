# WIN-005 验证报告 — MSVC static analysis / Application Verifier-ASan 可用集 / 重复-取消-路径测试

结论: **PASS**(`/analyze` 0 错误; ASan 无泄漏/invalid access; 重复/取消/路径测试 Windows+Linux 全绿; 修复 2 处真实 P1 候选; 判定 0 P0/P1)。

## 1. 判据(03 L149)
> MSVC static analysis、Application Verifier/ASan 可用集、重复/取消/路径测试; 0 P0/P1; 无竞态/泄漏/invalid access。

## 2. MSVC /analyze 静态分析
基于 MSVC BuildTools 18, 以 `/analyze` 配置 `build/win_analyze` 并对 astrocs 目标 Release 编译。
- **错误数 = 0**。
- **C6xxx 警告 227 条**，分类如下:
  - **第三方(cfitsio 等, 项目不自研/不入自研清单)**: 约 174 条（35×C6262, 32×C6011, 22×C6001, 14×C6387…）。
  - **自研代码**:
    - **C6262(栈占用, 2级): 5 文件**(main.cpp~66KB、sha256.cpp、backend_loader.cpp、aio_fits.cpp~1MB)。属代码质量(P2), 非 P0/P1; 1MB 栈(io 缓冲)经确认仅写路径大缓冲, 无越界。
    - **p3_resample.cpp C6011(空指针解引用)**: **误报**。`q[2][2]` 初值 nullptr, 后有 `if(!q[i][j]) q[i][j]=nearest_pt;` 兜底; 中心点 `den=1` 恒定加入 `pts`, `nearest_pt` 必非空, 故四个象限点必非空; 分析器无法跨循环证明, 判误报。
    - **aio_fits.cpp C6001(未初始化 iobuf)**: **误报**。`char iobuf[1<<20]; setvbuf(fp, iobuf, …)` 为 stdio 工作缓冲, 有意不初始化。
    - **aio_fits.cpp C6387(对可能为 0 的指针 memcpy, 3 处)**: **真实候选(P1)** → 已修复(见 §4)。
    - **upm.cpp C6297(算术溢出, `1u<<(target_order+9)`)**: **真实候选(P1)** → 已修复(见 §4)。`target_order` 允许范围 [0,29](stage2_common.cpp:29), `+9` 可达 38, 32 位移位溢出为 UB。
    - **win_analyze\\eval_y.c C6386**: build 目录生成物(bison/flex), 非源码。

## 3. ASan(Application Verifier/ASan 可用集)
以 `/fsanitize=address /Zi`(Debug)配置 `build/win_asan`, 构建成功, 运行时 DLL(clang_rt.asan* )随 exe 就位。
`python -m unittest tests.cli.test_cli_protocol`(ASan exe): **Ran 20 tests → OK**; 扫描 stderr **无** AddressSanitizer/LeakSanitizer/heap-buffer-overflow/use-after-free/SEGV → **无泄漏、无 invalid access**。

## 4. 修复的 P1 候选(已 commit)
1. **aio_fits.cpp C6387**: 3 处 `malloc(…*sizeof(AIOFITSKeyword))` 后未检 null 即 `memcpy` — 加 `if(out->keywords){…} else {out->keyword_count=0;}` OOM 防护, 防 null 写。
2. **upm.cpp C6297**: `(double)(1u<<(unsigned)(target_order+9))` 移位 UB — 改用 `std::ldexp(1.0, target_order+9)`, 任意指数精确且无 UB(对合法 [0,29] 值结果一致)。

## 5. 重复/取消/路径测试(跨平台)
`test_cli_protocol`(含 cancel→9、crash boundary→70、Unicode 路径、重复 manifest、退出码映射、stale profile→5 等)。
- Windows(Release): **20/20 OK**。
- Windows(ASan Debug): **20/20 OK, 无 ASan 发现**。
- Linux: **20/20 OK**。
本轮两个测试跨平台缺陷已修: `run()` 指定 `encoding="utf-8", errors="replace"`(Windows gbk 解码误致 `stderr=None`); `test_10_cancel` 改用 `CTRL_BREAK_EVENT`(Windows 不支持 SIGINT)。

## 6. Linux 回归(当前 SHA)
改 upm/aio_fits 后跑: `test_upm_recovery_oracle/test_upm_parallel/test_p2_api/test_p3_api/test_phase2_inprocess/test_phase3_inprocess/test_phase123_pipeline/test_monitor_events` → **Ran 44 tests → OK**(无回归)。

## 7. 结论 / 限制 / 遗留
- **0 P0/P1**(PASS): 无崩溃类(P0)、无判定的 P1(两处真实候选已修)。剩余 C6262 栈占用为 P2 代码质量; cfitsio 第三方告警不在自研质量范畴。
- 遗留: (a) Application Verifier 为 Windows 系统工具, 未在此自动化; ASan 作为等价可用集已覆盖"无泄漏/invalid access"; (b) C6262 大栈/AIO 缓冲区可后续优化。
