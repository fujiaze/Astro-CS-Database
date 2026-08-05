// ============================================================================
// json_config.h - Stage1 JSON 配置解析与 Schema 验证
// 功能: 解析 stage1.json 配置文件, 使用 nlohmann-json-schema-validator v2.4.0
//       进行正式 Draft 2020-12 Schema 验证 (单一权威, 无手写重复规则)
//       typed Stage1Config 直接驱动各 stage (无 compat flat JSON 桥)
//
// 设计说明:
//   - 唯一入口: orchestrator.exe <stage1.json>
//   - JSON 中的相对路径基于 JSON 文件所在目录解析为绝对路径
//   - Schema 验证规则与 stage1.schema.json (v1.1) 一致
//   - 验证通过后, config 中所有路径字段均为绝对路径
// ============================================================================

#pragma once

#include <string>
#include <map>
#include "orchestrator.h"  // for PrecisionMode

// Stage1 JSON 配置 (对应 stage1.schema.json)
struct Stage1Config {
    std::string schema_version = "1.1";
    std::string pipeline = "stage1";
    PrecisionMode precision = PrecisionMode::FP32;
    std::string gaia_data_dir;             // 必填: Gaia DR3SP 光谱数据库目录 (配置引入)

    struct Input {
        std::string light;          // 必填, 非空
        std::string master_bias;    // 可为空 (null -> "")
        std::string master_dark;    // 可为空
        std::string master_flat;    // 可为空
    } input;

    struct Calibration {
        std::string mode = "standard";       // standard/optimal/exposure_ratio
        double light_exposure_s = 0.0;       // > 0
        double dark_exposure_s = 0.0;        // 0 表示 null
        std::string fallback = "exposure_ratio"; // exposure_ratio/fail
    } calibration;

    struct PlateSolve {
        std::string gaia_catalog;            // 必填, 非空
        int max_stars = 2000;                // >= 1
        double initial_ra_deg = -999.0;      // -999 表示 null
        double initial_dec_deg = -999.0;     // -999 表示 null
    } platesolve;

    struct Psf {
        int fit_radius = 8;                  // >= 0
        int max_iterations = 100;            // >= 1
        double tolerance = 1e-6;             // > 0
    } psf;

    struct Photometric {
        std::string gaia_spectra;            // 必填, 非空
        std::string filter_response;         // 必填, 非空
        std::string qe_curve;                // 必填, 非空
    } photometric;

    struct Snr {
        int estimator_id = 1;                // >= 0
        double sampling_scale = 1.0;         // > 0
    } snr;

    struct Drizzle {
        std::string mode = "precise";        // const "precise"
        double pixfrac = 1.0;                // (0, 1]
        std::string nside_mode = "auto";     // "auto" 或 "explicit"
        int nside_value = 0;                 // 仅 explicit 模式, [16, 4194304]
        std::string ordering = "nested";     // const "nested"
    } drizzle;

    struct Output {
        std::string hiss;                    // 必填, 非空
        std::string log;                     // 必填, 非空
        std::string diagnostics_dir;         // 必填, 非空
        bool overwrite = false;
    } output;

    struct Execution {
        std::string stop_after = "hiss_verify";
        int threads = 0;                     // >= 0
        std::map<std::string, double> stage_timeout_sec;
    } execution;

    // 元数据 (解析后填充)
    std::string config_sha256;       // 规范化配置 SHA256
    std::string original_json_sha256; // 原始 JSON 文本 SHA256
    std::string original_json_path;   // 原始 JSON 文件绝对路径
};

// 解析 stage1.json 并严格 Schema 验证
// 相对路径基于 JSON 文件所在目录解析为绝对路径
// 返回 0=成功, <0=失败 (error_msg 填充)
int parse_stage1_config(const std::string& json_path, Stage1Config& config, std::string& error_msg);

// 验证 JSON 是否符合 stage1.schema.json (不填充 Stage1Config)
// 返回 true=通过, false=失败 (error_msg 填充)
bool validate_stage1_schema(const std::string& json_path, std::string& error_msg);

// 打印 Schema JSON (用于 --print-schema)
std::string get_stage1_schema_json();

// 计算内嵌 Schema JSON 的 SHA256 (与磁盘 schema 一致性测试用)
std::string get_stage1_schema_sha256();

// 计算规范化配置 SHA256 (基于序列化后的规范 JSON 文本)
std::string compute_config_sha256(const Stage1Config& config);
