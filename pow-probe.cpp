#include <windows.h>
#include <xsimd/xsimd.hpp>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <type_traits>
#include <vector>

volatile std::size_t current_batch = 0;
const char* volatile current_phase = "setup";

LONG CALLBACK record_exception(EXCEPTION_POINTERS* info) {
    char message[512];
    auto pc = info->ContextRecord->Rip;
    HMODULE module = nullptr;
    char module_name[MAX_PATH] = "unknown";
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCSTR>(pc), &module);
    if (module) GetModuleFileNameA(module, module_name, MAX_PATH);
    auto base = reinterpret_cast<std::uintptr_t>(module);
    int count = std::snprintf(message, sizeof(message),
        "EXCEPTION code=0x%08lX phase=%s element_offset=%llu MXCSR=0x%08lX RIP=0x%llX module=%s module_offset=0x%llX\n",
        info->ExceptionRecord->ExceptionCode, current_phase,
        static_cast<unsigned long long>(current_batch), info->ContextRecord->MxCsr,
        static_cast<unsigned long long>(pc), module_name, static_cast<unsigned long long>(pc - base));
    if (count < 0) return EXCEPTION_CONTINUE_SEARCH;
    if (static_cast<std::size_t>(count) >= sizeof(message)) count = sizeof(message) - 1;
    DWORD written;
    WriteFile(GetStdHandle(STD_ERROR_HANDLE), message, static_cast<DWORD>(count), &written, nullptr);
    return EXCEPTION_CONTINUE_SEARCH;
}

template<class T>
__attribute__((noinline)) xsimd::batch<T> operation(xsimd::batch<T> x, xsimd::batch<T> y) {
    return xsimd::pow(x, y);
}

template<class T>
int run(const std::string& mode, bool positive_only) {
    using B = xsimd::batch<T>;
    constexpr std::size_t width = B::size;
    const std::size_t count = width * 10000;
    std::vector<T> xs(count), ys(count);
    for (std::size_t i = 0; i < count; ++i) {
        T x = T(i / 4 + 1.2 * std::sqrt(i + 0.25));
        xs[i] = positive_only ? x : x * (i % 2);
        ys[i] = T(10.2 / (i + 2) + 0.25);
    }
    const unsigned saved = _mm_getcsr();
    const unsigned clean = (saved | _MM_MASK_MASK) & ~0x3fu;
    std::array<T, width> values{};
    volatile T sink{};
    if (mode == "sticky") {
        B x = B::load_unaligned(xs.data()), y = B::load_unaligned(ys.data());
        current_phase = "sticky-finite-add";
        _mm_setcsr((clean | _MM_EXCEPT_INVALID) & ~_MM_MASK_INVALID);
        (x + y).store_unaligned(values.data());
        for (auto value : values) sink = value;
        const unsigned after = _mm_getcsr();
        _mm_setcsr(saved);
        std::printf("STICKY_CONTROL_SURVIVED mxcsr=0x%08x width=%zu\n", after, width);
        return 0;
    }
    for (std::size_t i = 0; i < count; i += width) {
        B x = B::load_unaligned(xs.data() + i), y = B::load_unaligned(ys.data() + i);
        current_batch = i;
        current_phase = "pow";
        _mm_setcsr(mode == "trap" ? clean & ~_MM_MASK_INVALID : clean);
        operation(x, y).store_unaligned(values.data());
        for (auto value : values) sink = value;
        const unsigned after = _mm_getcsr();
        _mm_setcsr(saved);
        if (after & _MM_EXCEPT_INVALID) {
            std::printf("POW_INVALID batch=%zu mxcsr=0x%08x width=%zu positive=%d\n", i, after, width, positive_only);
            for (std::size_t lane = 0; lane < width; ++lane) {
                using U = std::conditional_t<sizeof(T) == 4, std::uint32_t, std::uint64_t>;
                std::printf("lane=%zu x=0x%llx y=0x%llx result=0x%llx\n", lane,
                    static_cast<unsigned long long>(std::bit_cast<U>(xs[i + lane])),
                    static_cast<unsigned long long>(std::bit_cast<U>(ys[i + lane])),
                    static_cast<unsigned long long>(std::bit_cast<U>(values[lane])));
            }
            return 2;
        }
    }
    std::printf("POW_SCAN_PASS batches=10000 width=%zu positive=%d mode=%s\n", width, positive_only, mode.c_str());
    return 0;
}

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    AddVectoredExceptionHandler(1, record_exception);
    if (argc != 4) return 64;
    const std::string mode(argv[1]);
    if (mode != "sticky" && mode != "masked" && mode != "trap") return 64;
    return std::string(argv[2]) == "float"
        ? run<float>(mode, std::string(argv[3]) == "positive")
        : run<double>(mode, std::string(argv[3]) == "positive");
}
