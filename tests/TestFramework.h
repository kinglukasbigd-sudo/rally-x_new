#pragma once
#include <cstdio>
#include <cmath>
#include <string>
#include <vector>
#include <functional>

// Deliberately tiny: these tests exercise gameplay rules, so the harness
// stays out of the way.
namespace test {

struct Case { const char* name; std::function<void()> fn; };
inline std::vector<Case>& registry() { static std::vector<Case> r; return r; }
inline int& failures() { static int f = 0; return f; }
inline const char*& currentCase() { static const char* c = ""; return c; }

struct Register { Register(const char* n, std::function<void()> f) { registry().push_back({n, f}); } };

inline void fail(const char* file, int line, const std::string& msg) {
    ++failures();
    std::printf("    FAIL %s:%d  %s\n", file, line, msg.c_str());
}

inline int run() {
    int failed = 0;
    for (auto& c : registry()) {
        const int before = failures();
        currentCase() = c.name;
        c.fn();
        const bool ok = (failures() == before);
        std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", c.name);
        if (!ok) ++failed;
    }
    std::printf("\n%zu tests, %d failed\n", registry().size(), failed);
    return failed == 0 ? 0 : 1;
}

} // namespace test

#define TEST(name)                                                            \
    static void name();                                                       \
    static ::test::Register reg_##name(#name, name);                          \
    static void name()

#define CHECK(cond)                                                           \
    do { if (!(cond)) ::test::fail(__FILE__, __LINE__, "CHECK(" #cond ")"); } while (0)

#define CHECK_EQ(a, b)                                                        \
    do { auto va = (a); auto vb = (b);                                        \
         if (!(va == vb)) ::test::fail(__FILE__, __LINE__,                    \
            std::string(#a " == " #b " (got ") + std::to_string(va) +         \
            ", want " + std::to_string(vb) + ")"); } while (0)

#define CHECK_NEAR(a, b, eps)                                                 \
    do { double va = (a), vb = (b);                                           \
         if (std::fabs(va - vb) > (eps)) ::test::fail(__FILE__, __LINE__,     \
            std::string(#a " ~= " #b " (got ") + std::to_string(va) +         \
            ", want " + std::to_string(vb) + ")"); } while (0)
