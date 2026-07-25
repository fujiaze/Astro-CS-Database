# 08 配置与构建 Spec

## 1. 目标环境基线

当前主目标：Windows、PowerShell 7、MinGW64、C++17、OpenMP、64GB 内存、Qt6 浏览器。

P01 要生成实际环境锁定文件，禁止只写“MinGW64”而没有版本。

## 2. 配置分层

```text
config/defaults.json
config/toolchain/<id>.json
config/datasets/<dataset_id>.json
config/runs/<run_name>.json
```

解析优先级固定，最终输出 `config.resolved.json`。

## 3. 机密与路径

- 项目配置不含本机绝对路径；
- Gaia 与大数据目录通过环境变量或本地未跟踪配置提供；
- 本地配置模板进入 Git，真实路径文件不进入 Git；
- 日志中避免输出无关个人路径信息。

## 4. 统一构建顺序

建议依赖顺序：

1. astro_image_io（含唯一 PipelineFrame/HEALPix I/O）；
2. gaia_xpsd_client；
3. star_detector；
4. calibration；
5. plate_solve；
6. dynamic_psf；
7. photometric_calib；
8. snr_estimator；
9. healpix_drizzle；
10. healpix_stack；
11. orchestrator；
12. healpix_browser_qt。

实际顺序以 P01 生成的依赖图为准。

## 5. 构建预设

至少提供：

- `dev-debug`：断言、符号、较低优化；
- `dev-release`：日常性能验证；
- `asan` 或 Windows 可用的内存检查替代方案；
- `release`：可发布产物；
- `browser`：Qt 浏览器。

## 6. 构建验收

干净 clone 后：

- bootstrap 成功；
- 每个模块构建日志独立；
- 产物都进入统一目录；
- DLL 依赖可解析；
- `orchestrator --version` 成功；
- smoke tests 成功；
- 重复构建无无关变更；
- 构建产物 manifest 和 SHA-256 完成。

## 7. 超时

- Python `subprocess.run/Popen` 必须设置 `timeout`；
- Agent 执行编译/测试时必须设置合理外部超时；
- 超时视为失败并保存部分日志，禁止无限等待；
- 超时阈值写入测试/任务报告。
