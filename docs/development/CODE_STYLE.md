# Code Style

- C++17；MSYS2 MinGW64 g++ 16.1.0；OpenMP 仅显式并行区。
- `.clang-format`（根目录，V14 落地）覆盖 first-party；third_party 不
  mass-format；`.editorconfig`（根目录）统一缩进/换行/编码。
- 命名：类型 `PascalCase`（C 结构 `P2`/`Aio` 前缀保留 ABI）、函数
  `snake_case`、成员 `snake_case_`（C++ 类）。
- include 顺序：本模块 → astrocs 头 → std；self-contained public header。
- C ABI：`extern "C"`、不抛异常、buffer ownership/lifetime/nullable/单位
  注释齐全；return/status 集中定义；logging 与 status 分离。
- 警告策略：`-Wall -Wextra` first-party 无新增警告。
