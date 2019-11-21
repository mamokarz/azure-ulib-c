// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef AZ_ULIB_USTREAM_COMPLIANCE_E2E_H
#define AZ_ULIB_USTREAM_COMPLIANCE_E2E_H

#include "az_ulib_ustream_mock_buffer.h"
#include "az_ulib_test_thread.h"
#include "az_ulib_ctest_aux.h"

/* check for test artifacts. */
#ifndef USTREAM_COMPLIANCE_EXPECTED_CONTENT_LENGTH
#error "USTREAM_COMPLIANCE_EXPECTED_CONTENT_LENGTH not defined"
#endif

#ifndef USTREAM_COMPLIANCE_TARGET_FACTORY
#error "USTREAM_COMPLIANCE_TARGET_FACTORY not defined"
#endif

#if(USTREAM_COMPLIANCE_EXPECTED_CONTENT_LENGTH / 20 == 0)
#error "USTREAM_COMPLIANCE_EXPECTED_CONTENT_LENGTH must be at least 20 uint8_t long"
#endif

/* split the content in 4 parts. */
#define USTREAM_COMPLIANCE_LENGTH_1           (USTREAM_COMPLIANCE_EXPECTED_CONTENT_LENGTH >> 2)
#define USTREAM_COMPLIANCE_LENGTH_2           (USTREAM_COMPLIANCE_LENGTH_1 + USTREAM_COMPLIANCE_LENGTH_1)
#define USTREAM_COMPLIANCE_LENGTH_3           (USTREAM_COMPLIANCE_LENGTH_2 + USTREAM_COMPLIANCE_LENGTH_1)

/* create local buffer with enough size to handle the full content. */
#define USTREAM_COMPLIANCE_TEMP_BUFFER_LENGTH (USTREAM_COMPLIANCE_EXPECTED_CONTENT_LENGTH + 2)
#define TEST_POSITION 10
#define TEST_SIZE 10

static az_ulib_ustream* compliance_thread_one_ustream;
static az_ulib_ustream* compliance_thread_two_ustream;
static const uint8_t* const compliance_expected_content = (const uint8_t*)USTREAM_COMPLIANCE_EXPECTED_CONTENT;

static int compliance_thread_one_func(void* arg)
{
    uint8_t buf_result[USTREAM_COMPLIANCE_EXPECTED_CONTENT_LENGTH] = { 0 };
    size_t size_result;
    offset_t cur_pos;
    az_ulib_ustream_read(
        compliance_thread_one_ustream,
        buf_result,
        TEST_SIZE,
        &size_result);
    az_ulib_ustream_get_position(compliance_thread_one_ustream, &cur_pos);
    ASSERT_ARE_EQUAL(int, TEST_SIZE, cur_pos);
    ASSERT_ARE_EQUAL(int, 0, strncmp((const char*)USTREAM_COMPLIANCE_LOCAL_EXPECTED_CONTENT, (const char*)buf_result, TEST_SIZE));

    return 0;
}

static int compliance_thread_two_func(void* arg)
{
    uint8_t buf_result[USTREAM_COMPLIANCE_EXPECTED_CONTENT_LENGTH] = { 0 };
    size_t size_result;
    offset_t cur_pos;
    size_t position = USTREAM_COMPLIANCE_LENGTH_1;
    az_ulib_ustream_set_position(compliance_thread_two_ustream, TEST_POSITION);
    az_ulib_ustream_read(
        compliance_thread_two_ustream,
        buf_result,
        TEST_SIZE,
        &size_result);
    az_ulib_ustream_get_position(compliance_thread_two_ustream, &cur_pos);
    ASSERT_ARE_EQUAL(int, TEST_POSITION + TEST_SIZE, cur_pos);
    ASSERT_ARE_EQUAL(int, 0, strncmp((const char*)(USTREAM_COMPLIANCE_LOCAL_EXPECTED_CONTENT + TEST_POSITION), (const char*)buf_result, TEST_SIZE));

    return 0;
}

/*
 * Start compliance tests:
 */
