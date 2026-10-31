#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>
#define main isolated_pow_probe_main
#include "pow-probe.cpp"
#undef main
int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::setvbuf(stderr, nullptr, _IONBF, 0);
    AddVectoredExceptionHandler(1, record_exception);
    doctest::Context context;
    context.applyCommandLine(argc, argv);
    return context.run();
}
