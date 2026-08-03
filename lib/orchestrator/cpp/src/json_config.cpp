// ============================================================================
// json_config.cpp - Stage1 JSON 配置解析与 Schema 验证实现
// 使用 nlohmann/json 解析, 手动实现 Schema 验证 (nlohmann/json 不内置 JSON Schema)
// ============================================================================

#include "json_config.h"
#include "logger.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <iomanip>

namespace fs = std::filesystem;
using json = nlohmann::json;

// 前向声明 sha256_impl (定义在 cli_command.cpp, 链接时解析)
namespace sha256_impl {
std::string sha256(const std::string& input);
}

// ============================================================================
// 内嵌 Schema JSON (与 stage1.schema.json 一致)
// ============================================================================
static const char* STAGE1_SCHEMA_JSON = R"({
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "$id": "https://astrocs.local/schema/stage1-1.0.json",
  "title": "AstroCS Stage1 Job",
  "type": "object",
  "additionalProperties": false,
  "required": [
    "schema_version", "pipeline", "precision",
    "input", "calibration", "platesolve", "psf",
    "photometric", "snr", "drizzle", "output", "execution"
  ],
  "properties": {
    "schema_version": { "const": "1.0" },
    "pipeline": { "const": "stage1" },
    "precision": { "enum": ["fp32", "fp64"] },
    "input": {
      "type": "object", "additionalProperties": false,
      "required": ["light", "master_bias", "master_dark", "master_flat"],
      "properties": {
        "light": { "type": "string", "minLength": 1 },
        "master_bias": { "type": ["string", "null"] },
        "master_dark": { "type": ["string", "null"] },
        "master_flat": { "type": ["string", "null"] }
      }
    },
    "calibration": {
      "type": "object", "additionalProperties": false,
      "required": ["mode", "light_exposure_s", "dark_exposure_s"],
      "properties": {
        "mode": { "enum": ["standard", "optimal", "exposure_ratio"] },
        "light_exposure_s": { "type": "number", "exclusiveMinimum": 0 },
        "dark_exposure_s": { "type": ["number", "null"] },
        "fallback": { "enum": ["exposure_ratio", "fail"] }
      }
    },
    "platesolve": {
      "type": "object", "additionalProperties": false,
      "required": ["gaia_catalog", "max_stars"],
      "properties": {
        "gaia_catalog": { "type": "string", "minLength": 1 },
        "max_stars": { "type": "integer", "minimum": 1 },
        "initial_ra_deg": { "type": ["number", "null"] },
        "initial_dec_deg": { "type": ["number", "null"], "minimum": -90, "maximum": 90 }
      }
    },
    "psf": {
      "type": "object", "additionalProperties": false,
      "required": ["fit_radius", "max_iterations", "tolerance"],
      "properties": {
        "fit_radius": { "type": "integer", "minimum": 0 },
        "max_iterations": { "type": "integer", "minimum": 1 },
        "tolerance": { "type": "number", "exclusiveMinimum": 0 }
      }
    },
    "photometric": {
      "type": "object", "additionalProperties": false,
      "required": ["gaia_spectra", "filter_response", "qe_curve"],
      "properties": {
        "gaia_spectra": { "type": "string", "minLength": 1 },
        "filter_response": { "type": "string", "minLength": 1 },
        "qe_curve": { "type": "string", "minLength": 1 }
      }
    },
    "snr": {
      "type": "object", "additionalProperties": false,
      "required": ["estimator_id", "sampling_scale"],
      "properties": {
        "estimator_id": { "type": "integer", "minimum": 0 },
        "sampling_scale": { "type": "number", "exclusiveMinimum": 0 }
      }
    },
    "drizzle": {
      "type": "object", "additionalProperties": false,
      "required": ["mode", "pixfrac", "nside", "ordering"],
      "properties": {
        "mode": { "const": "precise" },
        "pixfrac": { "type": "number", "exclusiveMinimum": 0, "maximum": 1 },
        "ordering": { "const": "nested" },
        "nside": {
          "oneOf": [
            { "type": "object", "additionalProperties": false,
              "required": ["mode"],
              "properties": { "mode": { "const": "auto" } } },
            { "type": "object", "additionalProperties": false,
              "required": ["mode", "value"],
              "properties": {
                "mode": { "const": "explicit" },
                "value": { "type": "integer", "minimum": 16, "maximum": 4194304 }
              } }
          ]
        }
      }
    },
    "output": {
      "type": "object", "additionalProperties": false,
      "required": ["hiss", "log", "diagnostics_dir", "overwrite"],
      "properties": {
        "hiss": { "type": "string", "minLength": 1 },
        "log": { "type": "string", "minLength": 1 },
        "diagnostics_dir": { "type": "string", "minLength": 1 },
        "overwrite": { "type": "boolean" }
      }
    },
    "execution": {
      "type": "object", "additionalProperties": false,
      "required": ["stop_after", "threads", "stage_timeout_sec"],
      "properties": {
        "stop_after": { "enum": ["read", "calibrate", "platesolve", "psf",
                                  "photometric", "snr", "drizzle",
                                  "hiss_verify", "browser_verify"] },
        "threads": { "type": "integer", "minimum": 0 },
        "stage_timeout_sec": {
          "type": "object",
          "additionalProperties": { "type": "number", "exclusiveMinimum": 0 }
        }
      }
    }
  }
})";

