# P3-001 执行日志 (2026-08-28, vm-bj)

## 输入
03_TASK_DETAILS P3-001 行(严格解析 HiPS properties;验证 order/tile width/format/frame;安全路径和缺 tile;验收=synthetic tiles:合法/缺失/恶意路径/边界 order;无 silent default); PHASE3_API_V1(API-P3-001 source.hips_dir 必须含合法 properties)。

## 动作
1. lib/phase3_session/hips_properties.{h,cpp}: 严格解析器+校验器——hips_properties_parse(键=值 行; #/空行忽略; **重复键=错误**; 必需键全查: hips_order/hips_tile_width/hips_tile_format/hips_frame/dataproduct_type; 值域: order∈[0,20](ARCH-P3 §3 内存守卫)/tile_width 恒 512/format 须含 fits/frame∈{equatorial,icrs}/dpt=image; **无 silent default**)。hips_product_validate: 路径安全(拒 .. / 反斜杠 / 空段 / NUL)→properties 解析→**缺 tile 探测**(Norder<order>/Dir*/Npix*.fits 至少 1)。path_is_safe 共享工具。
2. tests/backend/hips_properties_probe_main.cpp: 探针(validate/parse/path 三模式, FAIL+原因)。
3. tests/backend/test_hips_properties.py 6 测试: 合法目录 OK(**12 基元实测 fixture 输入**)/必需键逐个缺失全拒(5 键)/值非法全拒(order 负/99/非数/浮点, width 256, format jpeg, frame galactic, dpt catalog, 重复键, 无等号)/**边界 order**(0 与 20 合法, 21 拒)/缺 properties+缺 tile 拒/恶意路径(..、反斜杠、空段、空)拒+正常路径过。
4. 顺修: test_bench_cli 的 commit 比对改用二进制内嵌构建 commit(--version +g<hash12>→git 全哈希; 原 HEAD 比对在提交后必然漂移, 非缺陷)。

## 验证
- 全量回归 unittest **170/170 OK**(新增 6)。
- 用真实 fixture(CLI-005 合成 HiPS)反向核对: F1.hips/signal 校验 OK(order 0/512/fits/equatorial)。

## 限制与遗留
- 解析器为 p3_session(CODE-P3)的 source 校验层; MOC 一致性/像素变异指纹属 P3-002+。
- kMaxOrder=20 冻结为内存守卫初值, ARCH-P3 §3 的 max_tiles 降级逻辑在 session 层实现。

## 产物
lib/phase3_session/hips_properties.{h,cpp}; tests/backend/{hips_properties_probe_main.cpp,test_hips_properties.py}; 本日志。

## PASS 判定
严格解析(必需键/值域/重复键/边界 order)全机器验证; 安全路径与缺 tile 探测实现且实测; 无 silent default(逐键缺失拒绝)落锤。P3-001 = PASS。
