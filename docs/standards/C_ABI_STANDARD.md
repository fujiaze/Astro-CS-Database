# AstroCS C ABI Standard

- 异常禁止越界：C 边界函数内 try/catch 全包裹（如 p2_upm_open）。
- 输出在失败时重置：`*out_model = nullptr` 语义由实现保证；调用方不得依赖
  半初始化对象。
- partial allocation 单出口 cleanup：RAII / delete 于失败路径。
- 句柄类型：`void*` 只允许 opaque handle，ownership 文档化。
- 字符串输出：传入容量，写 NUL；禁止无界 strcpy。
- 整数参数：checked（count/offset 与 buffer 容量匹配）。
- 错误返回：0=success，非 0=hard error；recoverable science status 经
  输出状态字段表达，不得用 rc 双语义。