std::string get_stage1_schema_json() {
    return STAGE1_SCHEMA_JSON;
}

// ============================================================================
// 辅助: 读取文件全部内容
// ============================================================================
static bool read_file_all(const std::string& path, std::string& content, std::string& err) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs.is_open()) {
        err = "无法打开文件: " + path;
        return false;
    }
    std::stringstream ss;
    ss << ifs.rdbuf();
    content = ss.str();
    return true;
}

// ============================================================================
// 辅助: 检查 JSON 对象是否有未知属性 (additionalProperties: false)
// 跳过以 "$" 开头的键 (JSON Schema meta-keyword 如 $schema/$id)
// allowed: 允许的键集合
// 返回: 第一个未知键名, 全部合法则返回 ""
// ============================================================================
static std::string find_unknown_keys(const json& obj, const std::vector<std::string>& allowed) {
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        const std::string& key = it.key();
        if (!key.empty() && key[0] == '$') continue;  // 跳过 $schema/$id 等
        if (std::find(allowed.begin(), allowed.end(), key) == allowed.end()) {
            return key;
        }
    }
    return "";
}

// ============================================================================
// 辅助: 检查 required 字段
// 返回: 第一个缺失字段名, 全部存在则返回 ""
// ============================================================================
static std::string find_missing_required(const json& obj, const std::vector<std::string>& required) {
    for (const auto& key : required) {
        if (!obj.contains(key)) return key;
    }
    return "";
}

// ============================================================================
// 辅助: 路径解析 (相对路径基于 base_dir 解析为绝对路径)
// 如果 path 已经是绝对路径, 直接返回; 否则拼接 base_dir / path
// ============================================================================
static std::string resolve_path(const std::string& path, const fs::path& base_dir) {
    if (path.empty()) return path;
    fs::path p(path);
    if (p.is_absolute()) {
        return fs::weakly_canonical(p).string();
    }
    fs::path full = base_dir / p;
    return fs::weakly_canonical(full).string();
}

