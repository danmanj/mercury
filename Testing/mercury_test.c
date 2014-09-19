/*
 * Copyright (C) 2013 Argonne National Laboratory, Department of Energy,
 *                    UChicago Argonne, LLC and The HDF Group.
 * All rights reserved.
 *
 * The full copyright notice, including terms governing use, modification,
 * and redistribution, is contained in the COPYING file that can be
 * found at the root of the source code distribution tree.
 */

#include "mercury_test.h"
#include "na_test.h"
#include "mercury_rpc_cb.h"

#include "mercury_atomic.h"
#include "mercury_request.h"
#ifdef MERCURY_TESTING_HAS_THREAD_POOL
#include "mercury_thread_pool.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/****************/
/* Local Macros */
/****************/

/*******************/
/* Local Variables */
/*******************/
static na_class_t *hg_test_na_class_g = NULL;
static na_context_t *hg_test_na_context_g = NULL;
static hg_bool_t hg_test_is_client_g = HG_FALSE;
static na_addr_t hg_test_addr_g = NA_ADDR_NULL;
static int hg_test_rank_g = 0;

extern na_bool_t na_test_use_self_g;

#ifdef MERCURY_TESTING_HAS_THREAD_POOL
hg_thread_pool_t *hg_test_thread_pool_g = NULL;
#endif

/* test_rpc */
hg_id_t hg_test_rpc_open_id_g = 0;

/* test_bulk */
hg_id_t hg_test_bulk_write_id_g = 0;

/* test_bulk_seg */
hg_id_t hg_test_bulk_seg_write_id_g = 0;

/* test_pipeline */
hg_id_t hg_test_pipeline_write_id_g = 0;

/* test_posix */
hg_id_t hg_test_posix_open_id_g = 0;
hg_id_t hg_test_posix_write_id_g = 0;
hg_id_t hg_test_posix_read_id_g = 0;
hg_id_t hg_test_posix_close_id_g = 0;

/* test_scale */
hg_id_t hg_test_scale_open_id_g = 0;
hg_id_t hg_test_scale_write_id_g = 0;

/* test_finalize */
hg_id_t hg_test_finalize_id_g = 0;
hg_atomic_int32_t hg_test_finalizing_count_g;

/*---------------------------------------------------------------------------*/
int
HG_Test_request_progress(unsigned int timeout, void *arg)
{
    struct hg_info *hg_info = (struct hg_info *) arg;
    int ret = HG_UTIL_SUCCESS;

    if (HG_Progress(hg_info->hg_class, hg_info->context, timeout) != HG_SUCCESS)
        ret = HG_UTIL_FAIL;

    return ret;
}

/*---------------------------------------------------------------------------*/
int
HG_Test_request_trigger(unsigned int timeout, unsigned int *flag, void *arg)
{
    struct hg_info *hg_info = (struct hg_info *) arg;
    unsigned int actual_count = 0;
    int ret = HG_UTIL_SUCCESS;

    if (HG_Trigger(hg_info->hg_class, hg_info->context, timeout, 1, &actual_count) != HG_SUCCESS)
        ret = HG_UTIL_FAIL;
    *flag = (actual_count) ? HG_UTIL_TRUE : HG_UTIL_FALSE;

    return ret;
}

static hg_return_t
hg_test_finalize_rpc_cb(const struct hg_cb_info *callback_info)
{
    hg_request_object_t *request_object =
            (hg_request_object_t *) callback_info->arg;

    hg_request_complete(request_object);

    return HG_SUCCESS;
}

/*---------------------------------------------------------------------------*/
static void
hg_test_finalize_rpc(hg_class_t *hg_class)
{
    hg_return_t hg_ret;
    hg_context_t *context;
    hg_handle_t handle;
    hg_request_class_t *request_class = NULL;
    hg_request_object_t *request_object = NULL;
    struct hg_info hg_info;

    context = HG_Context_create(hg_class);

    hg_info.hg_class = hg_class;
    hg_info.context = context;

    request_class = hg_request_init(HG_Test_request_progress,
            HG_Test_request_trigger, &hg_info);
    request_object = hg_request_create(request_class);

    hg_ret = HG_Create(hg_class, context, hg_test_addr_g, hg_test_finalize_id_g,
            &handle);
    if (hg_ret != HG_SUCCESS) {
        fprintf(stderr, "Could not forward call\n");
    }

    /* Forward call to remote addr and get a new request */
    hg_ret = HG_Forward(handle, hg_test_finalize_rpc_cb, request_object, NULL);
    if (hg_ret != HG_SUCCESS) {
        fprintf(stderr, "Could not forward call\n");
    }

    hg_request_wait(request_object, HG_MAX_IDLE_TIME, NULL);

    /* Complete */
    hg_ret = HG_Destroy(handle);
    if (hg_ret != HG_SUCCESS) {
        fprintf(stderr, "Could not complete\n");
    }

    HG_Context_destroy(context);

    hg_request_destroy(request_object);
    hg_request_finalize(request_class);
}

