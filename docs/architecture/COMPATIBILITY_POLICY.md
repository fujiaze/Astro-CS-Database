# Compatibility Policy

> 上游：ASTROCS_DESIGN.md §8（软件架构）

## 原则

科学正确性优先于透明兼容；ambiguous/损坏旧文件显式拒绝，判据取自文件自述字段。

## UPM 持久化（DATA-UPM-MODEL-001）

- 历史 writer（v1/v2）frames 列表来自 std::map（升序），build 帧索引同样
  升序（std::set）→ 旧文件自洽，可直接读取（方案 A 安全迁移）。
- 读取时强制校验：frames 唯一/类型、C 行数==帧数、控件字段类型；
  畸形文件稳定报错（ERR-P2-UPM-001）。
- 绑定只由 frame_id_by_index 决定；有序容器遍历顺序与绑定取值无关。
- 旧签名保留仅为兼容：`p2_reject_stack` 为 COMPAT adapter，生产用 `p2_reject_stack_ex`（含 eligibility/large_scale）。

## 配置/接口

- 接口冻结（W2/V15/V19）后行为变化必须 Contract first。
- config default 两处不一致视为缺陷（S8 机器检查）。

## 版本

- FITS/HiPS/protocol 版本号允许出现在注释/文档；开发轮次版本号只进
  版本说明/ADR。
