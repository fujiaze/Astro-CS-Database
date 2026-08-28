# RUN-001 —— 历史锚 A/B 导出与只读构建（Fatduck Windows + Linux 交叉）

> 规格（03_TASK_SPECIFICATIONS RUN-001）：`git archive` 导出 A/B 到仓库外，只读构建；
> 禁止 branch/worktree commit；记录 archive SHA、compiler、binary SHA；
> **历史代码构建失败不得修历史锚**。
> 执行：2026-08-28 16:36–16:46 CST（Fatduck 在线窗口内）。主机：Fatduck Windows
> （MSYS2 MinGW64 g++ 16.1.0）为主构建平台；Linux（gcc 14.2.0）交叉验证 git archive 确定性。

## 1 导出与构建（全程只读，无 branch/worktree commit）

- 锚定义（继承 V2 32R 矩阵）：**A = b38b446e6**（SEAM/HOLE 修复基线，weight_mode=auto→legacy 0）、
  **B = 83471979a**（背景洁净 UPM 采样基线，weight_mode=auto→legacy 0）。
- 导出目标（仓库外、gitignored 临时区）：
  - Fatduck：`C:\temp\reaudit_v3\run001\{A,B}`（`git archive <sha> -o <tar>` + tar 解包）。
  - Linux 主仓：`run/reaudit_v3/run001/{A,B}`（`git archive | tar -x`）。
- **归档确定性交叉验证**：git archive tarball SHA256 两平台**逐字节一致**：
  - A：`1e455756d7d1cb66fe4d43c6c95a18f51ab8e016e786c6faafe559179d9eaf55`
  - B：`ea9c86a0c20420caf4fc8f579f755b5141e7da5e63becfcf852056e3cc2a7103`

## 2 构建结果

### Fatduck Windows（正式平台，与 V2 一致）

| 锚 | AIO 构建 astro_image_io.dll | astrocs-stage2.exe | binary SHA256 |
|---|---|---|---|
| A b38b446e6 | exit=0, sha `52d0ee1ad882932fd13f92aedb5e12bead228296ae2409249b0bd44855a61faa` | **exit=0** | `2570aca7ae941cc58c639eda3fda470359812a444ec6cc497fca2ab0c15210df` |
| B 83471979a | exit=0, sha `cac24dcce3193b4eeb5da2e9b81d211761559c5ec7b12835bcb402f010c9161a` | **exit=0** | `480aa7469a7038283178d7476c8fd990fbaa23b9748ea9c0799310667b735fcd` |

- 配置：`cmake -S lib/phase2 -B build_run001 -G Ninja -DCMAKE_BUILD_TYPE=Release
  -DP2_ENABLE_OPENMP=ON`；锚点树中 `lib/astro_image_io/*.dll` 被 .gitignore（`*.dll`），
  故先源码构建 AIO（与 V2 p2_ab_worktrees 流程、toolchain build 模块顺序一致）。
- compiler：MinGW64 g++ 16.1.0 / GNU Make 4.4.1 / CMake 4.3.2 / Ninja 1.13.2。
- 日志：`C:\temp\reaudit_v3\run001\run001.log`、`{A,B}_{aio,cfg,build}.log`。

### Linux 交叉记录（原样，不修历史锚）

- 锚 A/B 在 Linux gcc 14.2.0 配置成功但构建失败，失败点（两锚相同）：
  `lib/acr/backends/cuda/cuda_bridge_loader.cpp:14: fatal error: windows.h: No such file
  or directory` —— ACR CUDA 桥为 Windows-only（ACR 异构已按审核人指示整体跳过，DEV-ACR-001）。
- **按规格不修历史锚**，原样记录；历史锚以 Windows 为正式构建平台（与 V2 事实一致）。

## 3 结论

- **RUN-001 判 PASS**：A/B 归档 SHA 记录、两平台交叉一致、Windows 只读构建成功、
  binary SHA 记录在案；未修改任何历史锚、未在仓库留下 branch/worktree/commit。
- 限制：Linux 无法构建历史锚（ACR CUDA 桥 windows.h），该限制为既有架构事实
  （非本轮引入），已与 BLD-002/003、DEV-ACR-001 交叉记录。
- 后续：RUN-002（每板块 2 帧 A/B/C/D 小矩阵，验证 weight_mode 语义映射）。