// ============================================================================
// Schema 验证 (手动实现, 基于 stage1.schema.json 规则)
// 返回 true=通过, false=失败 (error_msg 填充)
// ============================================================================
static bool validate_schema_internal(const json& root, std::string& error_msg) {
    // 顶层必须是对象
    if (!root.is_object()) {
        error_msg = "根节点必须是 JSON 对象";
        return false;
    }

    // additionalProperties: false (允许的顶层键)
    static const std::vector<std::string> top_keys = {
        "schema_version", "pipeline", "precision", "input", "calibration",
        "platesolve", "psf", "photometric", "snr", "drizzle", "output", "execution"
    };
    std::string unknown = find_unknown_keys(root, top_keys);
    if (!unknown.empty()) {
        error_msg = "顶层未知字段: " + unknown;
        return false;
    }

    // required
    static const std::vector<std::string> top_required = {
        "schema_version", "pipeline", "precision", "input", "calibration",
        "platesolve", "psf", "photometric", "snr", "drizzle", "output", "execution"
    };
    std::string missing = find_missing_required(root, top_required);
    if (!missing.empty()) {
        error_msg = "缺少必填字段: " + missing;
        return false;
    }

    // schema_version: const "1.0"
    if (root["schema_version"] != "1.0") {
        error_msg = "schema_version 必须为 \"1.0\"";
        return false;
    }

    // pipeline: const "stage1"
    if (root["pipeline"] != "stage1") {
        error_msg = "pipeline 必须为 \"stage1\"";
        return false;
    }

    // precision: enum ["fp32", "fp64"]
    if (!root["precision"].is_string() ||
        (root["precision"] != "fp32" && root["precision"] != "fp64")) {
        error_msg = "precision 必须为 \"fp32\" 或 \"fp64\"";
        return false;
    }

    // --- input ---
    {
        const auto& v = root["input"];
        if (!v.is_object()) { error_msg = "input 必须是对象"; return false; }
        static const std::vector<std::string> keys = {"light","master_bias","master_dark","master_flat"};
        std::string u = find_unknown_keys(v, keys);
        if (!u.empty()) { error_msg = "input 未知字段: " + u; return false; }
        std::string m = find_missing_required(v, keys);
        if (!m.empty()) { error_msg = "input 缺少字段: " + m; return false; }
        if (!v["light"].is_string() || v["light"].get<std::string>().empty()) {
            error_msg = "input.light 必须是非空字符串"; return false;
        }
        // master_bias/dark/flat: string 或 null
        for (const auto& k : {"master_bias","master_dark","master_flat"}) {
            if (!v[k].is_string() && !v[k].is_null()) {
                error_msg = std::string("input.") + k + " 必须是字符串或 null"; return false;
            }
        }
    }

    // --- calibration ---
    {
        const auto& v = root["calibration"];
        if (!v.is_object()) { error_msg = "calibration 必须是对象"; return false; }
        static const std::vector<std::string> keys = {"mode","light_exposure_s","dark_exposure_s","fallback"};
        std::string u = find_unknown_keys(v, keys);
        if (!u.empty()) { error_msg = "calibration 未知字段: " + u; return false; }
        static const std::vector<std::string> req = {"mode","light_exposure_s","dark_exposure_s"};
        std::string m = find_missing_required(v, req);
        if (!m.empty()) { error_msg = "calibration 缺少字段: " + m; return false; }
        if (!v["mode"].is_string() ||
            (v["mode"] != "standard" && v["mode"] != "optimal" && v["mode"] != "exposure_ratio")) {
            error_msg = "calibration.mode 必须为 standard/optimal/exposure_ratio"; return false;
        }
        if (!v["light_exposure_s"].is_number() || v["light_exposure_s"].get<double>() <= 0) {
            error_msg = "calibration.light_exposure_s 必须是大于 0 的数"; return false;
        }
        if (!v["dark_exposure_s"].is_number() && !v["dark_exposure_s"].is_null()) {
            error_msg = "calibration.dark_exposure_s 必须是数字或 null"; return false;
        }
        if (v.contains("fallback") && !v["fallback"].is_null()) {
            if (!v["fallback"].is_string() ||
                (v["fallback"] != "exposure_ratio" && v["fallback"] != "fail")) {
                error_msg = "calibration.fallback 必须为 exposure_ratio/fail"; return false;
            }
        }
    }

    // --- platesolve ---
    {
        const auto& v = root["platesolve"];
        if (!v.is_object()) { error_msg = "platesolve 必须是对象"; return false; }
        static const std::vector<std::string> keys = {"gaia_catalog","max_stars","initial_ra_deg","initial_dec_deg"};
        std::string u = find_unknown_keys(v, keys);
        if (!u.empty()) { error_msg = "platesolve 未知字段: " + u; return false; }
        static const std::vector<std::string> req = {"gaia_catalog","max_stars"};
        std::string m = find_missing_required(v, req);
        if (!m.empty()) { error_msg = "platesolve 缺少字段: " + m; return false; }
        if (!v["gaia_catalog"].is_string() || v["gaia_catalog"].get<std::string>().empty()) {
            error_msg = "platesolve.gaia_catalog 必须是非空字符串"; return false;
        }
        if (!v["max_stars"].is_number_integer() || v["max_stars"].get<int>() < 1) {
            error_msg = "platesolve.max_stars 必须是 >= 1 的整数"; return false;
        }
        if (v.contains("initial_ra_deg") && !v["initial_ra_deg"].is_null()) {
            if (!v["initial_ra_deg"].is_number()) {
                error_msg = "platesolve.initial_ra_deg 必须是数字或 null"; return false;
            }
        }
        if (v.contains("initial_dec_deg") && !v["initial_dec_deg"].is_null()) {
            if (!v["initial_dec_deg"].is_number()) {
                error_msg = "platesolve.initial_dec_deg 必须是数字或 null"; return false;
            }
            double d = v["initial_dec_deg"].get<double>();
            if (d < -90 || d > 90) {
                error_msg = "platesolve.initial_dec_deg 必须在 [-90, 90] 范围内"; return false;
            }
        }
    }

    // --- psf ---
    {
        const auto& v = root["psf"];
        if (!v.is_object()) { error_msg = "psf 必须是对象"; return false; }
        static const std::vector<std::string> keys = {"fit_radius","max_iterations","tolerance"};
        std::string u = find_unknown_keys(v, keys);
        if (!u.empty()) { error_msg = "psf 未知字段: " + u; return false; }
        std::string m = find_missing_required(v, keys);
        if (!m.empty()) { error_msg = "psf 缺少字段: " + m; return false; }
        if (!v["fit_radius"].is_number_integer() || v["fit_radius"].get<int>() < 0) {
            error_msg = "psf.fit_radius 必须是 >= 0 的整数"; return false;
        }
        if (!v["max_iterations"].is_number_integer() || v["max_iterations"].get<int>() < 1) {
            error_msg = "psf.max_iterations 必须是 >= 1 的整数"; return false;
        }
        if (!v["tolerance"].is_number() || v["tolerance"].get<double>() <= 0) {
            error_msg = "psf.tolerance 必须是大于 0 的数"; return false;
        }
    }

    // --- photometric ---
    {
        const auto& v = root["photometric"];
        if (!v.is_object()) { error_msg = "photometric 必须是对象"; return false; }
        static const std::vector<std::string> keys = {"gaia_spectra","filter_response","qe_curve"};
        std::string u = find_unknown_keys(v, keys);
        if (!u.empty()) { error_msg = "photometric 未知字段: " + u; return false; }
        std::string m = find_missing_required(v, keys);
        if (!m.empty()) { error_msg = "photometric 缺少字段: " + m; return false; }
        for (const auto& k : keys) {
            if (!v[k].is_string() || v[k].get<std::string>().empty()) {
                error_msg = "photometric." + k + " 必须是非空字符串"; return false;
            }
        }
    }

    // --- snr ---
    {
        const auto& v = root["snr"];
        if (!v.is_object()) { error_msg = "snr 必须是对象"; return false; }
        static const std::vector<std::string> keys = {"estimator_id","sampling_scale"};
        std::string u = find_unknown_keys(v, keys);
        if (!u.empty()) { error_msg = "snr 未知字段: " + u; return false; }
        std::string m = find_missing_required(v, keys);
        if (!m.empty()) { error_msg = "snr 缺少字段: " + m; return false; }
        if (!v["estimator_id"].is_number_integer() || v["estimator_id"].get<int>() < 0) {
            error_msg = "snr.estimator_id 必须是 >= 0 的整数"; return false;
        }
        if (!v["sampling_scale"].is_number() || v["sampling_scale"].get<double>() <= 0) {
            error_msg = "snr.sampling_scale 必须是大于 0 的数"; return false;
        }
    }

    // --- drizzle ---
    {
        const auto& v = root["drizzle"];
        if (!v.is_object()) { error_msg = "drizzle 必须是对象"; return false; }
        static const std::vector<std::string> keys = {"mode","pixfrac","nside","ordering"};
        std::string u = find_unknown_keys(v, keys);
        if (!u.empty()) { error_msg = "drizzle 未知字段: " + u; return false; }
        std::string m = find_missing_required(v, keys);
        if (!m.empty()) { error_msg = "drizzle 缺少字段: " + m; return false; }
        if (v["mode"] != "precise") {
            error_msg = "drizzle.mode 必须为 \"precise\""; return false;
        }
        if (!v["pixfrac"].is_number() || v["pixfrac"].get<double>() <= 0 || v["pixfrac"].get<double>() > 1) {
            error_msg = "drizzle.pixfrac 必须在 (0, 1] 范围内"; return false;
        }
        if (v["ordering"] != "nested") {
            error_msg = "drizzle.ordering 必须为 \"nested\""; return false;
        }
        // nside: oneOf [auto | explicit+value]
        const auto& nside = v["nside"];
        if (!nside.is_object()) { error_msg = "drizzle.nside 必须是对象"; return false; }
        static const std::vector<std::string> nside_keys_auto = {"mode"};
        static const std::vector<std::string> nside_keys_explicit = {"mode","value"};
        if (!nside.contains("mode")) {
            error_msg = "drizzle.nside.mode 缺失"; return false;
        }
        if (nside["mode"] == "auto") {
            std::string nu = find_unknown_keys(nside, nside_keys_auto);
            if (!nu.empty()) { error_msg = "drizzle.nside 未知字段: " + nu; return false; }
        } else if (nside["mode"] == "explicit") {
            std::string nu = find_unknown_keys(nside, nside_keys_explicit);
            if (!nu.empty()) { error_msg = "drizzle.nside 未知字段: " + nu; return false; }
            if (!nside.contains("value")) {
                error_msg = "drizzle.nside.value 缺失 (explicit 模式必填)"; return false;
            }
            if (!nside["value"].is_number_integer() ||
                nside["value"].get<int>() < 16 || nside["value"].get<int>() > 4194304) {
                error_msg = "drizzle.nside.value 必须在 [16, 4194304] 范围内"; return false;
            }
        } else {
            error_msg = "drizzle.nside.mode 必须为 \"auto\" 或 \"explicit\""; return false;
        }
    }

    // --- output ---
    {
        const auto& v = root["output"];
        if (!v.is_object()) { error_msg = "output 必须是对象"; return false; }
        static const std::vector<std::string> keys = {"hiss","log","diagnostics_dir","overwrite"};
        std::string u = find_unknown_keys(v, keys);
        if (!u.empty()) { error_msg = "output 未知字段: " + u; return false; }
        std::string m = find_missing_required(v, keys);
        if (!m.empty()) { error_msg = "output 缺少字段: " + m; return false; }
        for (const auto& k : {"hiss","log","diagnostics_dir"}) {
            if (!v[k].is_string() || v[k].get<std::string>().empty()) {
                error_msg = std::string("output.") + k + " 必须是非空字符串"; return false;
            }
        }
        if (!v["overwrite"].is_boolean()) {
            error_msg = "output.overwrite 必须是布尔值"; return false;
        }
    }

    // --- execution ---
    {
        const auto& v = root["execution"];
        if (!v.is_object()) { error_msg = "execution 必须是对象"; return false; }
        static const std::vector<std::string> keys = {"stop_after","threads","stage_timeout_sec"};
        std::string u = find_unknown_keys(v, keys);
        if (!u.empty()) { error_msg = "execution 未知字段: " + u; return false; }
        std::string m = find_missing_required(v, keys);
        if (!m.empty()) { error_msg = "execution 缺少字段: " + m; return false; }
        static const std::vector<std::string> stop_options = {
            "read","calibrate","platesolve","psf","photometric","snr",
            "drizzle","hiss_verify","browser_verify"
        };
        if (!v["stop_after"].is_string() ||
            std::find(stop_options.begin(), stop_options.end(), v["stop_after"].get<std::string>()) == stop_options.end()) {
            error_msg = "execution.stop_after 值非法"; return false;
        }
        if (!v["threads"].is_number_integer() || v["threads"].get<int>() < 0) {
            error_msg = "execution.threads 必须是 >= 0 的整数"; return false;
        }
        const auto& timeouts = v["stage_timeout_sec"];
        if (!timeouts.is_object()) {
            error_msg = "execution.stage_timeout_sec 必须是对象"; return false;
        }
        for (auto it = timeouts.begin(); it != timeouts.end(); ++it) {
            if (!it.value().is_number() || it.value().get<double>() <= 0) {
                error_msg = "execution.stage_timeout_sec." + it.key() + " 必须是大于 0 的数"; return false;
            }
        }
    }

    return true;
}

