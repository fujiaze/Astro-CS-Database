# gaia_xpsd_client - 模块开发memory

## 模块职责
Gaia DR3/DR3SP星表C客户端，解析XPSD格式星表文件，提供锥形查询接口与多数据库支持，为plate_solve、photometric_calib等模块提供Gaia参考星数据。

## 当前版本
- 版本号：v1.0
- 最新commit：（稳定运行，未指定具体commit）
- 更新时间：2026-07-12

## GitHub仓库
- 仓库地址：https://github.com/fujiaze/Gaia-DR3-DR3SP-Client-C
- 默认分支：master

## 依赖列表
- C99（纯C实现）
- 无外部库（零依赖）

## 关键决策记录
- **XPSD格式解析**：自行实现XPSD二进制格式解析，避免依赖外部星表访问库
- **多数据库支持**：支持DR3与DR3SP两个数据库切换，通过初始化参数指定数据目录
- **内存缓存（60s TTL）**：锥形查询结果缓存60秒，避免重复查询同一区域的星表数据，提升批量处理性能
- **纯C接口设计**：导出C API（gaia_query_cone等），便于C++与Python（ctypes）双向调用

## 进度日志
### 2026-07-12 稳定运行
- 模块进入稳定运行状态，被plate_solve、photometric_calib、integration_test等模块依赖
- 配合Gaia驱动PSF流程优化（详见integration_test记录）：先查Gaia星表→投影到像素→均匀化采样→PSF只拟合采样星，45帧提速7.7x
- 推送至GitHub：commit 128eefd（integration_test记录中提及）
