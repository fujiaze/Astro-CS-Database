# Phase3 API 定义 v1 (API-005 冻结 — request/result/HiPS source/WCS output/sampler/coverage/FITS)

> ID: API-P3-001  范围: API-P3-001..006  状态: FROZEN (V5 API-005, 2026-08-28)  上游: API-001/002/004, ALG-P3-001..004, ARCH-P3-001  下游: CLI-002(phase3 run handler)/CODE-P3/SYN-007
> Phase3 为新建模块:本文件=施工规格合同;实现(CODE-P3)必须逐字段落地,布局测试随 ABI-001 模板建立。

## 1 生命周期(与 p1/p2 session 同构)

```c
acs_status p3_session_create(const astrocs_host_services_v1* host, acs_handle* out);
acs_status p3_session_validate(acs_handle, const acs_span_u8 request_json);   /* 纯校验, 无 IO; 显式拒清单全查 */
acs_status p3_session_run(acs_handle, const acs_span_u8 request_json);        /* 取消点=行带; fits 原子写 */
acs_status p3_session_inspect(acs_handle, acs_span_u8* out_result_json);      /* host alloc, 调用方释放 */
acs_status p3_session_destroy(acs_handle);
```

## 2 request(JSON 字段, schemas/phase3_request_v1.schema.json)

| 字段 | 类型/单位 | 约束(ALG-P3/SCI-P3 冻结) |
|---|---|---|
| `source.hips_dir` | UTF-8 path | 必须含合法 properties(ALG-P3-001) |
| `center` | {ra_deg, dec_deg} ICRS | `abs(dec)<=85°`;输出四角同半球 |
| `scale_deg_per_px` | deg/px | >0 |
| `width_px`/`height_px` | px | ∈[1,20000] |
| `projection` | 枚举 | **仅 "TAN"**,其他→UNSUPPORTED |
| `sampler` | 枚举 | "nearest"|"bilinear"(默认 bilinear, SCI-P3 §9a-7) |
| `longitude_parity` | 枚举 | "east_left"(默认, CD1_1<0)|"east_right" |
| `bitpix` | 枚举 | -32|-64 |
| `coverage_output` | 枚举 | "mask"(二值) 单值;通道/weight 模式不存在 |
| `max_tiles` | int | 可降不可升超内存守卫(ARCH-P3 §3) |

## 3 result(inspect JSON)

`{run_id, exit_code, output_fits_path, sha256, order_sel_used, sampler_used, provenance{hips_id, manifest_hash, missing_tiles[], software_version}, coverage_stats{covered_px, total_px}, timings}`;输出 FITS 本体=原子产物(S+C 合成或 COV 扩展, 由 CODE-P3 按 04/API-002 manifest 落实, 二选一在 CLI-002 前冻结)。

## 4 显式拒绝清单(输入不明确→确定错误, 禁猜测)

| 输入 | 错误 |
|---|---|
| projection≠TAN | ACS_ERR_UNSUPPORTED |
| frame≠ICRS 恒等(galactic/ecliptic) | ACS_ERR_UNSUPPORTED |
| 多通道/RGBA、JPEG/PNG lossy tile、int+BLANK | ACS_ERR_UNSUPPORTED |
| variance/weight/ivar/flux-per-pixel 输入模式 | ACS_ERR_UNSUPPORTED |
| properties 非法/缺键 | ACS_ERR_PARAM |
| abs(dec)>85°(距极点<5°)/输出跨 TAN 半球/W/H 越界 | ACS_ERR_PARAM |
| tile 文件缺失 | **非错误**:coverage=0+provenance.missing(SCI-P3 §8) |
| tile 内 NaN | 非错误:S=NaN+C=1(§8) |
| IO/运行失败 | ACS_ERR_IO/安全中止(ARCH-P3 §4) |

## 5 逐 claim 追溯

| API 声明 | 锚 |
|---|---|
| request 字段约束 | ALG-P3-002 / ALG-P3-003(G1/G3)+SCI-P3 §9a-4/5 |
| 拒绝清单 | SCI-P3 §9a-1/8/10+§8(逐行同源) |
| sampler 默认 | SCI-P3 §9a-7(bilinear) |
| result/provenance | ALG-P3-004(G5)+API-002 final 事件 |
| 取消/原子性 | ARCH-P3 §3(行带+整文件原子) |

## 6 机器门

tests/api/test_p3_api.py: 生命周期五函数/request 十字段/拒绝清单与 SCI-P3 文本同源交叉核对/锚点齐。
