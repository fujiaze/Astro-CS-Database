# RT-007 PLAN — 类型化 ArtifactStore + 跨阶段绑定

## 需求 (04_TASK_SPECIFICATIONS.md RT-007)
Artifact descriptor 至少含 ID、role、schema/version、unit、coordinate、dtype/shape、validity、
path/URI、size/hash、producer node/module/version、source commit、input IDs/hashes、created UTC。
消费前验证完整；禁止从文件名/目录猜角色。P1 output 必须成为 P2 input，P2 HiPS 成为 P3 input。
篡改 path、hash、unit、schema 或换 producer 均硬失败。

## 现状证据
- DataArtifactDescriptor 有 id/schema/unit/coord/shape/provenance，但无 role、producer node/module/
  version、input IDs/hashes、size/hash 字段；无独立 store（RunContext 内嵌 map）。

## 修改
1. include/astrocs/core/artifact_store.h（新）：ArtifactRole 枚举 + ArtifactDescriptor（完整字段）+
   ArtifactStore（唯一 producer/role 绑定/篡改检测/并发安全）。
2. lib/core/src/artifact_store.cpp（新）：validate/to_json/from_json；store 唯一 producer；
   bind_as_input role 匹配；bind_p1_to_p2/bind_p2_to_p3。
3. CMakeLists.txt：astrocs_core 加 artifact_store.cpp。
4. tests/unit/rt007_artifact_store_test.cpp（新）：6 组测试。

## 科学影响
无（数据绑定层；不动科学公式）。

## 风险
- unit 以名称序列化（JSON roundtrip 需回读映射）→ 已实现单元映射。

## 验收命令
1. `cmake --build run/temp/build_v61 --target rt007_artifact_store_test` → build=0
2. `./run/temp/build_v61/tests/unit/rt007_artifact_store_test` → RT-007_PASS
3. `ASTROCS_REPO=$(pwd) ctest -R "core_|rt0"` → 14/14 PASS
4. `cmake --build run/temp/build_v61 --target astrocs` → CLI 链接成功
