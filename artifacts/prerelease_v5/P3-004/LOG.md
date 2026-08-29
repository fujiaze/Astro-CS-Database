# P3-004 执行日志 (2026-08-28, vm-bj)

## 输入
03_TASK_DETAILS P3-004 行(原子写 FITS、header/provenance/checksum、失败清理;用独立 FITS/WCS reader 重开;验收=header/data/hash/coverage PASS;取消不留完整假文件); ALG-P3-004(G5): BITPIX=-32/BSCALE=1/BZERO=0/BUNIT、WCS 全量、HISTORY 源/order_sel/sampler/软件版本/manifest hash、coverage 语义、原子(tmp+rename)。

## 动作
1. lib/common/crypto/sha256.{h,cpp}: 新增 **sha256_file**(分块流式文件哈希, 不整体载入; 共享增量 Sha256 复用)。
2. lib/phase3_session/p3_output.{h,cpp}:
   - P3OutputStatus{OK/PARAM/IO/CANCELLED}; P3Provenance{hips_id/manifest_hash/missing_tiles/software_version/run_id/order_sel_used/sampler_used}; P3OutputResult{sha256/coverage_ok/reopen_ok/covered_px/total_px}。
   - **p3_output_write_atomic**: tmp=out.<pid>.tmp → fits_create_file/create_img(BITPIX=-32)→写 WCS 关键字(CTYPE/CUNIT/CRPIX/CRVAL/CD/BSCALE/BZERO/BUNIT)+provenance 键(HIPSID/RUNID/ORDERSEL/SAMPLER/SWVER)+HISTORY(源/order_sel/sampler/软件版本/manifest hash)→写 signal(primary)→**取消行(cancelled_at_row≥0)→close+unlink(tmp)=不落盘**→追加 COVERAGE 扩展 HDU(EXTNAME=COVERAGE)→写 coverage→DATASUM→**fsync+rename 原子替换**(tmp 与 out 同目录保证原子)→sha256_file。
   - **p3_output_verify**(独立重开): CFITSIO 直接读—primary(HDU1) NAXIS/逐值回环(**NaN 语义: 双 NaN 视为一致, 其余逐值精确**)+扩展(HDU2) coverage 回环; 不与写路径共享缓冲。
   - 取消/失败 → 删除 tmp, 不留完整假文件。
3. 探针 tests/backend/p3_output_probe_main.cpp(write/verify 两模式; 半幅矩形非平凡 coverage; seed 驱动信号)。
4. tests/backend/test_p3_output.py 6 测试: write→verify 回环(header/data/hash/coverage all PASS+非平凡 coverage=(W/2)*(H/2)+**sha256 与文件字节一致**)/header 关键字与 provenance(HIPSID/Jy-beam/bilinear 等)/**取消不留完整假文件**(CANCELLED+无 out+无 .tmp 残留)/wrong-shape→reopen_ok=0 不误报/**float32 精确回环**/覆盖写新 checksum(原子替换)。

## 验证
- 全量回归 unittest **188/188 OK**(新增 6, 两次完整重跑稳定)。
- 冒烟: write=0 sha=... verify reopen=1 covok=1; 取消=CANCELLED 无残留。
- 实现期修复(必记): (a) `fits_read_pix`/`fits_write_pix` 的 `firstpix` 是 naxis 长数组非标量(标量→BAD_ELEM_NUM 308, 逐值 0); (b) `fits_write_key(TDOUBLE)` 需非 const double 副本(不能传 `&wcs->crpix_x` const 成员); (c) `fits_get_img_param` 签名 `(fptr,maxaxis,&imgtype,&naxis,nax,&status)`; (d) **verify 读取时 NaN!=NaN 误判** → 双 NaN 视为一致(源无覆盖=NaN, §4); (e) `aio_free_image_data` 会 free 结构体本身 → verify 改用 `aio_read_fits` 堆分配后修正, 最终改用纯 CFITSIO 直读(规避 movabs_hdu(2) 行为); (f) 测试读头窗口 2000→2880(ORDERSEL 恰在 2000 字节边界)。

## 限制与遗留
- DATASUM 为辅助 32-bit 校验; 权威校验=sha256_file(文件字节)。FITS 标准 CHECKSUM(含头)未做(非 V5 验收项)。
- 输出布局=primary signal + COVERAGE 扩展(API §3"二选一"之一); CODE-P3 按 API-002 落实合成/校验逻辑, 本任务只冻结落盘契约。

## 产物
lib/common/crypto/sha256.{h,cpp}(+sha256_file); lib/phase3_session/p3_output.{h,cpp}; tests/backend/{p3_output_probe_main.cpp,test_p3_output.py}; 本日志。

## PASS 判定
原子写(tmp+rename)/header(bitpix/bscale/bzero/bunit+wcs)/provenance(HISTORY+键)/checksum(sha256+datasum)/失败清理(取消无残留)全部机器验证; 独立 FITS reader 重开 data/hash/coverage 回环 PASS。P3-004 = PASS。
