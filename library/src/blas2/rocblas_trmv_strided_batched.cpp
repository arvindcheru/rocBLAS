/* ************************************************************************
 * Copyright 2016-2022 Advanced Micro Devices, Inc.
 * ************************************************************************ */
#include "handle.hpp"
#include "logging.hpp"
#include "rocblas.h"
#include "rocblas_trmv.hpp"
#include "utility.hpp"

namespace
{

    template <typename>
    constexpr char rocblas_trmv_strided_batched_name[] = "unknown";
    template <>
    constexpr char rocblas_trmv_strided_batched_name<float>[] = "rocblas_strmv_strided_batched";
    template <>
    constexpr char rocblas_trmv_strided_batched_name<double>[] = "rocblas_dtrmv_strided_batched";
    template <>
    constexpr char rocblas_trmv_strided_batched_name<rocblas_float_complex>[]
        = "rocblas_ctrmv_strided_batched";
    template <>
    constexpr char rocblas_trmv_strided_batched_name<rocblas_double_complex>[]
        = "rocblas_ztrmv_strided_batched";

    template <typename T>
    rocblas_status rocblas_trmv_strided_batched_impl(rocblas_handle    handle,
                                                     rocblas_fill      uplo,
                                                     rocblas_operation transa,
                                                     rocblas_diagonal  diag,
                                                     rocblas_int       m,
                                                     const T*          a,
                                                     rocblas_int       lda,
                                                     rocblas_stride    stridea,
                                                     T*                x,
                                                     rocblas_int       incx,
                                                     rocblas_stride    stridex,
                                                     rocblas_int       batch_count)
    {
        if(!handle)
            return rocblas_status_invalid_handle;

        if(!handle->is_device_memory_size_query())
        {
            auto layer_mode = handle->layer_mode;
            if(layer_mode
               & (rocblas_layer_mode_log_trace | rocblas_layer_mode_log_bench
                  | rocblas_layer_mode_log_profile))
            {
                auto uplo_letter   = rocblas_fill_letter(uplo);
                auto transa_letter = rocblas_transpose_letter(transa);
                auto diag_letter   = rocblas_diag_letter(diag);
                if(layer_mode & rocblas_layer_mode_log_trace)
                {
                    log_trace(handle,
                              rocblas_trmv_strided_batched_name<T>,
                              uplo,
                              transa,
                              diag,
                              m,
                              a,
                              lda,
                              x,
                              incx,
                              stridea,
                              incx,
                              stridex,
                              batch_count);
                }

                if(layer_mode & rocblas_layer_mode_log_bench)
                {
                    log_bench(handle,
                              "./rocblas-bench",
                              "-f",
                              "trmv_strided_batched",
                              "-r",
                              rocblas_precision_string<T>,
                              "--uplo",
                              uplo_letter,
                              "--transposeA",
                              transa_letter,
                              "--diag",
                              diag_letter,
                              "-m",
                              m,
                              "--lda",
                              lda,
                              "--stride_a",
                              stridea,
                              "--incx",
                              incx,
                              "--stride_x",
                              stridex,
                              "--batch_count",
                              batch_count);
                }

                if(layer_mode & rocblas_layer_mode_log_profile)
                {
                    log_profile(handle,
                                rocblas_trmv_strided_batched_name<T>,
                                "uplo",
                                uplo_letter,
                                "transA",
                                transa_letter,
                                "diag",
                                diag_letter,
                                "M",
                                m,
                                "lda",
                                lda,
                                "stride_a",
                                stridea,
                                "incx",
                                incx,
                                "stride_x",
                                stridex,
                                "batch_count",
                                batch_count);
                }
            }
        }

        size_t         dev_bytes;
        rocblas_status arg_status = rocblas_trmv_arg_check<T>(
            handle, uplo, transa, diag, m, a, lda, x, incx, batch_count, dev_bytes);
        if(arg_status != rocblas_status_continue)
            return arg_status;

        auto workspace = handle->device_malloc(dev_bytes);
        if(!workspace)
            return rocblas_status_memory_error;

        auto check_numerics = handle->check_numerics;

        if(check_numerics)
        {
            bool           is_input = true;
            rocblas_status trmv_check_numerics_status
                = rocblas_trmv_check_numerics(rocblas_trmv_strided_batched_name<T>,
                                              handle,
                                              m,
                                              a,
                                              0,
                                              lda,
                                              stridea,
                                              x,
                                              0,
                                              incx,
                                              stridex,
                                              batch_count,
                                              check_numerics,
                                              is_input);
            if(trmv_check_numerics_status != rocblas_status_success)
                return trmv_check_numerics_status;
        }

        rocblas_stride        stridew  = m;
        constexpr rocblas_int offset_a = 0, offset_x = 0;
        rocblas_status        status = rocblas_internal_trmv_template(handle,
                                                               uplo,
                                                               transa,
                                                               diag,
                                                               m,
                                                               a,
                                                               offset_a,
                                                               lda,
                                                               stridea,
                                                               x,
                                                               offset_x,
                                                               incx,
                                                               stridex,
                                                               (T*)workspace,
                                                               stridew,
                                                               batch_count);
        if(status != rocblas_status_success)
            return status;

        if(check_numerics)
        {
            bool           is_input = false;
            rocblas_status trmv_check_numerics_status
                = rocblas_trmv_check_numerics(rocblas_trmv_strided_batched_name<T>,
                                              handle,
                                              m,
                                              a,
                                              0,
                                              lda,
                                              stridea,
                                              x,
                                              0,
                                              incx,
                                              stridex,
                                              batch_count,
                                              check_numerics,
                                              is_input);
            if(trmv_check_numerics_status != rocblas_status_success)
                return trmv_check_numerics_status;
        }
        return status;
    }

} // namespace

/*
* ===========================================================================
*    C wrapper
* ===========================================================================
*/

extern "C" {

#ifdef IMPL
#error IMPL ALREADY DEFINED
#endif

#define IMPL(routine_name_, T_)                                                             \
    rocblas_status routine_name_(rocblas_handle    handle,                                  \
                                 rocblas_fill      uplo,                                    \
                                 rocblas_operation transA,                                  \
                                 rocblas_diagonal  diag,                                    \
                                 rocblas_int       m,                                       \
                                 const T_*         A,                                       \
                                 rocblas_int       lda,                                     \
                                 rocblas_stride    stridea,                                 \
                                 T_*               x,                                       \
                                 rocblas_int       incx,                                    \
                                 rocblas_stride    stridex,                                 \
                                 rocblas_int       batch_count)                             \
    try                                                                                     \
    {                                                                                       \
        return rocblas_trmv_strided_batched_impl(                                           \
            handle, uplo, transA, diag, m, A, lda, stridea, x, incx, stridex, batch_count); \
    }                                                                                       \
    catch(...)                                                                              \
    {                                                                                       \
        return exception_to_rocblas_status();                                               \
    }

IMPL(rocblas_strmv_strided_batched, float);
IMPL(rocblas_dtrmv_strided_batched, double);
IMPL(rocblas_ctrmv_strided_batched, rocblas_float_complex);
IMPL(rocblas_ztrmv_strided_batched, rocblas_double_complex);

#undef IMPL

} // extern "C"
