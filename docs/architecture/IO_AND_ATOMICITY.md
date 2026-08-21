# I/O & Atomicity

- science product 写盘协议：temp write → validate → atomic promote。
- UPM sparse 模型：aio_upm_write_sparse 已于 V19R6R2 改为 temp+rename（lib/astro_image_io/src/aio_upm.cpp:60-97 temp write → validate → atomic promote，F-V19R2-IO-001 已修复）。
- HiPS tiles 当前仍非原子：remove → fits_create → write_chksum → close；partial-file 策略：abort 清理、finalize verify（失败清理 temp/partial，交付前 verify CHECKCODE/CHECKDATASUM）。
- dense cache：固定 512B 头部 + 二进制块 + streaming checksum；打开校验
  checksum 与 source_hash。
- HiPS 写：先 tiles/properties 到目标目录，最后 properties/index；
  verify（CHECKCODE/CHECKDATASUM）后交付。
- 只读：testdata/ 禁止写入；运行产物统一 run/。

## 契约

ENG-IO-001..003（S2 注册）。
