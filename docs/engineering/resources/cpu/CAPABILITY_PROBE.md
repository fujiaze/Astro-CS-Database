# CPU 能力探测

能力层区分硬件特征hw与OS可安全执行集合os_safe，不负责核心数预算或profile写盘。实际实现为`lib/infrastructure/benchmark/cpu/common/src/capability_detect.c`，公开头在同目录树`include/acsd/cpu/capability_v1.h`。

## 探测与判定

先查询CPUID，再在OSXSAVE允许时执行XGETBV，记录XCR0；高级kernel调用之前核对required特征是否属于os_safe。不能让高级ISA污染检测/握手自身。provider需求按其真实编译/指令与独立位词表解释，不把backend和provider的整数掩码互代。

当前classify把SSE/BMI等按相应硬件位处理，AVX家族要求OSXSAVE及XMM/YMM状态，AVX-512分支还有整组子集和opmask/ZMM条件。准确逻辑以cap_classify的嵌套条件核对；单列0xE0不等于完整前提，也不能把项目整组策略当所有AVX-512硬件的普适定义。具体位值保持公开头，不在本页另列第二套数字表。

## 接口与输出

acsd_cap_detect_v1采集，acsd_cap_classify_v1可接受合成证据做判定，acsd_cap_os_safe_satisfies_v1检查required。POD前两字段struct_size/abi_version，错误码与该头对应，不跨边界托管分配。schema为common/schemas/cpu_capability.schema.json，包含CPU身份、CPUID、OS状态、hw/os_safe及判断；序列化稳定性与真实不同主机输出分别验证。

亲和性、cgroup/Job Object、可用核心及线程租约由Runtime资源层测量。本层的能力记录不证明当前模块已加载或执行该指令，也不证明profile参数已生效。

## 验证范围

合成CPUID/XCR0矩阵覆盖缺AVX、缺FMA、缺OS保存、缺子集和非法位，再与真实平台采集比较；合成输入应通过实际判定函数，独立期望需来自一手硬件规范。两平台同schema、实际探测与真实kernel运行分别报告。旧测试目录和矩阵是设计/历史记录，须先取得载体才能称已执行，原文保存于[来源档案](../../../../实验/documentation-refactor/docs/CPU_SOURCE.txt)。
