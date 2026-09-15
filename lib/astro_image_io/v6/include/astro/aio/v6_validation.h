/* v6_validation.h — 统一 fail-closed 校验报告类型
 *
 * 任务: IMPL-AIO-001。语义锚: FZ-PROV-MINIMAL-SET / FZ-BUNIT-SEMANTICS /
 * ALG-P3-008 §7.3 / 宪章 §14.4 (fail-fast)。所有门命中即 append Violation；
 * ok() 仅当无任何违规。禁止"无违规即静默通过"以外的语义。
 */
#ifndef ASTROCS_V6_AIO_VALIDATION_H
#define ASTROCS_V6_AIO_VALIDATION_H

#include <string>
#include <vector>

namespace astrocs {
namespace aio {

struct Violation {
  std::string gate;       // 门 id，如 G-PROV-MINIMAL-SET
  std::string freeze_id;  // 冻结条款 id，如 FZ-PROV-MINIMAL-SET
  std::string message;    // 人读原因 (不得为空占位)
};

class ValidationReport {
 public:
  bool ok() const { return violations_.empty(); }
  const std::vector<Violation>& violations() const { return violations_; }
  void add(const std::string& gate, const std::string& freeze_id,
           const std::string& message) {
    violations_.push_back(Violation{gate, freeze_id, message});
  }
  void merge(const ValidationReport& other) {
    for (const auto& v : other.violations_) violations_.push_back(v);
  }
  std::string summary() const {
    std::string s;
    for (const auto& v : violations_) {
      if (!s.empty()) s += "; ";
      s += v.gate + "(" + v.freeze_id + "): " + v.message;
    }
    return s;
  }

 private:
  std::vector<Violation> violations_;
};

}  // namespace aio
}  // namespace astrocs

#endif  // ASTROCS_V6_AIO_VALIDATION_H
