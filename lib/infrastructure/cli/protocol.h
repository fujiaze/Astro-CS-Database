// JSONL 事件协议 v1 冻结合同 — CLI-004 (GUI 可调用进程协议, 控制包 02 CLI-004)
// 权威: docs/api/CLI_PROTOCOL_V1.md §4 (上游 04 §4 字段冻结) + schemas/jsonl_event_v1.schema.json
// 职责: 协议面唯一验证点 —— 外部 harness/GUI 只消费本协议面(禁链接科学库绕过 CLI)。
// 语义冻结, 发送侧 ValidateEventV1 自检硬闸: 非法事件拒发(stderr 诊断, stdout 纯净),
// 机器一致性校验(测试/CI)用 ValidateEventV1(读侧, 独立重实现协议文本, 防同源盲区)。
#pragma once

#include <nlohmann/json.hpp>

#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "exit_codes.h"

namespace astrocs {

// §4: 每行必含字段表(存在性冻结; kind 扩展字段另册)。字段数经 sizeof 推导,
// 不写字面量数值(04 §6-3 退出码/协议数值单源纪律)。
struct EventFieldsV1 {
    const char* names[10];
};
inline constexpr EventFieldsV1 kEventFieldsV1{{"schema_version", "event_id",   "run_id",
                                               "timestamp_utc",  "sequence",   "kind",
                                               "severity",       "phase",      "stage",
                                               "message"}};
inline constexpr int kEventFieldCountV1 =
    static_cast<int>(sizeof(kEventFieldsV1.names) / sizeof(kEventFieldsV1.names[0]));

// §2/§4 final 事件: exit_code 合法域 = 11 条冻结退出码(04 §2 唯一源 exit_codes.h)。
inline bool is_frozen_exit_code_v1(int c) {
    return c == OK || c == ARGS || c == INPUT || c == SCIENCE || c == BACKEND ||
           c == COMPUTE || c == IO || c == INTEGRITY || c == CANCELLED ||
           c == RESOURCE || c == INTERNAL;
}

// §4 kind 扩展字段冻结名册(04 §4 逐字; final 扩展由 emit_final 固定)。
// 返回缺失的必含扩展字段; kind 在基础五类之外 → 空串(开放 kind, 扩展字段不校验)。
inline std::string missing_required_extension_v1(const std::string& kind,
                                                 const nlohmann::json& ev) {
    static const std::map<std::string, std::vector<std::string>> kExt = {
        {"progress", {"completed", "total", "unit", "rate", "eta_seconds"}},
        {"resource", {"cpu_cores_used", "rss_bytes", "io_read_bytes", "io_write_bytes",
                      "threads"}},
        {"artifact", {"role", "path", "sha256", "size_bytes"}},
        {"backend", {"kernel", "backend_id", "isa", "workers", "block_size", "reason"}},
        {"final", {"exit_code", "status", "run_manifest", "summary"}},
    };
    auto it = kExt.find(kind);
    if (it == kExt.end()) return {};
    for (const auto& f : it->second) {
        if (!ev.contains(f)) return f;
    }
    return {};
}

// 发送侧协议自检(硬闸): 校验 10 必含字段 + sequence 单调(0 起) + kind 扩展字段 +
// final.exit_code ∈ 冻结退出码域。违规返回 false 并 stderr 诊断(调用方禁发该行,
// stdout 保持纯 JSONL, 诊断只走 stderr — §3)。
inline bool ValidateEventV1(const nlohmann::json& ev, unsigned long long expect_seq) {
    if (!ev.is_object()) {
        std::fprintf(stderr, "astrocs: protocol: event is not an object\n");
        return false;
    }
    for (const char* f : kEventFieldsV1.names) {
        if (!ev.contains(f)) {
            std::fprintf(stderr, "astrocs: protocol: event missing required field '%s'\n", f);
            return false;
        }
    }
    if (!ev["sequence"].is_number_integer() ||
        static_cast<unsigned long long>(ev["sequence"].get<long long>()) != expect_seq) {
        std::fprintf(stderr,
                     "astrocs: protocol: sequence must be %llu (monotonic from 0)\n",
                     expect_seq);
        return false;
    }
    const std::string kind = ev.value("kind", std::string());
    const std::string missing = missing_required_extension_v1(kind, ev);
    if (!missing.empty()) {
        std::fprintf(stderr,
                     "astrocs: protocol: kind '%s' missing frozen extension field '%s'\n",
                     kind.c_str(), missing.c_str());
        return false;
    }
    if (kind == "final" && (!ev["exit_code"].is_number_integer() ||
                            !is_frozen_exit_code_v1(ev["exit_code"].get<int>()))) {
        std::fprintf(stderr,
                     "astrocs: protocol: final.exit_code=%d outside frozen 04 §2 domain\n",
                     ev.value("exit_code", -1));
        return false;
    }
    return true;
}

}  // namespace astrocs
