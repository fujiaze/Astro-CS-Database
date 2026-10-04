# 兼容性策略

上游：最高设计的软件架构、I/O 与原子产品、版本与发布权三章[1]。

持久化模型、配置与接口的兼容性边界，以及不可透明兼容时的处置。

## 原则

- 科学正确性优先于透明兼容；判据取自文件自述字段，自述字段歧义或损坏的模型文件显式拒绝。

## UPM 持久化（DATA-UPM-MODEL-001）

- 模型文件按 frames 列表升序索引：写入端按升序落盘，读取端强制校验 frames 唯一/类型、
 C 行数==帧数、控件字段类型；畸形文件稳定报错（ERR-P2-UPM-001）。
- 绑定只由 frame_id_by_index 决定；有序容器遍历顺序与绑定取值无关。
- 生产排异入口 = `p2_reject_stack_ex`（含 eligibility/large_scale）；`p2_reject_stack` 为
 COMPAT 面，不参与生产接线。

## 配置/接口

- 接口冻结后行为变化必须 Contract first。
- config default 两处不一致视为缺陷：同一键在登记取值面与生产取值面出现两处不一致即判红[2]。
- FITS/HiPS/protocol 版本号允许出现在注释与文档；开发轮次版本号不进入程序、代码与产物
 [1]。

## 参考文献

[1] 内部文档 `docs/ACSD_DESIGN.md，最高设计`，上位来源。
[2] 内部文档 `docs/engineering/contracts/CONFIG.md`，同层相关正本。
