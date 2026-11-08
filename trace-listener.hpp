#include <cstdio>
struct TraceListener : doctest::IReporter
{
    explicit TraceListener(const doctest::ContextOptions&) {}
    static void mark(const char* kind, const char* name = "")
    {
        std::fprintf(stderr, "XSIMD_TRACE %s %s\n", kind, name);
        std::fflush(stderr);
    }
    void report_query(const doctest::QueryData&) override { mark("QUERY"); }
    void test_run_start() override { mark("RUN_START"); }
    void test_run_end(const doctest::TestRunStats&) override { mark("RUN_END"); }
    void test_case_start(const doctest::TestCaseData& t) override { mark("CASE", t.m_name); }
    void test_case_reenter(const doctest::TestCaseData& t) override { mark("REENTER", t.m_name); }
    void test_case_end(const doctest::CurrentTestCaseStats&) override { mark("CASE_END"); }
    void test_case_exception(const doctest::TestCaseException&) override { mark("EXCEPTION"); }
    void subcase_start(const doctest::SubcaseSignature& s) override { mark("SUBCASE", s.m_name.c_str()); }
    void subcase_end() override { mark("SUBCASE_END"); }
    void log_assert(const doctest::AssertData&) override {}
    void log_message(const doctest::MessageData&) override {}
    void test_case_skipped(const doctest::TestCaseData&) override {}
};
DOCTEST_REGISTER_LISTENER("xsimd-trace", 0, TraceListener);
