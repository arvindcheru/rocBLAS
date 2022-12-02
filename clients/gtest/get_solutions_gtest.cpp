/* ************************************************************************
 * Copyright (C) 2018-2022 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell cop-
 * ies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IM-
 * PLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNE-
 * CTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * ************************************************************************ */
#define ROCBLAS_NO_DEPRECATED_WARNINGS
#define ROCBLAS_BETA_FEATURES_API
#include "rocblas_data.hpp"

#include "rocblas_datatype2string.hpp"
#include "rocblas_test.hpp"
#include "testing_gemm_batched_ex_get_solutions.hpp"
#include "testing_gemm_ex_get_solutions.hpp"
#include "testing_gemm_strided_batched_ex_get_solutions.hpp"
#include "type_dispatch.hpp"
#include <cctype>
#include <cstring>
#include <type_traits>

namespace
{
    // Types of GEMM tests
    enum get_solutions_test_type
    {
        GEMM_EX,
        GEMM_BATCHED_EX,
        GEMM_STRIDED_BATCHED_EX,
    };

    // ----------------------------------------------------------------------------
    // get_solutions testing template
    // ----------------------------------------------------------------------------
    // The first template parameter is a class template which determines which
    // combination of types applies to this test, and for those that do, instantiates
    // the test code based on the function named in the test Arguments. The second
    // template parameter is an enum which allows the different flavors of GEMM_EX to
    // be differentiated.
    //
    // The RocBLAS_Test base class takes this class (CRTP) and the first template
    // parameter as arguments, and provides common types such as type_filter_functor,
    // and derives from the Google Test parameterized base classes.
    //
    // This class defines functions for filtering the types and function names which
    // apply to this test, and for generating the suffix of the Google Test name
    // corresponding to each instance of this test.
    template <template <typename...> class FILTER, get_solutions_test_type GEMM_TYPE>
    struct get_solutions_test_template
        : RocBLAS_Test<get_solutions_test_template<FILTER, GEMM_TYPE>, FILTER>
    {
        // Filter for which types apply to this suite
        static bool type_filter(const Arguments& arg)
        {
            return rocblas_gemm_dispatch<get_solutions_test_template::template type_filter_functor>(
                arg);
        }

        // Filter for which functions apply to this suite
        static bool function_filter(const Arguments& arg)
        {
            switch(GEMM_TYPE)
            {
#if(BUILD_WITH_TENSILE)
            case GEMM_EX:
                return !strcmp(arg.function, "gemm_ex_get_solutions");
            case GEMM_BATCHED_EX:
                return !strcmp(arg.function, "gemm_batched_ex_get_solutions");
            case GEMM_STRIDED_BATCHED_EX:
                return !strcmp(arg.function, "gemm_strided_batched_ex_get_solutions");
#endif
            }
            return false;
        }

        // Google Test name suffix based on parameters
        static std::string name_suffix(const Arguments& arg)
        {
            RocBLAS_TestName<get_solutions_test_template> name(arg.name);
            name << rocblas_datatype2string(arg.a_type);

            constexpr bool isBatched
                = (GEMM_TYPE == GEMM_STRIDED_BATCHED_EX || GEMM_TYPE == GEMM_BATCHED_EX);

            name << rocblas_datatype2string(arg.b_type) << rocblas_datatype2string(arg.c_type)
                 << rocblas_datatype2string(arg.d_type)
                 << rocblas_datatype2string(arg.compute_type);

            name << '_' << (char)std::toupper(arg.transA) << (char)std::toupper(arg.transB);

            name << '_' << arg.M << '_' << arg.N << '_' << arg.K << '_' << arg.alpha << '_'
                 << arg.lda << '_' << arg.ldb << '_' << arg.beta << '_' << arg.ldc;

            name << '_' << arg.ldd;

            if(isBatched)
                name << '_' << arg.batch_count;

            if(GEMM_TYPE == GEMM_STRIDED_BATCHED_EX)
                name << '_' << arg.stride_a << '_' << arg.stride_b << '_' << arg.stride_c;

            if(arg.fortran)
            {
                name << "_F";
            }

            return std::move(name);
        }
    };

#if(BUILD_WITH_TENSILE)
    // ----------------------------------------------------------------------------
    // gemm_ex
    // gemm_batched_ex
    // gemm_strided_batched_ex
    // ----------------------------------------------------------------------------

    // In the general case of <Ti, To, Tc>, these tests do not apply, and if this
    // functor is called, an internal error message is generated. When converted
    // to bool, this functor returns false.
    template <typename Ti, typename To = Ti, typename Tc = To, typename = void>
    struct get_solutions_testing : rocblas_test_invalid
    {
    };

    // When Ti != void, this test applies.
    // When converted to bool, this functor returns true.
    template <typename Ti, typename To, typename Tc>
    struct get_solutions_testing<
        Ti,
        To,
        Tc,
        std::enable_if_t<!std::is_same<Ti, void>{}
                         && !(std::is_same<Ti, Tc>{} && std::is_same<Ti, rocblas_bfloat16>{})>>
        : rocblas_test_valid
    {
        void operator()(const Arguments& arg)
        {
            if(!strcmp(arg.function, "gemm_ex_get_solutions"))
                testing_gemm_ex_get_solutions<Ti, To, Tc>(arg);
            else if(!strcmp(arg.function, "gemm_batched_ex_get_solutions"))
                testing_gemm_batched_ex_get_solutions<Ti, To, Tc>(arg);
            else if(!strcmp(arg.function, "gemm_strided_batched_ex_get_solutions"))
                testing_gemm_strided_batched_ex_get_solutions<Ti, To, Tc>(arg);
            else
                FAIL() << "Internal error: Test called with unknown function: " << arg.function;
        }
    };

    using gemm_ex_get_solutions = get_solutions_test_template<get_solutions_testing, GEMM_EX>;
    TEST_P(gemm_ex_get_solutions, blas3_tensile)
    {
        RUN_TEST_ON_THREADS_STREAMS(rocblas_gemm_dispatch<get_solutions_testing>(GetParam()));
    }
    INSTANTIATE_TEST_CATEGORIES(gemm_ex_get_solutions);

    using gemm_batched_ex_get_solutions
        = get_solutions_test_template<get_solutions_testing, GEMM_BATCHED_EX>;
    TEST_P(gemm_batched_ex_get_solutions, blas3_tensile)
    {
        CATCH_SIGNALS_AND_EXCEPTIONS_AS_FAILURES(
            rocblas_gemm_dispatch<get_solutions_testing>(GetParam()));
    }
    INSTANTIATE_TEST_CATEGORIES(gemm_batched_ex_get_solutions);

    using gemm_strided_batched_ex_get_solutions
        = get_solutions_test_template<get_solutions_testing, GEMM_STRIDED_BATCHED_EX>;
    TEST_P(gemm_strided_batched_ex_get_solutions, blas3_tensile)
    {
        CATCH_SIGNALS_AND_EXCEPTIONS_AS_FAILURES(
            rocblas_gemm_dispatch<get_solutions_testing>(GetParam()));
    }
    INSTANTIATE_TEST_CATEGORIES(gemm_strided_batched_ex_get_solutions);
#endif //  BUILD_WITH_TENSILE

} // namespace