/*---------------------------------------------------------------------------*/
static hg_return_t
hg_test_finalize_cb(hg_handle_t handle)
{
    hg_return_t ret = HG_SUCCESS;

    hg_atomic_incr32(&hg_test_finalizing_count_g);

    /* Free handle and send response back */
    ret = HG_Respond(handle, NULL, NULL, NULL);
    if (ret != HG_SUCCESS) {
        fprintf(stderr, "Could not respond\n");
        return ret;
    }

    HG_Destroy(handle);

    return ret;
}

/*---------------------------------------------------------------------------*/
static void
hg_test_register(hg_class_t *hg_class)
{
    /* test_rpc */
    hg_test_rpc_open_id_g = MERCURY_REGISTER(hg_class, "hg_test_rpc_open",
            rpc_open_in_t, rpc_open_out_t, hg_test_rpc_open_cb);

//    /* test_bulk */
//    hg_test_bulk_write_id_g = MERCURY_REGISTER(hg_class, "hg_test_bulk_write",
//            bulk_write_in_t, bulk_write_out_t, hg_test_bulk_write_cb);
//
//    /* test_bulk_seg */
//    hg_test_bulk_seg_write_id_g = MERCURY_REGISTER(hg_class,
//            "hg_test_bulk_seg_write", bulk_write_in_t, bulk_write_out_t,
//            hg_test_bulk_seg_write_cb);
//
//    /* test_pipeline */
//    hg_test_pipeline_write_id_g = MERCURY_REGISTER(hg_class,
//            "hg_test_pipeline_write", bulk_write_in_t, bulk_write_out_t,
//            hg_test_pipeline_write_cb);
//
//    /* test_posix */
//    hg_test_posix_open_id_g = MERCURY_REGISTER(hg_class, "hg_test_posix_open",
//            open_in_t, open_out_t, hg_test_posix_open_cb);
//    hg_test_posix_write_id_g = MERCURY_REGISTER(hg_class, "hg_test_posix_write",
//            write_in_t, write_out_t, hg_test_posix_write_cb);
//    hg_test_posix_read_id_g = MERCURY_REGISTER(hg_class, "hg_test_posix_read",
//            read_in_t, read_out_t, hg_test_posix_read_cb);
//    hg_test_posix_close_id_g = MERCURY_REGISTER(hg_class, "hg_test_posix_close",
//            close_in_t, close_out_t, hg_test_posix_close_cb);
//
//    /* test_scale */
//    hg_test_scale_open_id_g = MERCURY_REGISTER(hg_class, "hg_test_scale_open",
//            open_in_t, open_out_t, hg_test_scale_open_cb);
//    hg_test_scale_write_id_g = MERCURY_REGISTER(hg_class, "hg_test_scale_write",
//            write_in_t, write_out_t, hg_test_scale_write_cb);

    /* test_finalize */
    hg_test_finalize_id_g = MERCURY_REGISTER(hg_class, "hg_test_finalize",
            void, void, hg_test_finalize_cb);
}

