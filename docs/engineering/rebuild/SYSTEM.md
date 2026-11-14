# 现有编译模块依赖（生成器兼容数据）

本页仅保留当前 typed 绑定生成器解析的真实模块依赖表；工程架构正本在 [ARCHITECTURE](ARCHITECTURE.md)，内存流水线见 [PIPELINE](PIPELINE.md)。这些行不意味着所有历史科学扩展都是本次实现目标。

| 当前模块源码目录 | 依赖及编译说明 |
|---|---|
| acsd_model / src/model | C++20；身份、单位、数组索引及 action recipe 数据，不含 I/O |
| acsd_numerics / src/numerics | model、Eigen、MPFR/GMP；区间、求积、QR/SVD/LSMR、PSD/QP |
| acsd_coordinates / src/coordinates | model、numerics、WCSLIB、ERFA；规范 WCS、历元、曲边 |
| acsd_observation / src/observation | model、operators、uncertainty、numerics、runtime；校准和G（模块组合走同一虚拟计算池的 science Context 桥） |
| acsd_catalogue / src/catalogue | model、coordinates、uncertainty；只读本地索引，Gaia/XP 解码 |
| acsd_sources / src/sources | observation、coordinates、catalogue、uncertainty、numerics；盲解、PSF、科学源 |
| acsd_response / src/response | model、numerics；响应基积分 |
| acsd_photometry / src/photometry | sources、response、uncertainty、numerics；联合尺度及影响 |
| acsd_geometry / src/geometry | coordinates、operators、numerics；唯一生产交叠与支持 Boolean |
| acsd_operators / src/operators | model、numerics；CSR、组合、transpose；消费几何数据，不调用 geometry |
| acsd_uncertainty / src/uncertainty | operators、numerics；根图、协方差、PSD 解 |
| acsd_information / src/information | uncertainty、numerics；V_F/V_B/controls 及三角重建 |
| acsd_sky / src/sky | uncertainty、information、operators；controls、全局相对 sky |
| acsd_coadd / src/coadd | sky、uncertainty、operators；选择和权及混合核 |
| acsd_projection / src/projection | geometry、operators、uncertainty；目标 Q |
| acsd_storage / src/storage | model、CFITSIO、zstd、nlohmann_json；块、products、发布、Array provider |
| acsd_resources / src/resources | model、numerics、Threads；资源子域/预算/租约、CPU KernelExecutor、Storage接口消费者、唯一线程池 |
| acsd_workflows / src/workflows | 上述全部；Normalize/Mosaic/Export 的因果编排与细化控制器 |
