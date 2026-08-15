# AstroCS Error Handling Standard

## 三层语义

1. success（rc=0，状态=OK）；
2. recoverable science status（status 字段：UNDERDETERMINED、
   NO_CANDIDATES、ALL_REJECTED 等，产品仍有效或显式 fallback）；
3. hard error（rc≠0：CONFIG / INPUT_CORRUPT / DEPENDENCY / NUMERIC /
   NO_DATA / RESOURCE / TIMEOUT / IO / SCIENCE_GATE / INTERNAL）。

## 禁止

- `rc=0 + invalid status` 双语义；
- 以 warning/log 替代 error status；
- 默认 swallow 错误（catch(...) 空吞）；
- 静默截断/置零（损坏输入必须报错）。

## 每个 error-sensitive 模块

- diagnostics stage ID（P1.* / P2.*，见 DIAGNOSTICS_STANDARD）；
- stable error category/code；
- troubleshooting 条目（docs/diagnostics/TROUBLESHOOTING.md）。
