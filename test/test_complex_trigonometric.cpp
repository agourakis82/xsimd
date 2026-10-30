/***************************************************************************
 * Copyright (c) Johan Mabille, Sylvain Corlay, Wolf Vollprecht and         *
 * Martin Renou                                                             *
 * Copyright (c) QuantStack                                                 *
 * Copyright (c) Serge Guelton                                              *
 *                                                                          *
 * Distributed under the terms of the BSD 3-Clause License.                 *
 *                                                                          *
 * The full license is in the file LICENSE, distributed with this software. *
 ****************************************************************************/

#include "xsimd/xsimd.hpp"

#ifndef XSIMD_NO_SUPPORTED_ARCHITECTURE

#include "test_utils.hpp"

template <class B>
struct complex_trigonometric_test
{
    using batch_type = B;
    using real_batch_type = typename B::real_batch;
    using value_type = typename B::value_type;
    using real_value_type = typename value_type::value_type;
    static constexpr size_t size = B::size;
    using vector_type = std::vector<value_type>;

    size_t nb_input;
    vector_type input;
    vector_type ainput;
    vector_type atan_input;
    vector_type expected;

    complex_trigonometric_test()
    {
        nb_input = size * 10000;
        input.resize(nb_input);
        ainput.resize(nb_input);
        atan_input.resize(nb_input);
        for (size_t i = 0; i < nb_input; ++i)
        {
            input[i] = value_type(real_value_type(0.) + i * real_value_type(80.) / nb_input,
                                  real_value_type(0.1) + i * real_value_type(56.) / nb_input);
            ainput[i] = value_type(real_value_type(-1.) + real_value_type(2.) * i / nb_input,
                                   real_value_type(-1.1) + real_value_type(2.1) * i / nb_input);
            // Avoid sampling the branch cut of complex atan at (Re=0, |Im|>=1):
            // the sign of Re(catan(+/-0 + i*y)) for |y|>1 is implementation-defined
            // (C99 7.3.4.1), and glibc and musl disagree there. Starting Re at -9.5
            // makes Re=0 occur at i=19/40 of the range, where |Im|<1 (analytic).
            atan_input[i] = value_type(real_value_type(-9.5) + i * real_value_type(20.) / nb_input,
                                       real_value_type(-9.) + i * real_value_type(21.) / nb_input);
        }
        expected.resize(nb_input);
    }

    void test_sin()
    {
        std::transform(input.cbegin(), input.cend(), expected.begin(),
                       [](const value_type& v)
                       { using std::sin; return sin(v); });
        for (size_t i = 0; i < nb_input; i += size)
        {
            batch_type in, out, ref;
            detail::load_batch(in, input, i);
            out = sin(in);
            detail::load_batch(ref, expected, i);
            CHECK_BATCH_EQ(ref, out);
        }
    }

    void test_cos()
    {
        std::transform(input.cbegin(), input.cend(), expected.begin(),
                       [](const value_type& v)
                       { using std::cos; return cos(v); });
        for (size_t i = 0; i < nb_input; i += size)
        {
            batch_type in, out, ref;
            detail::load_batch(in, input, i);
            out = cos(in);
            detail::load_batch(ref, expected, i);
            CHECK_BATCH_EQ(ref, out);
        }
    }

    void test_sincos()
    {
        vector_type expected2(nb_input), res2(nb_input);
        std::transform(input.cbegin(), input.cend(), expected.begin(),
                       [](const value_type& v)
                       { using std::sin; return sin(v); });
        std::transform(input.cbegin(), input.cend(), expected2.begin(),
                       [](const value_type& v)
                       { using std::cos; return cos(v); });
        for (size_t i = 0; i < nb_input; i += size)
        {
            batch_type in, out1, out2, ref1, ref2;
            detail::load_batch(in, input, i);
            std::tie(out1, out2) = sincos(in);

            detail::load_batch(ref1, expected, i);
            CHECK_BATCH_EQ(ref1, out1);

            detail::load_batch(ref2, expected2, i);
            CHECK_BATCH_EQ(ref2, out2);
        }
    }

    void test_tan()
    {
        test_conditional_tan<real_value_type>();
    }

    void test_asin()
    {
        std::transform(ainput.cbegin(), ainput.cend(), expected.begin(),
                       [](const value_type& v)
                       { using std::asin; return asin(v); });
        for (size_t i = 0; i < nb_input; i += size)
        {
            batch_type in, out, ref;
            detail::load_batch(in, ainput, i);
            out = asin(in);
            detail::load_batch(ref, expected, i);
            CHECK_BATCH_EQ(ref, out);
        }
    }

    void test_acos()
    {
        std::transform(ainput.cbegin(), ainput.cend(), expected.begin(),
                       [](const value_type& v)
                       { using std::acos; return acos(v); });
        for (size_t i = 0; i < nb_input; i += size)
        {
            batch_type in, out, ref;
            detail::load_batch(in, ainput, i);
            out = acos(in);
            detail::load_batch(ref, expected, i);
            CHECK_BATCH_EQ(ref, out);
        }
    }

