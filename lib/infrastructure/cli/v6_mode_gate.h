// lib/infrastructure/cli/v6_mode_gate.h — RUNTIME-CI-001: V6 显式 CLI 模式路由门
//
// 单一事实源 = lib/infrastructure/cli/v6_runtime_contract.h。语义（宪章 §3.2 / 冻结 FZ-MODE-*）：
//   * 显式 --mode <token>（phase2）/ --export-mode <token>（phase3）必须经冻结路由表判定：
//     production 放行；reject → ARGS(2)（fail-closed）。
//     **phase2 的 --mode 无任何合法 token**（FZ-WEIGHT-SINGLE-PATH）⇒ 一律拒绝；
//     baseline 面（原 equal / pixel_ivar 非生产放行）已按 §9.73 裁决 A44 删除。
//   * phase2 config 的 legacy 整数 weight_mode：**该键不存在**，任何形态出现即拒绝。
//   * 未显式给出模式时不介入（既有缺省路径不变）。
//   * 只对本 phase 生效，只读本 phase 配置；不把三个 Phase 串接（§3.2）。
#pragma once

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <nlohmann/json.hpp>

#include "exit_codes.h"
#include "jsonl.h"
#include "v6_runtime_contract.h"

namespace astrocs {
namespace v6cli {

inline int mode_gate(const Parsed& p, int phase, astrocs::JsonlEmitter& ev) {
    using namespace astrocs::v6runtime;
    if (phase != 2 && phase != 3) return astrocs::OK;
    const char* flag = (phase == 2) ? "--mode" : "--export-mode";

    // 读 config（解析失败不在此报错：常规 validate 路径给出权威诊断）
    nlohmann::json doc;
    bool have_doc = false;
    if (p.values.count("--config")) {
        std::ifstream f(std::filesystem::u8path(p.values.at("--config")), std::ios::binary);
        if (f) {
            std::stringstream buf; buf << f.rdbuf();
            try { doc = nlohmann::json::parse(buf.str()); have_doc = true; } catch (...) {}
        }
    }

    auto emit_route = [&](const ModeRoute& mr, const char* source, const char* label) {
        const bool reject = (mr.kind == RouteKind::kReject);
        const char* sev = reject ? "error"
                                 : (mr.kind == RouteKind::kBaseline ? "warning" : "info");
        nlohmann::json payload = {
            {"route_kind", route_kind_name(mr.kind)},
            {"token", mr.token},
            {"surface", mr.surface},
            {"source", source},
            {"reason", mr.reason},
            {"phase", phase_name(phase == 2 ? Phase::kP2 : Phase::kP3)},
            {"implicit_phase_chain", is_implicit_phase_chain(p.cmd)},
            {"budget_source_owner", ProcessBudgetRegistry::instance().owner()},
            {"budget_allocated_cores", ProcessBudgetRegistry::instance().allocated_cores()},
            {"one_budget_source_rule", kOneBudgetSourceRule},
        };
        ev.emit("v6_mode_route", sev, phase == 2 ? "phase2" : "phase3",
                std::string("v6 mode route: ") + route_kind_name(mr.kind) +
                    (mr.reason.empty() ? std::string() : (" - " + mr.reason)),
                payload);
        if (reject) {
            std::fprintf(stderr, "acsd: %s rejected: %s\n", label, mr.reason.c_str());
        } else if (mr.kind == RouteKind::kBaseline) {
            std::fprintf(stderr, "acsd: WARNING %s=%s is a baseline "
                                 "(non-production) weight surface\n",
                         label, mr.token.c_str());
        }
    };

    // 1) phase2 config 的 legacy 整数 weight_mode —— §9.73 裁决 A44 后**该键不存在**，
    //    任何形态（整数 / 字符串 / 其它）出现即 fail-closed 具名拒绝（rc=ARGS）。
    //    原实现只对整数形态路由，且 1|2 → baseline 放行；两处一并删除。
    //    依据：ASTROCS_DESIGN.md §3.1:175「没有可选择项」；PSF_SIGNAL_WEIGHT.md §4:72
    //    「不存在口径选择键、口径枚举、口径配置项或口径产物」。
    if (phase == 2 && have_doc && doc.contains("weight_mode")) {
        const ModeRoute mr = doc["weight_mode"].is_number_integer()
            ? route_legacy_weight_mode_int(doc["weight_mode"].get<int>())
            : route_phase2_weight_token(doc["weight_mode"].is_string()
                                        ? doc["weight_mode"].get<std::string>()
                                        : std::string("<non-scalar>"));
        emit_route(mr, "config.weight_mode", "config.weight_mode");
        if (mr.kind == RouteKind::kReject) return astrocs::ARGS;
    }
    // 2) 显式 CLI 模式旗标
    if (p.values.count(flag)) {
        const std::string tok = p.values.at(flag);
        const ModeRoute mr = (phase == 2) ? route_phase2_weight_token(tok) : route_phase3_mode(tok);
        emit_route(mr, "cli", flag);
        if (mr.kind == RouteKind::kReject) return astrocs::ARGS;
    }
    return astrocs::OK;
}

}  // namespace v6cli
}  // namespace astrocs