// ============================================================================
// validate_stage1_schema - 公开接口: 验证 JSON 文件
// ============================================================================
bool validate_stage1_schema(const std::string& json_path, std::string& error_msg) {
    std::string content;
    if (!read_file_all(json_path, content, error_msg)) {
        return false;
    }

    json root;
    try {
        root = json::parse(content);
    } catch (const json::parse_error& e) {
        error_msg = std::string("JSON 解析错误: ") + e.what();
        return false;
    } catch (const std::exception& e) {
        error_msg = std::string("JSON 解析异常: ") + e.what();
        return false;
    }

    return validate_schema_internal(root, error_msg);
}

// ============================================================================
// parse_stage1_config - 解析 + 验证 + 填充 Stage1Config + 解析路径
// ============================================================================
int parse_stage1_config(const std::string& json_path, Stage1Config& config, std::string& error_msg) {
    // 1. 检查文件存在
    if (json_path.empty()) {
        error_msg = "JSON 配置路径为空";
        return -1;
    }
    fs::path abs_json_path = fs::weakly_canonical(fs::path(json_path));
    if (!fs::exists(abs_json_path)) {
        error_msg = "JSON 配置文件不存在: " + abs_json_path.string();
        return -1;
    }

    // 2. 读取文件
    std::string content;
    if (!read_file_all(abs_json_path.string(), content, error_msg)) {
        return -1;
    }

    // 3. 计算原始 JSON SHA256
    config.original_json_sha256 = sha256_impl::sha256(content);
    config.original_json_path = abs_json_path.string();

    // 4. 解析 JSON
    json root;
    try {
        root = json::parse(content);
    } catch (const json::parse_error& e) {
        error_msg = std::string("JSON 解析错误: ") + e.what();
        return -1;
    } catch (const std::exception& e) {
        error_msg = std::string("JSON 解析异常: ") + e.what();
        return -1;
    }

    // 5. Schema 验证
    if (!validate_schema_internal(root, error_msg)) {
        return -1;
    }

    // 6. 填充 Stage1Config (路径基于 JSON 文件所在目录解析)
    fs::path base_dir = abs_json_path.parent_path();

    config.schema_version = root["schema_version"].get<std::string>();
    config.pipeline = root["pipeline"].get<std::string>();
    config.precision = (root["precision"] == "fp64") ? PrecisionMode::FP64 : PrecisionMode::FP32;

    // input
    config.input.light = resolve_path(root["input"]["light"].get<std::string>(), base_dir);
    config.input.master_bias = root["input"]["master_bias"].is_null() ? "" :
        resolve_path(root["input"]["master_bias"].get<std::string>(), base_dir);
    config.input.master_dark = root["input"]["master_dark"].is_null() ? "" :
        resolve_path(root["input"]["master_dark"].get<std::string>(), base_dir);
    config.input.master_flat = root["input"]["master_flat"].is_null() ? "" :
        resolve_path(root["input"]["master_flat"].get<std::string>(), base_dir);

    // calibration
    config.calibration.mode = root["calibration"]["mode"].get<std::string>();
    config.calibration.light_exposure_s = root["calibration"]["light_exposure_s"].get<double>();
    config.calibration.dark_exposure_s = root["calibration"]["dark_exposure_s"].is_null() ? 0.0 :
        root["calibration"]["dark_exposure_s"].get<double>();
    if (root["calibration"].contains("fallback") && !root["calibration"]["fallback"].is_null()) {
        config.calibration.fallback = root["calibration"]["fallback"].get<std::string>();
    }

    // platesolve
    config.platesolve.gaia_catalog = resolve_path(root["platesolve"]["gaia_catalog"].get<std::string>(), base_dir);
    config.platesolve.max_stars = root["platesolve"]["max_stars"].get<int>();
    if (root["platesolve"].contains("initial_ra_deg") && !root["platesolve"]["initial_ra_deg"].is_null()) {
        config.platesolve.initial_ra_deg = root["platesolve"]["initial_ra_deg"].get<double>();
    }
    if (root["platesolve"].contains("initial_dec_deg") && !root["platesolve"]["initial_dec_deg"].is_null()) {
        config.platesolve.initial_dec_deg = root["platesolve"]["initial_dec_deg"].get<double>();
    }

    // psf
    config.psf.fit_radius = root["psf"]["fit_radius"].get<int>();
    config.psf.max_iterations = root["psf"]["max_iterations"].get<int>();
    config.psf.tolerance = root["psf"]["tolerance"].get<double>();

    // photometric
    config.photometric.gaia_spectra = resolve_path(root["photometric"]["gaia_spectra"].get<std::string>(), base_dir);
    config.photometric.filter_response = resolve_path(root["photometric"]["filter_response"].get<std::string>(), base_dir);
    config.photometric.qe_curve = resolve_path(root["photometric"]["qe_curve"].get<std::string>(), base_dir);

    // snr
    config.snr.estimator_id = root["snr"]["estimator_id"].get<int>();
    config.snr.sampling_scale = root["snr"]["sampling_scale"].get<double>();

    // drizzle
    config.drizzle.mode = root["drizzle"]["mode"].get<std::string>();
    config.drizzle.pixfrac = root["drizzle"]["pixfrac"].get<double>();
    config.drizzle.ordering = root["drizzle"]["ordering"].get<std::string>();
    config.drizzle.nside_mode = root["drizzle"]["nside"]["mode"].get<std::string>();
    if (config.drizzle.nside_mode == "explicit") {
        config.drizzle.nside_value = root["drizzle"]["nside"]["value"].get<int>();
    }

    // output
    config.output.hiss = resolve_path(root["output"]["hiss"].get<std::string>(), base_dir);
    config.output.log = resolve_path(root["output"]["log"].get<std::string>(), base_dir);
    config.output.diagnostics_dir = resolve_path(root["output"]["diagnostics_dir"].get<std::string>(), base_dir);
    config.output.overwrite = root["output"]["overwrite"].get<bool>();

    // execution
    config.execution.stop_after = root["execution"]["stop_after"].get<std::string>();
    config.execution.threads = root["execution"]["threads"].get<int>();
    const auto& timeouts = root["execution"]["stage_timeout_sec"];
    for (auto it = timeouts.begin(); it != timeouts.end(); ++it) {
        config.execution.stage_timeout_sec[it.key()] = it.value().get<double>();
    }

    // 7. 计算规范化配置 SHA256
    config.config_sha256 = compute_config_sha256(config);

    LOG_INFO("json_config", "配置解析成功: " + abs_json_path.string());
    LOG_INFO("json_config", "原始 JSON SHA256: " + config.original_json_sha256);
    LOG_INFO("json_config", "规范化配置 SHA256: " + config.config_sha256);

    return 0;
}

