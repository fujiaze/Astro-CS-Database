// lib/algorithms/resample/p3_input_unit_io.cpp — 输入单位裁决的 I/O 包装
//
// 与 p3_input_unit.cpp（纯逻辑）分 TU 的理由: 单元层测试只需编译纯逻辑 TU 即可
// 覆盖全部声明矩阵，不必链接 aio/cfitsio 闭包；而 I/O 一律经 aio 唯一实现
// （禁本模块自持 <fstream> 通道，CLEAN-403 统一 I/O 纪律）。
#include "p3_input_unit.h"

#include "aio_file_io.h"

namespace acsd::phase3 {

P3InputUnitVerdict p3_input_unit_verdict(const std::string& hips_dir) {
    const std::string path = hips_dir + "/signal/properties";
    P3InputUnitVerdict v;
    v.properties_path = path;
    std::string text;
    if (!aio_file::read_all(path.c_str(), &text)) {
        v.accepted = false;
        v.input_error = true;
        v.code = "P3-INPUT-PROPERTIES-UNREADABLE";
        v.reason = "signal/properties not readable (输入产品身份不可读): " + path;
        return v;
    }
    return p3_input_unit_verdict_from_text(text, path);
}

}  // namespace acsd::phase3