/*---------------------------------------------------------------------------*/
hg_class_t *
HG_Test_client_init(int argc, char *argv[], na_addr_t *addr, int *rank)
{
    hg_class_t *hg_class = NULL;
    char test_addr_name[NA_TEST_MAX_ADDR_NAME];
    na_return_t na_ret;

    hg_test_na_class_g = NA_Test_client_init(argc, argv, test_addr_name,
            NA_TEST_MAX_ADDR_NAME, &hg_test_rank_g);

    hg_test_na_context_g = NA_Context_create(hg_test_na_class_g);

    hg_class = HG_Init(hg_test_na_class_g, hg_test_na_context_g, NULL);
    if (!hg_class) {
        fprintf(stderr, "Could not initialize Mercury\n");
        goto done;
    }

    if (na_test_use_self_g) {
        /* Self addr */
        NA_Addr_self(hg_test_na_class_g, &hg_test_addr_g);

        /* In case of self we need the local thread pool */
#ifdef MERCURY_TESTING_HAS_THREAD_POOL
        hg_thread_pool_init(MERCURY_TESTING_NUM_THREADS, &hg_test_thread_pool_g);
        printf("# Starting server with %d threads...\n", MERCURY_TESTING_NUM_THREADS);
#endif
    } else {
        /* Look up addr using port name info */
        na_ret = NA_Addr_lookup_wait(hg_test_na_class_g, test_addr_name, &hg_test_addr_g);
        if (na_ret != NA_SUCCESS) {
            fprintf(stderr, "Could not find addr %s\n", test_addr_name);
            goto done;
        }
    }

    /* Register routines */
    hg_test_register(hg_class);

    /* When finalize is called we need to free the addr etc */
    hg_test_is_client_g = HG_TRUE;

    if (addr) *addr = hg_test_addr_g;
    if (rank) *rank = hg_test_rank_g;

done:
    return hg_class;
}

/*---------------------------------------------------------------------------*/
hg_class_t *
HG_Test_server_init(int argc, char *argv[], char ***addr_table,
        unsigned int *addr_table_size, unsigned int *max_number_of_peers)
{
    hg_class_t *hg_class = NULL;

    hg_test_na_class_g = NA_Test_server_init(argc, argv, NA_FALSE, addr_table,
            addr_table_size, max_number_of_peers);

    hg_test_na_context_g = NA_Context_create(hg_test_na_class_g);

    /* Initalize atomic variable to finalize server */
    hg_atomic_set32(&hg_test_finalizing_count_g, 0);

#ifdef MERCURY_TESTING_HAS_THREAD_POOL
    hg_thread_pool_init(MERCURY_TESTING_NUM_THREADS, &hg_test_thread_pool_g);
    printf("# Starting server with %d threads...\n", MERCURY_TESTING_NUM_THREADS);
#endif

    hg_class = HG_Init(hg_test_na_class_g, hg_test_na_context_g, NULL);
    if (!hg_class) {
        fprintf(stderr, "Could not initialize Mercury\n");
        goto done;
    }

    /* Register test routines */
    hg_test_register(hg_class);

    /* Used by CTest Test Driver */
    printf("Waiting for client...\n");
    fflush(stdout);

done:
    return hg_class;
}

/*---------------------------------------------------------------------------*/
hg_return_t
HG_Test_finalize(hg_class_t *hg_class)
{
    hg_return_t ret = HG_SUCCESS;
    na_return_t na_ret;

    NA_Test_barrier();

    if (hg_test_is_client_g) {
        /* Terminate server */
        if (hg_test_rank_g == 0) hg_test_finalize_rpc(hg_class);

        /* Free addr id */
        na_ret = NA_Addr_free(hg_test_na_class_g, hg_test_addr_g);
        if (na_ret != NA_SUCCESS) {
            fprintf(stderr, "Could not free addr\n");
            goto done;
        }
        hg_test_addr_g = NA_ADDR_NULL;
    }

#ifdef MERCURY_TESTING_HAS_THREAD_POOL
        hg_thread_pool_destroy(hg_test_thread_pool_g);
#endif

    /* Finalize interface */
    ret = HG_Finalize(hg_class);
    if (ret != HG_SUCCESS) {
        fprintf(stderr, "Could not finalize Mercury\n");
        goto done;
    }

    na_ret = NA_Context_destroy(hg_test_na_class_g, hg_test_na_context_g);
    if (na_ret != NA_SUCCESS) {
        fprintf(stderr, "Could not destroy NA context\n");
        goto done;
    }
    hg_test_na_context_g = NULL;

    na_ret = NA_Test_finalize(hg_test_na_class_g);
    if (na_ret != NA_SUCCESS) {
        fprintf(stderr, "Could not finalize NA interface\n");
        goto done;
    }
    hg_test_na_class_g = NULL;

done:
     return ret;
}