// ============================================================================
// compute_config_sha256 - 将 Stage1Config 序列化为规范 JSON, 计算 SHA256
// ============================================================================
std::string compute_config_sha256(const Stage1Config& config) {
    json j;
    j["schema_version"] = config.schema_version;
    j["pipeline"] = config.pipeline;
    j["precision"] = (config.precision == PrecisionMode::FP64) ? "fp64" : "fp32";

    j["input"] = {
        {"light", config.input.light},
        {"master_bias", config.input.master_bias},
        {"master_dark", config.input.master_dark},
        {"master_flat", config.input.master_flat}
    };

    j["calibration"] = {
        {"mode", config.calibration.mode},
        {"light_exposure_s", config.calibration.light_exposure_s},
        {"dark_exposure_s", config.calibration.dark_exposure_s},
        {"fallback", config.calibration.fallback}
    };

    j["platesolve"] = {
        {"gaia_catalog", config.platesolve.gaia_catalog},
        {"max_stars", config.platesolve.max_stars}
    };
    if (config.platesolve.initial_ra_deg != -999.0) {
        j["platesolve"]["initial_ra_deg"] = config.platesolve.initial_ra_deg;
    } else {
        j["platesolve"]["initial_ra_deg"] = nullptr;
    }
    if (config.platesolve.initial_dec_deg != -999.0) {
        j["platesolve"]["initial_dec_deg"] = config.platesolve.initial_dec_deg;
    } else {
        j["platesolve"]["initial_dec_deg"] = nullptr;
    }

    j["psf"] = {
        {"fit_radius", config.psf.fit_radius},
        {"max_iterations", config.psf.max_iterations},
        {"tolerance", config.psf.tolerance}
    };

    j["photometric"] = {
        {"gaia_spectra", config.photometric.gaia_spectra},
        {"filter_response", config.photometric.filter_response},
        {"qe_curve", config.photometric.qe_curve}
    };

    j["snr"] = {
        {"estimator_id", config.snr.estimator_id},
        {"sampling_scale", config.snr.sampling_scale}
    };

    j["drizzle"] = {
        {"mode", config.drizzle.mode},
        {"pixfrac", config.drizzle.pixfrac},
        {"ordering", config.drizzle.ordering}
    };
    j["drizzle"]["nside"]["mode"] = config.drizzle.nside_mode;
    if (config.drizzle.nside_mode == "explicit") {
        j["drizzle"]["nside"]["value"] = config.drizzle.nside_value;
    }

    j["output"] = {
        {"hiss", config.output.hiss},
        {"log", config.output.log},
        {"diagnostics_dir", config.output.diagnostics_dir},
        {"overwrite", config.output.overwrite}
    };

    j["execution"] = {
        {"stop_after", config.execution.stop_after},
        {"threads", config.execution.threads}
    };
    j["execution"]["stage_timeout_sec"] = json::object();
    for (const auto& [k, v] : config.execution.stage_timeout_sec) {
        j["execution"]["stage_timeout_sec"][k] = v;
    }

    // 规范序列化 (排序键, 紧凑格式)
    std::string canonical = j.dump(-1, ' ', false, nlohmann::detail::error_handler_t::replace);
    return sha256_impl::sha256(canonical);
}

