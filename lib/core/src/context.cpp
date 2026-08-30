// CORE-005 RunContext 实现
#include "astrocs/core/context.h"

namespace astrocs::core {

void RunContext::log(LogLevel level, const std::string& component,
                     const std::string& message) {
  const char* lv = level == LogLevel::DEBUG ? "DEBUG"
                  : level == LogLevel::INFO ? "INFO"
                  : level == LogLevel::WARN ? "WARN" : "ERROR";
  log_entries_.push_back(std::string("[") + lv + "][" + component + "] " + message);
}

Result<void> RunContext::store_artifact(DataArtifactDescriptor desc) {
  std::string err;
  if (!desc.validate(&err)) {
    return Result<void>::fail(Error(ErrorDomain::DATA, "store_artifact: " + err));
  }
  if (artifacts_.count(desc.id.id)) {
    return Result<void>::fail(Error(ErrorDomain::DATA,
        "store_artifact: duplicate id " + desc.id.id));
  }
  artifacts_[desc.id.id] = std::move(desc);
  return Result<void>::success();
}

const DataArtifactDescriptor* RunContext::get_artifact(const std::string& id) const {
  auto it = artifacts_.find(id);
  return it == artifacts_.end() ? nullptr : &it->second;
}

std::vector<std::string> RunContext::artifact_ids() const {
  std::vector<std::string> out;
  out.reserve(artifacts_.size());
  for (const auto& [k, v] : artifacts_) out.push_back(k);
  return out;
}

}  // namespace astrocs::core