    void test_atan()
    {
        std::transform(atan_input.cbegin(), atan_input.cend(), expected.begin(),
                       [](const value_type& v)
                       { using std::atan; return atan(v); });
        for (size_t i = 0; i < nb_input; i += size)
        {
            batch_type in, out, ref;
            detail::load_batch(in, atan_input, i);
            out = atan(in);
            detail::load_batch(ref, expected, i);
            CHECK_BATCH_EQ(ref, out);
        }
    }

private:
    void test_tan_impl()
    {
        std::transform(input.cbegin(), input.cend(), expected.begin(),
                       [](const value_type& v)
                       { using std::tan; return tan(v); });
        for (size_t i = 0; i < nb_input; i += size)
        {
            batch_type in, out, ref;
            detail::load_batch(in, input, i);
            out = tan(in);
            detail::load_batch(ref, expected, i);
            CHECK_BATCH_EQ(ref, out);
        }
    }

    template <class T, std::enable_if_t<!std::is_same_v<T, float>, int> = 0>
    void test_conditional_tan()
    {
        test_tan_impl();
    }

    template <class T, std::enable_if_t<std::is_same_v<T, float>, int> = 0>
    void test_conditional_tan()
    {
#if (XSIMD_ARM_INSTR_SET >= XSIMD_ARM7_NEON_VERSION)
#if DEBUG_ACCURACY
        test_tan_impl();
#endif
#else
        test_tan_impl();
#endif
    }
};

TEST_CASE_TEMPLATE("[complex trigonometric]", B, BATCH_COMPLEX_TYPES)
{
    complex_trigonometric_test<B> Test;
    SUBCASE("sin")
    {
        Test.test_sin();
    }

    SUBCASE("cos")
    {
        Test.test_cos();
    }

    SUBCASE("sincos")
    {
        Test.test_sincos();
    }

    SUBCASE("tan")
    {
        Test.test_tan();
    }

    SUBCASE("asin")
    {
        Test.test_asin();
    }

    SUBCASE("acos")
    {
        Test.test_acos();
    }

    SUBCASE("atan")
    {
        Test.test_atan();
    }
}
#ifndef __FAST_MATH__
TEST_CASE_TEMPLATE("[complex asin acos real axis]", B, BATCH_COMPLEX_TYPES)
{
    using value_type = typename B::value_type;
    using real_type = typename value_type::value_type;
    const real_type zero(0);
    const auto outside = std::nextafter(real_type(1), real_type(2));
    const std::array<real_type, 12> real_parts = {
        real_type(-4), real_type(-2), -outside, real_type(-1), real_type(-0.5), -zero,
        zero, real_type(0.5), real_type(1), outside, real_type(2), real_type(4)
    };
    std::array<value_type, 28> samples;
    for (size_t i = 0; i < real_parts.size(); ++i)
    {
        samples[2 * i] = value_type(real_parts[i], zero);
        samples[2 * i + 1] = value_type(real_parts[i], -zero);
    }
    samples[24] = value_type(-2, -1);
    samples[25] = value_type(2, 1);
    samples[26] = value_type(real_type(-0.5), real_type(0.5));
    samples[27] = value_type(real_type(0.5), real_type(-0.5));
    std::array<value_type, B::size> inputs;
    std::array<value_type, B::size> results;
    const auto tolerance = real_type(64) * std::numeric_limits<real_type>::epsilon();
    for (size_t start = 0; start < samples.size(); ++start)
    {
        for (size_t lane = 0; lane < B::size; ++lane)
        {
            inputs[lane] = samples[(start + lane) % samples.size()];
        }
        for (bool cosine : { false, true })
        {
            const auto batch = B::load_unaligned(inputs.data());
            (cosine ? xsimd::acos(batch) : xsimd::asin(batch)).store_unaligned(results.data());
            for (size_t lane = 0; lane < B::size; ++lane)
            {
                const auto expected = cosine ? std::acos(inputs[lane]) : std::asin(inputs[lane]);
                CHECK_EQ(results[lane].real(), doctest::Approx(expected.real()).epsilon(tolerance).scale(1));
                CHECK_EQ(results[lane].imag(), doctest::Approx(expected.imag()).epsilon(tolerance).scale(1));
                if (expected.real() == zero)
                {
                    CHECK_EQ(results[lane].real(), zero);
                    CHECK_EQ(std::signbit(results[lane].real()), std::signbit(expected.real()));
                }
                if (expected.imag() == zero)
                {
                    CHECK_EQ(results[lane].imag(), zero);
                    CHECK_EQ(std::signbit(results[lane].imag()), std::signbit(expected.imag()));
                }
            }
        }
    }
}
#endif
#endif
