// 夹具 · 正例面: 全部是「对返回�� / 变量 / 成员判空」，判据**不得**判红。
#include "legit_checks.hpp"
#include <cstdlib>
#include <cstring>

static const char* find_name(const char* key) { return key; }
struct Node { Node* next; };

int legit_function_return() {
    const char* a = std::getenv("FIXTURE_ENV");
    if (a == nullptr) return 1;                       // 合法: 库函数返回值
    if (find_name(a) == nullptr) return 2;            // 合法: 库函数返回值
    Node* c = find_node();
    if (c == nullptr) return 3;                       // 合法: 函数返回值
    if (c->next != nullptr) return 4;                 // 合法: 成员取值
    char* buf = std::strdup(a);
    if (buf != nullptr) { std::free(buf); }           // 合法: 返回值判空后使用
    return 0;
}

static Node* dummy;
static Node* find_node() { return dummy; }

int legit_macro_call_sites() {
    fixpos::CheckState cs;
    const char* runtime_name = std::getenv("FIXTURE_NAME");   // 运行期实参
    FIXPOS_CHECK(cs, true, runtime_name);                     // 合法: 守卫可失败
    FIXPOS_CHECK(cs, true, nullptr);                          // 合法: nullptr 实参
    FIXPOS_CHECK(cs, true, "");                               // 合法: 空串实参
    return cs.failures;
}
