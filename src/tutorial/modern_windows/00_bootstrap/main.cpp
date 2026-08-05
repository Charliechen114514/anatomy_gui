#include <expected>
#include <print>

// 编译期自检：这些特性宏存在，说明 /std:c++latest 真的把 C++23 拉上来了。
static_assert(__cpp_lib_print    >= 202207L, "需要 C++23 <print>：检查 /std:c++latest 是否生效");
static_assert(__cpp_lib_expected >= 202211L, "需要 C++23 <expected>：检查 /std:c++latest 是否生效");

int main()
{
    // 顺手编一下 expected——后面整个框架的错误模型就靠 std::expected<T, Error>。
    const std::expected<int, int> ok = 42;

    std::println("ModernWindows toolchain self-check OK: expected={}", *ok);
    return 0;
}
