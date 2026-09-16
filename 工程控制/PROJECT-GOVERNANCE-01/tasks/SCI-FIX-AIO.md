# 任务：SCI-FIX-AIO AIO/HiPS 勘误与 ABI 版本化（R-7 结论落地）

状态：NOT_STARTED　层：L1　依赖：R-7（已完成）、AIO-001（已完成）　互斥组：S10-B（aio 实现 + 公开头 + 文档 + 共址测试）

## 依据（负责人指令：文档必须正确，不要遵循旧制）

证据：reports/PROJECT-GOVERNANCE-01/research/R-7_AIO_HIPS_ABI与精度.md（含 IVOA HiPS 1.0 原文抽文 + 12 个真实 CDS tile）+ run/PROJECT-GOVERNANCE-01/R-7/**。

## 逐条要做

| # | 订正 | 落点 |
|---|---|---|
| 1 | M2b-A-02 是勘误不是裁决：docs/algorithms/HIPS_WRITER.md:146 的 NSIDE=2^k 改为 2^(k+9)（IVOA HiPS 1.0 REC §4.2.1 公式 + 表 5 三值逐位复算 + 12 个真实 tile 全无 NSIDE 卡；且同段 :142 自己就写了 2^(k+9)、DATA_SEMANTICS:327 与 IO_002:67/:104 已冻结该口径）。写侧/读侧不动 | 该文档一行 |
| 2 | V11-N-01 ABI 原位版本化：给 AioHipsSnrPoint、AstroSphereTileView、AioHipsDiagTileView、AioHipsTile 首部加 struct_size/abi_version，并照仓内样板 ipv_abi_mirror.py + ipv_abi_layout_probe.cpp + ipv_abi_layout_lock.py（含 --selfcheck 变异负例）建「C 探针 + 唯一 Python 镜像 + 逐字段布局锁」。依据：实测 C=40B vs 镜像=32B，按镜像语义传 3 点会静默写错数据（第 3 条 snr 落进 ra_deg 槽、越界读 24B、rc 仍 0）；且 hips_direct_smoke.py:43 硬编码的旧 DLL 路径在迁移后已不存在 ⇒ 破窗代价为零。同时修该 smoke 的旧 DLL 路径 | aio 公开头 + 镜像 + 新门 |
| 3 | M2b-H-01 改精度不改容差：properties 与 manifest 共用同一格式化函数，取 %.17g；<1e-9 容差保留。并修那条恒绿的门：p1hips_tests_oracle.cpp:130 取 3/12=0.25，任何 ≥1 位小数都精确 ⇒ 对序列化精度零判别力（同文件另一断言又把 1/12 钉成 6 位 0.083333，误差是容差的 333 倍） | 实现 + 门 |
| 4 | M2a-H-3 层级归约改用未钳制的真实覆盖面积（flux_n += sig·a；area_n += a），sup 只在发布时钳一次；I2（support≤1）保留；新增可观测钳制计数；DATA_SEMANTICS §20.3 的「F=signal×support×A_cell 闭合」补编码限声明（需 claim）。依据：异质覆盖下真面积加权 sig=8.02 而生产 sig_p=5.05 ⇒ 父级通量损失 74.813%，其中可避免损失 37.032%；修复对 c≤1 既有产品只造成 4.994e-16 重结合差。并修 p1hips_tests_properties.cpp:95 的同源 oracle（逐字复刻生产公式 ⇒ 恒真） | 实现 + 门 + 文档 |
| 5 | M9-F-3 drizzle_scale_arcsec 合法域：!isfinite→2；!(x>0)→2；x>824.5167388361774″→2；每次拒绝必须 set_error；删除「>0 才写」的静默省略分支。上界由仓内冻结常量推导（nside≥512 ⇒ 最粗叶像素 412.258369″ × SCI-DRZ-001 的 1–2× 过采样）。不要用 k_corr 的 [300,600]″ 当拒绝域。依据：负面矩阵只列 pixfrac、0 被接受但 finalize 静默不落键、两处 return 2 不带 set_error；全仓 5 个调用点无一处传 0 ⇒ 收紧无破坏面 | 实现 + 门 + 文档 |

## 登记转 CI-003（不要自己改）

- tools/check_abi_boundary.py 的 HEADERS 只扫 common_abi_v1.h + backend_host，aio_hips.h 不在面内（所以恒绿）；
- tools/quality/check_module_map.py:355 用「整目录任一 header 含 token」的 any()，被 aio_pipeline.h:84-85 顶掉 ⇒ aio 不报 header_missing_abi_version 且恒 rc=0。

## 硬纪律

1. 零 git 写；不得改 ci/**、.github/**、tools/**、docs/science/**、docs/algorithms/** 的其它内容；
2. 每条给「改前 → 改后 → 依据」；实现/门改动给「改前红 → 改后绿」（第 2 项必须给布局锁的变异自检：改字段顺序即红）；
3. ninja -C build -k 0 必须 0 FAILED；ctest -R aio|hips|p1hips 全绿；改被锚文档后复跑锚点门 rc=0；
4. 在 SCIENCE_CORRECTNESS.md 追加 claim SC-006；
5. 日志落 run/PROJECT-GOVERNANCE-01/SCI-FIX-AIO/logs/。

## 交付（中文，直白）

1. 逐条执行表；2. 布局锁与恒绿门修复的红→绿证据；3. 通量损失的改前/改后数值对照（应给出 37.032% → 0）；4. 未做项；5. 自证摘要。