# I/O & Atomicity

- science product 写盘协议：temp write → validate → atomic promote。
- UPM sparse 模型：aio_upm_write_sparse（当前直接 trunc+flush；findings
  F-V19R2-IO-001 挂账 S6 修复 temp+rename）。
- dense cache：固定 512B 头部 + 二进制块 + streaming checksum；打开校验
  checksum 与 source_hash。
- HiPS 写：先 tiles/properties 到目标目录，最后 properties/index；
  verify（CHECKCODE/CHECKDATASUM）后交付。
- 只读：testdata/ 禁止写入；运行产物统一 run/。

## 契约

ENG-IO-001..003（S2 注册）。
