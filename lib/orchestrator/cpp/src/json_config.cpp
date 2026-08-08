// ============================================================================
// json_config.cpp - Stage1 JSON 配置解析与 Schema 验证实现
// 使用 nlohmann-json-schema-validator v2.4.0 (vendor: third_party/) 进行正式
// Draft 2020-12 Schema 验证, 单一权威, 无手写重复规则。
// typed Stage1Config 直接驱动各 stage, 无 compat flat JSON 桥。
// ============================================================================

#include "json_config.h"
#include "logger.h"

#include <nlohmann/json.hpp>
#include <nlohmann/json-schema.hpp>
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
// 内嵌 Schema JSON (v1.1, 与 lib/orchestrator/configs/stage1.schema.json 一致)
// 由控制包 schemas/stage1.schema.json 生成 (gen_json_config.py)
// ============================================================================
static const char* STAGE1_SCHEMA_JSON = R"JSON(
{
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "$id": "https://astrocs.local/schema/stage1-1.1.json",
  "title": "AstroCS Stage1 Job",
  "type": "object",
  "additionalProperties": false,
  "required": [
    "schema_version",
    "pipeline",
    "precision",
    "gaia_data_dir",
    "input",
    "calibration",
    "platesolve",
    "psf",
    "photometric",
    "snr",
    "drizzle",
    "output",
    "execution"
  ],
  "properties": {
    "$schema": {
      "type": "string"
    },
    "schema_version": {
      "const": "1.1"
    },
    "pipeline": {
      "const": "stage1"
    },
    "precision": {
      "enum": [
        "fp32",
        "fp64"
      ]
    },
    "gaia_data_dir": {
      "type": "string",
      "minLength": 1,
      "description": "Gaia DR3SP 光谱数据库目录 (必须由配置文件引入, 不硬编码默认路径)"
    },
    "input": {
      "type": "object",
      "additionalProperties": false,
      "required": [
        "light",
        "master_bias",
        "master_dark",
        "master_flat"
      ],
      "properties": {
        "light": {
          "type": "string",
          "minLength": 1
        },
        "master_bias": {
          "type": [
            "string",
            "null"
          ]
        },
        "master_dark": {
          "type": [
            "string",
            "null"
          ]
        },
        "master_flat": {
          "type": [
            "string",
            "null"
          ]
        }
      }
    },
    "calibration": {
      "type": "object",
      "additionalProperties": false,
      "required": [
        "mode",
        "light_exposure_s",
        "dark_exposure_s",
        "fallback"
      ],
      "properties": {
        "mode": {
          "enum": [
            "standard",
            "optimal",
            "exposure_ratio"
          ]
        },
        "light_exposure_s": {
          "type": "number",
          "exclusiveMinimum": 0
        },
        "dark_exposure_s": {
          "type": "number",
          "exclusiveMinimum": 0
        },
        "fallback": {
          "enum": [
            "exposure_ratio",
            "fail"
          ]
        }
      }
    },
    "platesolve": {
      "type": "object",
      "additionalProperties": false,
      "required": [
        "gaia_catalog",
        "max_stars",
        "initial_ra_deg",
        "initial_dec_deg"
      ],
      "properties": {
        "gaia_catalog": {
          "type": "string",
          "minLength": 1
        },
        "max_stars": {
          "type": "integer",
          "minimum": 1
        },
        "initial_ra_deg": {
          "type": [
            "number",
            "null"
          ],
          "minimum": 0,
          "exclusiveMaximum": 360
        },
        "initial_dec_deg": {
          "type": [
            "number",
            "null"
          ],
          "minimum": -90,
          "maximum": 90
        }
      }
    },
    "psf": {
      "type": "object",
      "additionalProperties": false,
      "required": [
        "fit_radius",
        "max_iterations",
        "tolerance"
      ],
      "properties": {
        "fit_radius": {
          "type": "integer",
          "minimum": 1
        },
        "max_iterations": {
          "type": "integer",
          "minimum": 1
        },
        "tolerance": {
          "type": "number",
          "exclusiveMinimum": 0
        }
      }
    },
    "photometric": {
      "type": "object",
      "additionalProperties": false,
      "required": [
        "gaia_spectra",
        "filter_response",
        "qe_curve"
      ],
      "properties": {
        "gaia_spectra": {
          "type": "string",
          "minLength": 1
        },
        "filter_response": {
          "type": "string",
          "minLength": 1
        },
        "qe_curve": {
          "type": "string",
          "minLength": 1
        }
      }
    },
    "snr": {
      "type": "object",
      "additionalProperties": false,
      "required": [
        "estimator_id",
        "sampling_scale"
      ],
      "properties": {
        "estimator_id": {
          "type": "integer",
          "minimum": 0
        },
        "sampling_scale": {
          "type": "number",
          "exclusiveMinimum": 0
        }
      }
    },
    "drizzle": {
      "type": "object",
      "additionalProperties": false,
      "required": [
        "mode",
        "pixfrac",
        "nside",
        "ordering"
      ],
      "properties": {
        "mode": {
          "const": "precise"
        },
        "pixfrac": {
          "type": "number",
          "exclusiveMinimum": 0,
          "maximum": 1
        },
        "ordering": {
          "const": "nested"
        },
        "nside": {
          "oneOf": [
            {
              "type": "object",
              "additionalProperties": false,
              "required": [
                "mode"
              ],
              "properties": {
                "mode": {
                  "const": "auto"
                }
              }
            },
            {
              "type": "object",
              "additionalProperties": false,
              "required": [
                "mode",
                "value"
              ],
              "properties": {
                "mode": {
                  "const": "explicit"
                },
                "value": {
                  "type": "integer",
                  "minimum": 16,
                  "maximum": 4194304
                }
              }
            }
          ]
        }
      }
    },
    "output": {
      "type": "object",
      "additionalProperties": false,
      "required": [
        "log",
        "diagnostics_dir",
        "overwrite"
      ],
      "properties": {
        "hiss": {
          "type": "string",
          "default": ""
        },
        "hips": {
          "type": "string",
          "default": ""
        },
        "log": {
          "type": "string",
          "minLength": 1
        },
        "diagnostics_dir": {
          "type": "string",
          "minLength": 1
        },
        "overwrite": {
          "type": "boolean"
        }
      }
    },
    "validation": {
      "type": "object",
      "additionalProperties": false,
      "properties": {
        "legacy_hiss_compare": {
          "type": "boolean",
          "default": false
        }
      }
    },
    "execution": {
      "type": "object",
      "additionalProperties": false,
      "required": [
        "stop_after",
        "threads",
        "stage_timeout_sec"
      ],
      "properties": {
        "stop_after": {
          "enum": [
            "read",
            "calibrate",
            "platesolve",
            "psf",
            "photometric",
            "snr",
            "nside",
            "drizzle",
            "hips_verify",
            "hiss_verify",
            "browser_verify"
          ]
        },
        "threads": {
          "type": "integer",
          "minimum": 0
        },
        "stage_timeout_sec": {
          "type": "object",
          "additionalProperties": false,
          "required": [
            "read",
            "calibrate",
            "platesolve",
            "psf",
            "photometric",
            "snr",
            "nside",
            "drizzle",
            "hips_verify",
            "hiss_verify",
            "browser_verify"
          ],
          "properties": {
            "read": {
              "type": "number",
              "exclusiveMinimum": 0
            },
            "calibrate": {
              "type": "number",
              "exclusiveMinimum": 0
            },
            "platesolve": {
              "type": "number",
              "exclusiveMinimum": 0
            },
            "psf": {
              "type": "number",
              "exclusiveMinimum": 0
            },
            "photometric": {
              "type": "number",
              "exclusiveMinimum": 0
            },
            "snr": {
              "type": "number",
              "exclusiveMinimum": 0
            },
            "nside": {
              "type": "number",
              "exclusiveMinimum": 0
            },
            "drizzle": {
              "type": "number",
              "exclusiveMinimum": 0
            },
            "hips_verify": {
              "type": "number",
              "exclusiveMinimum": 0
            },
            "hiss_verify": {
              "type": "number",
              "exclusiveMinimum": 0
            },
            "browser_verify": {
              "type": "number",
              "exclusiveMinimum": 0
            }
          }
        }
      }
    }
  },
  "allOf": [
    {
      "if": {
        "properties": {
          "calibration": {
            "properties": {
              "mode": {
                "const": "standard"
              }
            }
          }
        }
      },
      "then": {
        "properties": {
          "input": {
            "properties": {
              "master_dark": {
                "type": "string",
                "minLength": 1
              },
              "master_flat": {
                "type": "string",
                "minLength": 1
              }
            }
          }
        }
      }
    },
    {
      "if": {
        "properties": {
          "calibration": {
            "properties": {
              "mode": {
                "enum": [
                  "optimal",
                  "exposure_ratio"
                ]
              }
            }
          }
        }
      },
      "then": {
        "properties": {
          "input": {
            "properties": {
              "master_bias": {
                "type": "string",
                "minLength": 1
              },
              "master_dark": {
                "type": "string",
                "minLength": 1
              },
              "master_flat": {
                "type": "string",
                "minLength": 1
              }
            }
          }
        }
      }
    }
  ]
}
)JSON";

