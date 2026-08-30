# Linux / Fatduck Windows 执行规范

## 1. 节点角色

### Linux 2c2g 常在线节点

负责控制、代码/文档一致性、GCC/Clang 轻量构建、静态分析、合成 Oracle、sanitizer 定向测试、任务状态与审核包。不得因资源小就把生产 heavy 路径写成串行；应使用规模可控但持续≥10s 的合成 workload 验证两个核。

### Fatduck Windows 远程节点

负责 MSVC 正式构建、多 ISA benchmark、Windows 合成套件、少量真实冒烟、最终 32R、HiPS 浏览器和发布产物。Windows 离线时仅相关 task 进入 `WAITING_WINDOWS`。

## 2. Linux 固定流程

1. 记录 `/etc/os-release`、kernel、CPU、memory、filesystem、cgroup/affinity、compiler/CMake/Ninja/Clang versions。
2. 依赖缺失先记录 prerequisite；只安装项目明确允许的包，不升级整机。
3. build dir 置仓库外或 `.gitignore` 内：`build/gcc-release`、`build/clang-debug`、`build/sanitizers`。
4. 每条外部命令使用 `timeout`；长命令 `tee` 日志并定期输出。
5. 先模块 test，再影响链；并发编译按可用 RAM 限制，避免 2GiB OOM。
6. heavy synthetic 与 monitor 同启；资源证据属于 task result。
7. 不把 build、原始帧、完整 HiPS 放进 git/审核包。

建议默认 timeout（可按已测耗时向上调整并记录）：

| 操作 | timeout |
|---|---|
| git fetch/单文件 SCP | 5 min |
| configure | 10 min |
| 2c2g build | 60 min |
| 单元/合成 test group | 30 min |
| sanitizer group | 60 min |
| Linux 少量真实 smoke | 120 min |

## 3. Fatduck 数据获取

- Linux 只按 `SMALL_REAL_DATA_REQUEST.json` 请求三块各最多一帧 R 与必需 masters。
- 远程先生成 path/size/SHA-256 manifest，再传；Linux 重算 hash。
- 禁止把 Fatduck 全 testdata 克隆到 repo 或审核包。
- 绝对路径只保存在本机运行配置，不写生产示例；文档使用变量/占位符。
- 网络中断可续传但必须验证最终 hash；所有脚本含 connect、command、idle、total timeout。

## 4. Windows 在线探测状态机

```text
UNKNOWN → probe(只读, 30s)
  ├─ offline → WAITING_WINDOWS；继续 Linux
  └─ online  → validate identity/toolchain/disk/data
                 ├─ mismatch → BLOCKED_WINDOWS
                 └─ ready    → build/test/benchmark
```

不得反复高频轮询。每个工作阶段最多按预设退避探测，报告最后时间与错误；不能让 SSH 无 timeout 卡住主 Agent。

## 5. Windows 固定流程

1. 只读探测 hostname、OS、CPU、RAM、disk、VS/MSVC/CMake、repo path、testdata manifest。
2. `git fetch` 后确认 `main/origin/main` 与 Linux 冻结 candidate 相同；不得在 Windows 产生未 push 的不同代码。
3. clean MSVC Debug/Release；ACR OFF；baseline provider 必须存在。
4. `doctor` 验证 CFITSIO reentrant、provider ABI、runtime/profile。
5. 运行 synthetic tests，再 benchmark，再小真实；全部通过才 32R。
6. 32R 运行全程 monitor；输出到新 run directory，不覆盖历史结果。
7. 生成技术预览和锁定 viewer state；大 HiPS 留本机，审核包只含 manifest/小预览。
8. 从同一 commit 生成 Windows 发布包和 checksums。

## 6. 最终 32R 唯一运行合同

- 三板块数量：11 + 11 + 10 = 32 R；masters 单独列，不计 light 数。
- 每帧 path、size、SHA、panel/frame identity、exposure、filter 固结。
- Phase1 对 32 帧逐帧产出/验证 HiPS；不得用三张旧 HiPS 冒充。
- Phase2 input manifest 必须引用这 32 个本轮 Artifact IDs/hashes。
- Phase2 输出：mosaic HiPS、support、明确类型的 weight/variance、UPM、rejection diagnostics、resource trace。
- Phase3 输入引用本轮 Phase2 mosaic Artifact；输出平面 FITS、coverage、WCS verify。
- 成功定义是所有 node completed、Artifact verify、资源门通过；仅 exit 0 不足。

## 7. HiPS 视觉审核

Agent 准备但不代替负责人判断：

- 固定坐标/FOV/rotation/stretch/color map；保存 `VIEWER_STATE.json`。
- 至少视图：三个 overlap seam、低背景、亮星/星云、support 边界、卫星线、全景、Phase3 对应区域。
- 每视图附 before/after UPM（同 stretch）、support/rejection overlay 和 seam 数值。
- 不得单独自动拉伸每张图后比较，否则会隐藏接缝。
- 负责人最终记录 ACCEPT/REJECT 与具体视图 ID。

## 8. 减少等待和人工干预

- Windows offline：完成所有 Linux/G0–G9 可做任务、准备 remote scripts/config/manifests。
- Windows 一上线：按 WIN-001→006 连续执行，不为普通 checkpoint 停止。
- 仅最终视觉审核需要负责人；中间报告以机器 Gate 自动推进。
- 任何失败先用最小 synthetic reproduce；不让用户手工看 CPU 曲线或反复点运行。
