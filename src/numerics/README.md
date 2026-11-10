# numerics

责任：区间、精确裁剪原语、QR/SVD/LSMR/MINRES、PSD/QP。依赖和算法正本：docs/engineering/rebuild/SYSTEM.md；字段/接口：include/acsd/contracts.hpp。本目录已有 CPU 实现源码；实现存在不等于科学/验收通过，当前裁决见[实施与验收状态](../../docs/audit/IMPLEMENTATION-STATUS.md)。

共享数值原语的唯一详细合同见[NUMERICS](../../docs/engineering/rebuild/standards/NUMERICS.md)，不为每个scalar helper建立process节点。