std::string get_stage1_schema_json() {
    return STAGE1_SCHEMA_JSON;
}

std::string get_stage1_schema_sha256() {
    return sha256_impl::sha256(STAGE1_SCHEMA_JSON);
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
// 辅助: 路径解析 (相对路径基于 base_dir 解析为绝对路径)
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
// 正式 Schema 验证 (nlohmann-json-schema-validator, 唯一权威)
// ============================================================================
static bool validate_with_json_schema_validator(const json& root, std::string& error_msg) {
    try {
        nlohmann::json_schema::json_validator validator;
        validator.set_root_schema(json::parse(STAGE1_SCHEMA_JSON));
        validator.validate(root);
        return true;
    } catch (const std::exception& e) {
        error_msg = std::string("Schema 验证失败: ") + e.what();
        return false;
    }
}

// ============================================================================
// 运行时能力检查 (Schema 之外的条件约束, JSON_CONFIG_REPAIR.md)
// ============================================================================
static bool runtime_checks(const json& root, const fs::path& base_dir,
                           bool check_paths, std::string& error_msg) {
    // 1. explicit nside 必须是 2 的幂
    if (root["drizzle"]["nside"]["mode"] == "explicit") {
        long long v = root["drizzle"]["nside"]["value"].get<long long>();
        if (v < 1 || (v & (v - 1)) != 0) {
            error_msg = "drizzle.nside.value 必须是 2 的幂: " + std::to_string(v);
            return false;
        }
    }
    if (check_paths) {
        // 2. 必填输入路径必须存在 (仅正式执行; --validate 模板不检查占位路径)
        const auto& inp = root["input"];
        for (const char* key : {"light", "master_bias", "master_dark", "master_flat"}) {
            if (inp[key].is_null()) continue;
            std::string p = resolve_path(inp[key].get<std::string>(), base_dir);
            if (!fs::exists(p)) {
                error_msg = std::string("输入文件不存在: ") + key + " -> " + p;
                return false;
            }
        }
        // 3. 输出目录父目录必须存在或可创建 (hiss 已可选, 空串跳过)
        const auto& out = root["output"];
        for (const char* key : {"hiss", "log", "diagnostics_dir"}) {
            if (out[key].is_null() || out[key].get<std::string>().empty()) continue;
            std::string p = resolve_path(out[key].get<std::string>(), base_dir);
            fs::path parent = fs::path(p).parent_path();
            if (!parent.empty()) {
                std::error_code ec;
                fs::create_directories(parent, ec);
                if (ec && !fs::exists(parent)) {
                    error_msg = std::string("无法创建输出父目录: ") + key + " -> " + parent.string() + " (" + ec.message() + ")";
                    return false;
                }
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
    fs::path base_dir = fs::weakly_canonical(fs::path(json_path)).parent_path();
    if (!validate_with_json_schema_validator(root, error_msg)) {
        return false;
    }
    return runtime_checks(root, base_dir, false, error_msg);
}

// ============================================================================
// parse_stage1_config - 解析 + 正式 Schema 验证 + 填充 Stage1Config + 路径解析
// ============================================================================
int parse_stage1_config(const std::string& json_path, Stage1Config& config, std::string& error_msg) {
    if (json_path.empty()) {
        error_msg = "JSON 配置路径为空";
        return -1;
    }
    fs::path abs_json_path = fs::weakly_canonical(fs::path(json_path));
    if (!fs::exists(abs_json_path)) {
        error_msg = "JSON 配置文件不存在: " + abs_json_path.string();
        return -1;
    }

    std::string content;
    if (!read_file_all(abs_json_path.string(), content, error_msg)) {
        return -1;
    }

    config.original_json_sha256 = sha256_impl::sha256(content);
    config.original_json_path = abs_json_path.string();

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

    if (!validate_with_json_schema_validator(root, error_msg)) {
        return -1;
    }
    fs::path base_dir = abs_json_path.parent_path();
    if (!runtime_checks(root, base_dir, true, error_msg)) {
        return -1;
    }

    // ---- 填充 Stage1Config (路径基于 JSON 文件所在目录解析) ----
    config.schema_version = root["schema_version"].get<std::string>();
    config.pipeline = root["pipeline"].get<std::string>();
    config.precision = (root["precision"] == "fp64") ? PrecisionMode::FP64 : PrecisionMode::FP32;
    config.gaia_data_dir = resolve_path(root["gaia_data_dir"].get<std::string>(), base_dir);

    config.input.light = resolve_path(root["input"]["light"].get<std::string>(), base_dir);
    config.input.master_bias = root["input"]["master_bias"].is_null() ? "" :
        resolve_path(root["input"]["master_bias"].get<std::string>(), base_dir);
    config.input.master_dark = root["input"]["master_dark"].is_null() ? "" :
        resolve_path(root["input"]["master_dark"].get<std::string>(), base_dir);
    config.input.master_flat = root["input"]["master_flat"].is_null() ? "" :
        resolve_path(root["input"]["master_flat"].get<std::string>(), base_dir);

    config.calibration.mode = root["calibration"]["mode"].get<std::string>();
    config.calibration.light_exposure_s = root["calibration"]["light_exposure_s"].get<double>();
    config.calibration.dark_exposure_s = root["calibration"]["dark_exposure_s"].get<double>();
    config.calibration.fallback = root["calibration"]["fallback"].get<std::string>();

    config.platesolve.gaia_catalog = resolve_path(root["platesolve"]["gaia_catalog"].get<std::string>(), base_dir);
    config.platesolve.max_stars = root["platesolve"]["max_stars"].get<int>();
    if (!root["platesolve"]["initial_ra_deg"].is_null()) {
        config.platesolve.initial_ra_deg = root["platesolve"]["initial_ra_deg"].get<double>();
    }
    if (!root["platesolve"]["initial_dec_deg"].is_null()) {
        config.platesolve.initial_dec_deg = root["platesolve"]["initial_dec_deg"].get<double>();
    }

    config.psf.fit_radius = root["psf"]["fit_radius"].get<int>();
    config.psf.max_iterations = root["psf"]["max_iterations"].get<int>();
    config.psf.tolerance = root["psf"]["tolerance"].get<double>();

    config.photometric.gaia_spectra = resolve_path(root["photometric"]["gaia_spectra"].get<std::string>(), base_dir);
    config.photometric.filter_response = resolve_path(root["photometric"]["filter_response"].get<std::string>(), base_dir);
    config.photometric.qe_curve = resolve_path(root["photometric"]["qe_curve"].get<std::string>(), base_dir);

    config.snr.estimator_id = root["snr"]["estimator_id"].get<int>();
    config.snr.sampling_scale = root["snr"]["sampling_scale"].get<double>();

    config.drizzle.mode = root["drizzle"]["mode"].get<std::string>();
    config.drizzle.pixfrac = root["drizzle"]["pixfrac"].get<double>();
    config.drizzle.ordering = root["drizzle"]["ordering"].get<std::string>();
    config.drizzle.nside_mode = root["drizzle"]["nside"]["mode"].get<std::string>();
    if (config.drizzle.nside_mode == "explicit") {
        config.drizzle.nside_value = root["drizzle"]["nside"]["value"].get<int>();
    }

    const auto& out_cfg = root["output"];
    config.output.hiss = out_cfg.contains("hiss") && !out_cfg["hiss"].is_null()
        ? resolve_path(out_cfg["hiss"].get<std::string>(), base_dir) : std::string();
    config.output.hips = out_cfg.contains("hips") && !out_cfg["hips"].is_null()
        ? resolve_path(out_cfg["hips"].get<std::string>(), base_dir) : std::string();
    config.output.log = resolve_path(root["output"]["log"].get<std::string>(), base_dir);
    config.output.diagnostics_dir = resolve_path(root["output"]["diagnostics_dir"].get<std::string>(), base_dir);
    config.output.overwrite = root["output"]["overwrite"].get<bool>();
    config.validation.legacy_hiss_compare =
        root.contains("validation") && root["validation"].contains("legacy_hiss_compare")
        ? root["validation"]["legacy_hiss_compare"].get<bool>() : false;

    config.execution.stop_after = root["execution"]["stop_after"].get<std::string>();
    config.execution.threads = root["execution"]["threads"].get<int>();
    const auto& timeouts = root["execution"]["stage_timeout_sec"];
    for (auto it = timeouts.begin(); it != timeouts.end(); ++it) {
        config.execution.stage_timeout_sec[it.key()] = it.value().get<double>();
    }

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
    j["gaia_data_dir"] = config.gaia_data_dir;

    j["input"] = {
        {"light", config.input.light},
        {"master_bias", config.input.master_bias.empty() ? json(nullptr) : json(config.input.master_bias)},
        {"master_dark", config.input.master_dark.empty() ? json(nullptr) : json(config.input.master_dark)},
        {"master_flat", config.input.master_flat.empty() ? json(nullptr) : json(config.input.master_flat)}
    };

    j["calibration"] = {
        {"mode", config.calibration.mode},
        {"light_exposure_s", config.calibration.light_exposure_s},
        {"dark_exposure_s", config.calibration.dark_exposure_s},
        {"fallback", config.calibration.fallback}
    };

    j["platesolve"] = {
        {"gaia_catalog", config.platesolve.gaia_catalog},
        {"max_stars", config.platesolve.max_stars},
        {"initial_ra_deg", config.platesolve.initial_ra_deg == -999.0 ? json(nullptr) : json(config.platesolve.initial_ra_deg)},
        {"initial_dec_deg", config.platesolve.initial_dec_deg == -999.0 ? json(nullptr) : json(config.platesolve.initial_dec_deg)}
    };

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
        {"hips", config.output.hips},
        {"log", config.output.log},
        {"diagnostics_dir", config.output.diagnostics_dir},
        {"overwrite", config.output.overwrite}
    };
    j["validation"] = {
        {"legacy_hiss_compare", config.validation.legacy_hiss_compare}
    };

    j["execution"] = {
        {"stop_after", config.execution.stop_after},
        {"threads", config.execution.threads}
    };
    j["execution"]["stage_timeout_sec"] = json::object();
    for (const auto& [k, v] : config.execution.stage_timeout_sec) {
        j["execution"]["stage_timeout_sec"][k] = v;
    }

    std::string canonical = j.dump(-1, ' ', false, nlohmann::detail::error_handler_t::replace);
    return sha256_impl::sha256(canonical);
}
