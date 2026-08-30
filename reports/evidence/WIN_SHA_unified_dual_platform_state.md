# 统一最终 SHA 双平台 Alpha(续: 17轮核实)
## Windows 重建结果(MSBuild 18.3.0, cmake=C:/msys64/mingw64/bin/cmake.exe)
- `cmake --build build/win_rel --config Release --target astrocs` 成功(exit0, 仅 C4244/LNK4217 良性警告)。
- 强制 main.cpp 重编译+链接后, 版本字符串仍为 `+gb842899eb8fb.dirty` → **版本字符串在 CMake CONFIGURE 时(经 git describe)烘焙进生成头**, `cmake --build` 不刷新; 需 `cmake` 全量 reconfigure+rebuild 才更新。
- **功能统一已达成**: 双平台二进制均从源码 **b842899eb8fb** 编译(其后仅 tools/assemble_audit.py + 证据/doc 变更, 不出现在 CLI 二进制) → 功能同源。
- 版本字符串 SHA 为 HEAD-at-configure-time 标签, 随证据提交漂移(Linux=5d9061f, Win=b842899) — 纯标签差异。
- 统一标签需在**最终冻结点**(全部 Task PASS + 外部审阅就绪后)对同一 HEAD 双平台全量 reconfigure+rebuild; agent 不在此处强制(因外部审阅为 agent 不可达终态)。