TEST_FUNCTION(az_ulib_ustream_e2e_compliance_multi_read_succeed)
{
    ///arrange
    az_ulib_ustream multi_ustream;
    USTREAM_COMPLIANCE_TARGET_FACTORY(&multi_ustream);
    az_ulib_ustream concat_ustream;
    USTREAM_COMPLIANCE_TARGET_FACTORY(&concat_ustream);

    az_ulib_ustream_multi_data_cb* multi_data1 =
        (az_ulib_ustream_multi_data_cb*)malloc(sizeof(az_ulib_ustream_multi_data_cb));
    ASSERT_IS_NOT_NULL(multi_data1);

    az_ulib_result result = az_ulib_ustream_concat(&multi_ustream, &concat_ustream, multi_data1, free);
    ASSERT_ARE_EQUAL(int, result, AZ_ULIB_SUCCESS);

    az_ulib_ustream_dispose(&concat_ustream);

    //Clone the multistream
    az_ulib_ustream multibuffer_clone;
    result = az_ulib_ustream_clone(&multibuffer_clone, &multi_ustream, 0);
    ASSERT_ARE_EQUAL(int, AZ_ULIB_SUCCESS, result);

    compliance_thread_one_ustream = &multi_ustream;
    compliance_thread_two_ustream = &multibuffer_clone;

    ///act
    THREAD_HANDLE test_thread_one;
    THREAD_HANDLE test_thread_two;
    (void)test_thread_create(&test_thread_one, &compliance_thread_one_func, NULL);
    (void)test_thread_create(&test_thread_two, &compliance_thread_two_func, NULL);

    ///assert
    int res1;
    int res2;
    test_thread_join(test_thread_one, &res1);
    test_thread_join(test_thread_two, &res2);

    ///cleanup
    (void)az_ulib_ustream_dispose(&multi_ustream);
    (void)az_ulib_ustream_dispose(&multibuffer_clone);

}

TEST_FUNCTION(az_ulib_ustream_e2e_compliance_read_and_reset_succeed)
{
    ///arrange
    az_ulib_ustream ustream;
    USTREAM_COMPLIANCE_TARGET_FACTORY(&ustream);

    uint8_t buf_result[USTREAM_COMPLIANCE_EXPECTED_CONTENT_LENGTH] = { 0 };

    //act
    size_t returned_size;
    az_ulib_ustream_read(&ustream, buf_result, USTREAM_COMPLIANCE_EXPECTED_CONTENT_LENGTH, &returned_size);
    ASSERT_ARE_EQUAL(int, USTREAM_COMPLIANCE_EXPECTED_CONTENT_LENGTH, returned_size);
    ASSERT_ARE_EQUAL(int, 0, strncmp((const char*)buf_result, (const char*)USTREAM_COMPLIANCE_LOCAL_EXPECTED_CONTENT, USTREAM_COMPLIANCE_EXPECTED_CONTENT_LENGTH));

    az_ulib_ustream_reset(&ustream);

    az_ulib_ustream_read(&ustream, buf_result, USTREAM_COMPLIANCE_EXPECTED_CONTENT_LENGTH, &returned_size);
    ASSERT_ARE_EQUAL(int, 0, strncmp((const char*)buf_result, (const char*)USTREAM_COMPLIANCE_LOCAL_EXPECTED_CONTENT, USTREAM_COMPLIANCE_EXPECTED_CONTENT_LENGTH));

    az_ulib_ustream_reset(&ustream);

    az_ulib_ustream_read(&ustream, buf_result, USTREAM_COMPLIANCE_EXPECTED_CONTENT_LENGTH, &returned_size);
    ASSERT_ARE_EQUAL(int, 0, strncmp((const char*)buf_result, (const char*)USTREAM_COMPLIANCE_LOCAL_EXPECTED_CONTENT, USTREAM_COMPLIANCE_EXPECTED_CONTENT_LENGTH));

    ///cleanup
    (void)az_ulib_ustream_dispose(&ustream);

}

#endif /* AZ_ULIB_USTREAM_COMPLIANCE_E2E_H */