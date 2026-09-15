// cli/v6_mode_gate.h — RUNTIME-CI-001: V6 显式 CLI 模式路由门
//
// 单一事实源 = cli/v6_runtime_contract.h。语义（宪章 §3.2 / 冻结 FZ-MODE-*）：
//   * 显式 --mode <token>（phase2）/ --export-mode <token>（phase3）必须经冻结路由表判定：
//     production 放行；baseline 放行但标非生产；reject → ARGS(2)（fail-closed）。
//   * phase2 config 的 legacy 整数 weight_mode：0 → 拒绝（不得进生产）；1|2 → baseline。
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
            std::fprintf(stderr, "astrocs: %s rejected: %s\n", label, mr.reason.c_str());
        } else if (mr.kind == RouteKind::kBaseline) {
            std::fprintf(stderr, "astrocs: WARNING %s=%s is a baseline "
                                 "(non-production) weight surface\n",
                         label, mr.token.c_str());
        }
    };

    // 1) phase2 config 的 legacy 整数 weight_mode（0 必拒）
    if (phase == 2 && have_doc && doc.contains("weight_mode") &&
        doc["weight_mode"].is_number_integer()) {
        const ModeRoute mr = route_legacy_weight_mode_int(doc["weight_mode"].get<int>());
        emit_route(mr, "config.weight_mode", "config.weight_mode");
        if (mr.kind == RouteKind::kReject) return astrocs::ARGS;
    }
    // 2) 显式 CLI 模式旗标
    if (p.values.count(flag)) {
        const std::string tok = p.values.at(flag);
        const ModeRoute mr = (phase == 2) ? route_phase2_mode(tok) : route_phase3_mode(tok);
        emit_route(mr, "cli", flag);
        if (mr.kind == RouteKind::kReject) return astrocs::ARGS;
    }
    return astrocs::OK;
}

}  // namespace v6cli
}  // namespace astrocs
