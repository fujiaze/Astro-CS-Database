# CLI 命令、协议与兼容性合同

## 1. 必须实现的命令

```text
astrocs --version --json
astrocs hardware inspect --json
astrocs config init --output pipeline_config.json
astrocs config validate --config pipeline_config.json
astrocs config show-effective --config ... --cpu-profile ... --json
astrocs benchmark cpu --quick|--full --output cpu_profile.json
astrocs doctor --json
astrocs test synthetic --group <all|calibration|wcs_psf|noise_snr|drizzle|upm|rejection_integration|pipeline>
astrocs phase1 run --config ... [--cpu-profile ...] [--events-jsonl]
astrocs phase2 run --config ... [--cpu-profile ...] [--events-jsonl]
astrocs phase3 run --config ... [--cpu-profile ...] [--events-jsonl]
astrocs run --phases 1,2,3 --config ... [--cpu-profile ...] [--events-jsonl]
astrocs verify --run-manifest ... --json
```

不得另外发布 benchmark exe。允许构建树生成仅供测试的内部 benchmark harness，但正式用户入口必须是 `astrocs benchmark cpu`。

## 2. 退出码冻结

| code | 含义 |
|---:|---|
| 0 | 成功，且所有请求门禁通过 |
| 2 | CLI 参数或配置错误 |
| 3 | 输入缺失、格式或 hash 错误 |
| 4 | 科学验证/数值不变量失败 |
| 5 | backend ABI、签名、CPU 特征或加载失败 |
| 6 | 计算执行失败 |
| 7 | I/O 失败 |
| 8 | 输出完整性/验证失败 |
| 9 | 用户取消或超时 |
| 10 | 资源利用率或内存增长门禁失败 |
| 70 | 未分类内部软件错误；必须生成 crash report |

Windows/Linux 同一版本不得给同一失败返回不同退出码。

## 3. stdout/stderr

- 人类模式：stdout 为简洁结果，stderr 为日志/诊断。
- `--json`：stdout 只能有一个 JSON 文档。
- `--events-jsonl`：stdout 每行一个 UTF-8 JSON 事件；不得夹杂普通文字。日志进入 stderr。
- 所有路径在 JSON 中为 UTF-8；Windows 内部正确处理 Unicode 路径。

## 4. JSONL 事件 v1

每行必含：`schema_version,event_id,run_id,timestamp_utc,sequence,kind,severity,phase,stage,message`。

按事件类型增加：

- `progress`：`completed,total,unit,rate,eta_seconds`；
- `resource`：`cpu_cores_used,rss_bytes,io_read_bytes,io_write_bytes,threads`；
- `artifact`：`role,path,sha256,size_bytes`；
- `backend`：`kernel,backend_id,isa,workers,block_size,reason`；
- `final`：`exit_code,status,run_manifest,summary`。

`sequence` 从 0 单调递增。所有重计算 stage 必须发 `stage_start/stage_end` 和实际 backend 事件。未来 GUI 不得链接科学库绕过 CLI，而只消费此协议。

## 5. 取消与崩溃

- Ctrl-C/Windows console cancel 设置协作取消令牌；kernel 在文档规定的安全点检查。
- 取消后关闭 writer、写入 incomplete manifest、删除/隔离未完成临时产物，返回 9。
- 不得留下看似完整的 HiPS/结果。
- 未捕获异常必须转换为 70，带 run_id、阶段和最小脱敏 crash report；不得泄露凭据。

## 6. 机器化接口一致性

仓库必须有 schema 和检查器，至少验证：

1. `--help` 命令树与本文件完全一致；
2. JSON/JSONL 对 schema 有效；
3. 退出码常量只在一个源定义；
4. 每个 CLI handler 可追到 Phase API；
5. 发布 manifest 不含旧 Phase exe；
6. Windows/Linux golden command tests 的字段与退出码一致。

