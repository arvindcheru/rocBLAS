/* ************************************************************************
 * Copyright (C) 2018-2024 Advanced Micro Devices, Inc. All rights reserved.
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

#pragma once

#include "testing_common.hpp"

template <typename T, typename U = T, typename V = T>
void testing_rot_batched_bad_arg(const Arguments& arg)
{
    auto rocblas_rot_batched_fn = arg.api == FORTRAN ? rocblas_rot_batched<T, U, V, true>
                                                     : rocblas_rot_batched<T, U, V, false>;

    auto rocblas_rot_batched_fn_64 = arg.api == FORTRAN_64 ? rocblas_rot_batched_64<T, U, V, true>
                                                           : rocblas_rot_batched_64<T, U, V, false>;

    int64_t N           = 100;
    int64_t incx        = 1;
    int64_t incy        = 1;
    int64_t batch_count = 5;

    rocblas_local_handle handle{arg};

    // Allocate device memory
    device_batch_vector<T> dx(N, incx, batch_count);
    device_batch_vector<T> dy(N, incy, batch_count);
    device_vector<U>       dc(1, 1);
    device_vector<V>       ds(1, 1);

    // Check device memory allocation
    CHECK_DEVICE_ALLOCATION(dx.memcheck());
    CHECK_DEVICE_ALLOCATION(dy.memcheck());
    CHECK_DEVICE_ALLOCATION(dc.memcheck());
    CHECK_DEVICE_ALLOCATION(ds.memcheck());

    DAPI_EXPECT(
        rocblas_status_invalid_handle,
        rocblas_rot_batched_fn,
        (nullptr, N, dx.ptr_on_device(), incx, dy.ptr_on_device(), incy, dc, ds, batch_count));

    DAPI_EXPECT(rocblas_status_invalid_pointer,
                rocblas_rot_batched_fn,
                (handle, N, nullptr, incx, dy.ptr_on_device(), incy, dc, ds, batch_count));
    DAPI_EXPECT(rocblas_status_invalid_pointer,
                rocblas_rot_batched_fn,
                (handle, N, dx.ptr_on_device(), incx, nullptr, incy, dc, ds, batch_count));
    DAPI_EXPECT(
        rocblas_status_invalid_pointer,
        rocblas_rot_batched_fn,
        (handle, N, dx.ptr_on_device(), incx, dy.ptr_on_device(), incy, nullptr, ds, batch_count));
    DAPI_EXPECT(
        rocblas_status_invalid_pointer,
        rocblas_rot_batched_fn,
        (handle, N, dx.ptr_on_device(), incx, dy.ptr_on_device(), incy, dc, nullptr, batch_count));
}

template <typename T, typename U = T, typename V = T>
void testing_rot_batched(const Arguments& arg)
{
    auto rocblas_rot_batched_fn = arg.api == FORTRAN ? rocblas_rot_batched<T, U, V, true>
                                                     : rocblas_rot_batched<T, U, V, false>;

    auto rocblas_rot_batched_fn_64 = arg.api == FORTRAN_64 ? rocblas_rot_batched_64<T, U, V, true>
                                                           : rocblas_rot_batched_64<T, U, V, false>;

    int64_t N           = arg.N;
    int64_t incx        = arg.incx;
    int64_t incy        = arg.incy;
    int64_t batch_count = arg.batch_count;

    rocblas_local_handle handle{arg};
    double               cpu_time_used;
    double norm_error_host_x = 0.0, norm_error_host_y = 0.0, norm_error_device_x = 0.0,
           norm_error_device_y = 0.0;

    // check to prevent undefined memory allocation error
    if(N <= 0 || batch_count <= 0)
    {
        CHECK_ROCBLAS_ERROR(rocblas_set_pointer_mode(handle, rocblas_pointer_mode_device));
        DAPI_CHECK(rocblas_rot_batched_fn,
                   (handle, N, nullptr, incx, nullptr, incy, nullptr, nullptr, batch_count));
        return;
    }

    // Naming: `h` is in CPU (host) memory(eg hx), `d` is in GPU (device) memory (eg dx).
    // Allocate host memory
    host_batch_vector<T> hx(N, incx, batch_count);
    host_batch_vector<T> hy(N, incy, batch_count);
    host_vector<U>       hc(1, 1);
    host_vector<V>       hs(1, 1);

    // Check host memory allocation
    CHECK_HIP_ERROR(hx.memcheck());
    CHECK_HIP_ERROR(hy.memcheck());

    // Allocate device memory
    device_batch_vector<T> dx(N, incx, batch_count);
    device_batch_vector<T> dy(N, incy, batch_count);
    device_vector<U>       dc(1, 1);
    device_vector<V>       ds(1, 1);

    // Check device memory allocation
    CHECK_DEVICE_ALLOCATION(dx.memcheck());
    CHECK_DEVICE_ALLOCATION(dy.memcheck());
    CHECK_DEVICE_ALLOCATION(dc.memcheck());
    CHECK_DEVICE_ALLOCATION(ds.memcheck());

    // Initialize data on host memory
    rocblas_init_vector(hx, arg, rocblas_client_alpha_sets_nan, true);
    rocblas_init_vector(hy, arg, rocblas_client_alpha_sets_nan, false);
    rocblas_init_vector(hc, arg, rocblas_client_alpha_sets_nan, false);
    rocblas_init_vector(hs, arg, rocblas_client_alpha_sets_nan, false);

    // CPU BLAS reference data
    host_batch_vector<T> hx_gold(N, incx, batch_count);
    host_batch_vector<T> hy_gold(N, incy, batch_count);
    hx_gold.copy_from(hx);
    hy_gold.copy_from(hy);

    if(arg.unit_check || arg.norm_check)
    {
        if(arg.pointer_mode_host)
        {
            CHECK_ROCBLAS_ERROR(rocblas_set_pointer_mode(handle, rocblas_pointer_mode_host));
            CHECK_HIP_ERROR(dx.transfer_from(hx));
            CHECK_HIP_ERROR(dy.transfer_from(hy));
            handle.pre_test(arg);
            DAPI_CHECK(rocblas_rot_batched_fn,
                       (handle,
                        N,
                        dx.ptr_on_device(),
                        incx,
                        dy.ptr_on_device(),
                        incy,
                        hc,
                        hs,
                        batch_count));
            handle.post_test(arg);

            CHECK_HIP_ERROR(hx.transfer_from(dx));
            CHECK_HIP_ERROR(hy.transfer_from(dy));
        }
        if(arg.pointer_mode_device)
        {
            CHECK_ROCBLAS_ERROR(rocblas_set_pointer_mode(handle, rocblas_pointer_mode_device));

            CHECK_HIP_ERROR(dx.transfer_from(hx_gold));
            CHECK_HIP_ERROR(dy.transfer_from(hy_gold));
            CHECK_HIP_ERROR(dc.transfer_from(hc));
            CHECK_HIP_ERROR(ds.transfer_from(hs));

            handle.pre_test(arg);
            DAPI_CHECK(rocblas_rot_batched_fn,
                       (handle,
                        N,
                        dx.ptr_on_device(),
                        incx,
                        dy.ptr_on_device(),
                        incy,
                        dc,
                        ds,
                        batch_count));
            handle.post_test(arg);

            if(arg.repeatability_check)
            {
                host_batch_vector<T> hx_copy(N, incx, batch_count);
                host_batch_vector<T> hy_copy(N, incy, batch_count);

                CHECK_HIP_ERROR(hx.transfer_from(dx));
                CHECK_HIP_ERROR(hy.transfer_from(dy));

                for(int i = 0; i < arg.iters; i++)
                {
                    CHECK_HIP_ERROR(dx.transfer_from(hx_gold));
                    CHECK_HIP_ERROR(dy.transfer_from(hy_gold));

                    DAPI_CHECK(rocblas_rot_batched_fn,
                               (handle,
                                N,
                                dx.ptr_on_device(),
                                incx,
                                dy.ptr_on_device(),
                                incy,
                                dc,
                                ds,
                                batch_count));
                    CHECK_HIP_ERROR(hx_copy.transfer_from(dx));
                    CHECK_HIP_ERROR(hy_copy.transfer_from(dy));

                    unit_check_general<T>(1, N, incx, hx, hx_copy, batch_count);
                    unit_check_general<T>(1, N, incy, hy, hy_copy, batch_count);
                }
                return;
            }
        }

        cpu_time_used = get_time_us_no_sync();
        for(size_t b = 0; b < batch_count; b++)
        {
            ref_rot<T, T, U, V>(N, hx_gold[b], incx, hy_gold[b], incy, hc, hs);
        }
        cpu_time_used = get_time_us_no_sync() - cpu_time_used;

        if(arg.pointer_mode_host)
        {
            if(arg.unit_check)
            {
                unit_check_general<T>(1, N, incx, hx_gold, hx, batch_count);
                unit_check_general<T>(1, N, incy, hy_gold, hy, batch_count);
            }
            if(arg.norm_check)
            {
                norm_error_host_x
                    = norm_check_general<T>('F', 1, N, incx, hx_gold, hx, batch_count);
                norm_error_host_y
                    = norm_check_general<T>('F', 1, N, incy, hy_gold, hy, batch_count);
            }
        }

        if(arg.pointer_mode_device)
        {
            CHECK_HIP_ERROR(hx.transfer_from(dx));
            CHECK_HIP_ERROR(hy.transfer_from(dy));

            if(arg.unit_check)
            {
                unit_check_general<T>(1, N, incx, hx_gold, hx, batch_count);
                unit_check_general<T>(1, N, incy, hy_gold, hy, batch_count);
            }
            if(arg.norm_check)
            {
                norm_error_device_x
                    = norm_check_general<T>('F', 1, N, incx, hx_gold, hx, batch_count);
                norm_error_device_y
                    = norm_check_general<T>('F', 1, N, incy, hy_gold, hy, batch_count);
            }
        }
    }

    if(arg.timing)
    {
        double gpu_time_used;
        int    number_cold_calls = arg.cold_iters;
        int    total_calls       = number_cold_calls + arg.iters;
        CHECK_ROCBLAS_ERROR(rocblas_set_pointer_mode(handle, rocblas_pointer_mode_device));
        CHECK_HIP_ERROR(dx.transfer_from(hx));
        CHECK_HIP_ERROR(dy.transfer_from(hy));
        CHECK_HIP_ERROR(dc.transfer_from(hc));
        CHECK_HIP_ERROR(ds.transfer_from(hs));

        hipStream_t stream;
        CHECK_ROCBLAS_ERROR(rocblas_get_stream(handle, &stream));
        for(int iter = 0; iter < total_calls; iter++)
        {
            if(iter == number_cold_calls)
                gpu_time_used = get_time_us_sync(stream);

            DAPI_DISPATCH(rocblas_rot_batched_fn,
                          (handle,
                           N,
                           dx.ptr_on_device(),
                           incx,
                           dy.ptr_on_device(),
                           incy,
                           dc,
                           ds,
                           batch_count));
        }

        gpu_time_used = (get_time_us_sync(stream) - gpu_time_used);

        ArgumentModel<e_N, e_incx, e_incy, e_batch_count>{}.log_args<T>(
            rocblas_cout,
            arg,
            gpu_time_used,
            rot_gflop_count<T, T, U, V>(N),
            rot_gbyte_count<T>(N),
            cpu_time_used,
            norm_error_host_x,
            norm_error_device_x,
            norm_error_host_y,
            norm_error_device_y);
    }
}
