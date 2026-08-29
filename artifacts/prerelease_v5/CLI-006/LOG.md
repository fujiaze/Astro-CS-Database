# CLI-006 执行日志 (2026-08-28, vm-bj)

## 输入
03_TASK_DETAILS CLI-006 行「接入 `phase3 run`，所有参数走 config/API，不在 CLI 复制算法；end-to-end Phase3 synthetic 命令 PASS」；API-P3-001(p3_session 5 段式)/ALG-P3-001..004/ARCH-P3-001。

## 动作
1. lib/phase3_session/p3_session.{h,cpp}(CODE-P3)。opaque handle, 五段式 create→validate→run→inspect→destroy, 与 p1/p2 同构(host services 注入, 不 shell-out)。
   - **create**: 校验 host struct_size/abi, nothrow 分配。
   - **validate**(纯读, 无 IO): 解析 request JSON + **显式拒清单全查**: 必填 source.hips_dir/center.ra_deg/dec_deg; projection≠"TAN"→UNSUPPORTED; frame 缺失=icrs, 显式非 icrs→UNSUPPORTED; |dec|<5°→PARAM; scale≤0→PARAM; width/height∉[1,20000]→PARAM; sampler∉{nearest,bilinear}→PARAM; longitude_parity∉{east_left,east_right}→PARAM; bitpix∉{-32,-64}→PARAM; coverage_output≠"mask"→PARAM。无 silent default。
   - **run**: WCS 构造(P3-002 p3_wcs_make)→order select(P3-003 p3_order_select, max_order 20 冻结内存守卫)→sampler open(P3-001 properties 严格校验)→行带采样(nearest/bilinear; 取消点=行带, cancelled_at_row≥0→ACS_ERR_CANCELLED)→provenance 组装→p3_output_write_atomic(P3-004, 原子 fall-passthrough)。
   - **inspect**: host allocator 分配 result JSON, 调用方释放。
2. cli/main.cpp cmd_phase3_run: 解析 config→host services 注入(cancel/logger/budget, 不分叉)→create→validate(错→ARGS)→run→inspect→写 run manifest→**错误映射**: PARAM→ARGS/UNSUPPORTED|STATE→SCIENCE/IO→IO/其余→INTERNAL; 取消→CANCELLED。所有参数一律从 config 透传, CLI 不复制算法。
3. cli/CMakeLists.txt: P3_SRCS glob + phase3 源编译选项/包含目录(common/healpix/aio/cfitsio/include/third_party)。
4. tests/cli/test_phase3_inprocess.py 6 测试(CLI-006): 生产路由 complete(事件+manifest+输出 FITS artifact)/projection≠TAN→2|dec<5°→2|width=0+scale=0→2(显式拒)/nearest+bilinear 两采样器都产出合法 FITS/--events-jsonl stdout 纯 JSON。

## 验证
- 全量回归 **unittest 194/194 OK**(新增 6)。
- 手工端到端: phase3 run 产出 /tmp/output_phase3.fits(17280B, 2 HDUs), 独立 CFITSIO 探针确认 CTYPE1=RA---TAN/CRPIX1=20.5/CRVAL1=210/coverage ext 40×30 @1200px 全覆盖; manifest status=complete+sha256。
- 显式拒: projection=SIN→2, dec=3°→2, sampler= bad→2; 合法→0。

## 明确拒绝清单(机器验证)
非 TAN/非 ICRS/|dec|<5°/W·H 越界/scale≤0/sampler 非法/parity 非法/bitpix 非法/coverage_output 非 mask → 2 或 UNSUPPORTED, 不进入运行。

## 限制与遗留
- P3 采样主循环为单线程串行(worker pool 注入/预算驱动并行化归 CODE-P3 后序或 P3-006, 本任务验收=参数走 API+end-to-end 命令 PASS, 不含并行)。
- CLI 侧未复制任何采样/WCS 公式(全部在 p3_* 库), 符合「所有参数走 config/API, 不在 CLI 复制算法」。

## 产物
lib/phase3_session/p3_session.{h,cpp}; cli/main.cpp(cmd_phase3_run+dispatch+include+global); cli/CMakeLists.txt(P3_SRCS); tests/cli/test_phase3_inprocess.py; 本日志。

## PASS 判定
phase3 run 端到端合成命令机器验证 PASS——properties→WCS→采样→原子 FITS writing, 参数走 config/API, 显式拒清单全查。CLI-006 = PASS。