// ============================================================================
// build_compat_config_json - 桥接: Stage1Config -> flat config_json
// 供 run_stage1 内部 orc_* 文本查找的 stage handler 使用
// 映射新嵌套 schema 到旧 flat key
// ============================================================================
std::string build_compat_config_json(const Stage1Config& config) {
    json j;

    // precision
    j["precision"] = (config.precision == PrecisionMode::FP64) ? "fp64" : "fp32";

    // gaia 数据目录 (init_platesolve_env 用 GaiaDR3SP 路径)
    j["gaia_data_dir"] = config.photometric.gaia_spectra;
    // gaia 星表目录 (platesolve 参考星来源, 新字段)
    j["gaia_catalog"] = config.platesolve.gaia_catalog;

    // 校准文件路径 (新 schema -> 旧 flat key)
    j["master_bias_path"] = config.input.master_bias;
    j["master_dark_path"] = config.input.master_dark;
    j["master_flat_path"] = config.input.master_flat;

    // calibration_dir (新 schema 无此字段, 留空)
    j["calibration_dir"] = "";

    // platesolve 参数
    j["max_stars"] = config.platesolve.max_stars;
    // initial_ra/dec: 旧代码读字符串, -999 表示 null -> 空字符串
    if (config.platesolve.initial_ra_deg != -999.0) {
        std::ostringstream oss;
        oss << std::setprecision(10) << config.platesolve.initial_ra_deg;
        j["initial_ra"] = oss.str();
    } else {
        j["initial_ra"] = "";
    }
    if (config.platesolve.initial_dec_deg != -999.0) {
        std::ostringstream oss;
        oss << std::setprecision(10) << config.platesolve.initial_dec_deg;
        j["initial_dec"] = oss.str();
    } else {
        j["initial_dec"] = "";
    }
    j["focal_length"] = 0.0;
    j["pixel_size"] = 0.0;

    // psf 参数 (新 schema -> 旧 key 名)
    j["fit_radius"] = config.psf.fit_radius;
    j["max_iter"] = config.psf.max_iterations;
    j["tolerance"] = config.psf.tolerance;

    // photometric 参数
    j["filters_json"] = config.photometric.filter_response;
    j["qe_curves_json"] = config.photometric.qe_curve;
    j["mag_min"] = 6.0;
    j["mag_max"] = 16.0;
    j["fov_radius_deg"] = 0.0;

    // drizzle 参数 (新嵌套 nside -> 旧 flat nside_strategy/nside_override)
    if (config.drizzle.nside_mode == "explicit") {
        j["nside_strategy"] = "fixed";
        j["nside_override"] = config.drizzle.nside_value;
    } else {
        j["nside_strategy"] = "1x_to_2x_drizzle";
        j["nside_override"] = 0;
    }
    j["pixfrac"] = config.drizzle.pixfrac;
    j["nested"] = (config.drizzle.ordering == "nested");

    // snr 参数
    j["estimator_id"] = config.snr.estimator_id;
    j["sampling_scale"] = config.snr.sampling_scale;

    // execution 参数
    j["threads"] = config.execution.threads;
    j["log_level"] = "INFO";
    j["allow_partial_output"] = false;

    // stage_timeouts: 新 schema 用小写键 (read/calibrate/...),
    // 旧代码用大写 stage 名查找 (READ_FITS/CALIBRATE/...),
    // 此处映射键名以兼容
    static const std::map<std::string, std::string> timeout_key_map = {
        {"read", "READ_FITS"},
        {"calibrate", "CALIBRATE"},
        {"platesolve", "PLATESOLVE"},
        {"psf", "PSF"},
        {"photometric", "PHOTOMETRIC"},
        {"snr", "SNR"},
        {"drizzle", "DRIZZLE"},
        {"hiss_verify", "HISS_VERIFY"},
        {"browser_verify", "BROWSER_VERIFY"}
    };
    json timeouts_obj = json::object();
    for (const auto& [k, v] : config.execution.stage_timeout_sec) {
        auto it = timeout_key_map.find(k);
        std::string mapped_key = (it != timeout_key_map.end()) ? it->second : k;
        timeouts_obj[mapped_key] = v;
    }
    j["stage_timeouts"] = timeouts_obj;

    // 校准曝光时间 (新字段, 供 run_stage_calibrate 使用)
    j["light_exposure_s"] = config.calibration.light_exposure_s;
    j["dark_exposure_s"] = config.calibration.dark_exposure_s;

    return j.dump();
}
