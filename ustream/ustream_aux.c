// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ucontract.h"
#include "ustream.h"
#include "ulib_result.h"
#include "ulib_heap.h"
#include "az_pal_os.h"
#include "az_pal_os_api.h"
#include "ulog.h"

static AZIOT_ULIB_RESULT concrete_set_position(AZIOT_USTREAM* ustream_interface, offset_t position);
static AZIOT_ULIB_RESULT concrete_reset(AZIOT_USTREAM* ustream_interface);
static AZIOT_ULIB_RESULT concrete_read(AZIOT_USTREAM* ustream_interface, uint8_t* const buffer, size_t buffer_length, size_t* const size);
static AZIOT_ULIB_RESULT concrete_get_remaining_size(AZIOT_USTREAM* ustream_interface, size_t* const size);
static AZIOT_ULIB_RESULT concrete_get_position(AZIOT_USTREAM* ustream_interface, offset_t* const position);
static AZIOT_ULIB_RESULT concrete_release(AZIOT_USTREAM* ustream_interface, offset_t position);
static AZIOT_ULIB_RESULT concrete_clone(AZIOT_USTREAM* ustream_interface_clone, AZIOT_USTREAM* ustream_interface, offset_t offset);
static AZIOT_ULIB_RESULT concrete_dispose(AZIOT_USTREAM* ustream_interface);
static const AZIOT_USTREAM_INTERFACE api =
{
        concrete_set_position,
        concrete_reset,
        concrete_read,
        concrete_get_remaining_size,
        concrete_get_position,
        concrete_release,
        concrete_clone,
        concrete_dispose
};

static void destroy_instance(AZIOT_USTREAM* ustream_interface)
{
    AZIOT_USTREAM_MULTI_DATA* multidata = (AZIOT_USTREAM_MULTI_DATA*)ustream_interface->inner_buffer->ptr;
    az_pal_os_lock_deinit(&multidata->lock);

    if(ustream_interface->inner_buffer->data_release != NULL)
    {
        ustream_interface->inner_buffer->data_release(ustream_interface->inner_buffer->ptr);
    }
    if(ustream_interface->inner_buffer->inner_buffer_release != NULL)
    {
        ustream_interface->inner_buffer->inner_buffer_release(ustream_interface->inner_buffer);
    }
}

static AZIOT_ULIB_RESULT concrete_set_position(
        AZIOT_USTREAM* ustream_interface, 
        offset_t position)
{
    /*[aziot_ustream_set_position_compliance_null_buffer_failed]*/
    /*[aziot_ustream_set_position_compliance_non_type_of_buffer_api_failed]*/
    AZIOT_UCONTRACT(AZIOT_UCONTRACT_REQUIRE(!AZIOT_USTREAM_IS_NOT_TYPE_OF(ustream_interface, api),
                                            AZIOT_ULIB_ILLEGAL_ARGUMENT_ERROR, "Passed uStream is not the correct type\r\n"),
                    AZIOT_UCONTRACT_REQUIRE_NOT_NULL(ustream_interface, AZIOT_ULIB_ILLEGAL_ARGUMENT_ERROR));
    AZIOT_ULIB_RESULT result;

    offset_t inner_position = position - ustream_interface->offset_diff;

    /*[aziot_ustream_set_position_compliance_forward_out_of_the_buffer_failed]*/
    /*[aziot_ustream_set_position_compliance_back_before_first_valid_position_failed]*/
    /*[aziot_ustream_set_position_compliance_back_before_first_valid_position_with_offset_failed]*/
    if((inner_position > (offset_t)(ustream_interface->length)) ||
       (inner_position < ustream_interface->inner_first_valid_position))
    {
        result = AZIOT_ULIB_NO_SUCH_ELEMENT_ERROR;
    }
    else
    {
        /*[aziot_ustream_set_position_compliance_back_to_beginning_succeed]*/
        /*[aziot_ustream_set_position_compliance_back_position_succeed]*/
        /*[aziot_ustream_set_position_compliance_forward_position_succeed]*/
        /*[aziot_ustream_set_position_compliance_forward_to_the_end_position_succeed]*/
        /*[aziot_ustream_set_position_compliance_run_full_buffer_byte_by_byte_succeed]*/
        /*[aziot_ustream_set_position_compliance_run_full_buffer_byte_by_byte_reverse_order_succeed]*/
        /*[aziot_ustream_set_position_compliance_cloned_buffer_back_to_beginning_succeed]*/
        /*[aziot_ustream_set_position_compliance_cloned_buffer_back_position_succeed]*/
        /*[aziot_ustream_set_position_compliance_cloned_buffer_forward_position_succeed]*/
        /*[aziot_ustream_set_position_compliance_cloned_buffer_forward_to_the_end_position_succeed]*/
        /*[aziot_ustream_set_position_compliance_cloned_buffer_run_full_buffer_byte_by_byte_succeed]*/
        /*[aziot_ustream_set_position_compliance_cloned_buffer_run_full_buffer_byte_by_byte_reverse_order_succeed]*/
        ustream_interface->inner_current_position = inner_position;
        result = AZIOT_ULIB_SUCCESS;
    }
    return result;
}

static AZIOT_ULIB_RESULT concrete_reset(AZIOT_USTREAM* ustream_interface)
{
    /*[aziot_ustream_reset_compliance_null_buffer_failed]*/
    /*[aziot_ustream_reset_compliance_non_type_of_buffer_api_failed]*/
    AZIOT_UCONTRACT(AZIOT_UCONTRACT_REQUIRE(!AZIOT_USTREAM_IS_NOT_TYPE_OF(ustream_interface, api),
                                            AZIOT_ULIB_ILLEGAL_ARGUMENT_ERROR, "Passed uStream is not the correct type\r\n"));

    /*[aziot_ustream_reset_compliance_back_to_beginning_succeed]*/
    /*[aziot_ustream_reset_compliance_back_position_succeed]*/
    /*[aziot_ustream_reset_compliance_cloned_buffer_succeed]*/
    ustream_interface->inner_current_position = ustream_interface->inner_first_valid_position;
    return AZIOT_ULIB_SUCCESS;
}

static AZIOT_ULIB_RESULT concrete_read(
        AZIOT_USTREAM* ustream_interface,
        uint8_t* const buffer,
        size_t buffer_length,
        size_t* const size)
{
    /*[aziot_ustream_read_compliance_null_buffer_failed]*/
    /*[aziot_ustream_read_compliance_non_type_of_buffer_api_failed]*/
    /*[aziot_ustream_read_compliance_null_return_buffer_failed]*/
    /*[aziot_ustream_read_compliance_null_return_size_failed]*/
    /*[aziot_ustream_read_compliance_buffer_with_zero_size_failed]*/
    AZIOT_UCONTRACT(AZIOT_UCONTRACT_REQUIRE(!AZIOT_USTREAM_IS_NOT_TYPE_OF(ustream_interface, api),
                                            AZIOT_ULIB_ILLEGAL_ARGUMENT_ERROR, "Passed uStream is not the correct type\r\n"),
                    AZIOT_UCONTRACT_REQUIRE_NOT_NULL(buffer, AZIOT_ULIB_ILLEGAL_ARGUMENT_ERROR),
                    AZIOT_UCONTRACT_REQUIRE_NOT_EQUALS(buffer_length, 0, AZIOT_ULIB_ILLEGAL_ARGUMENT_ERROR),
                    AZIOT_UCONTRACT_REQUIRE_NOT_NULL(size, AZIOT_ULIB_ILLEGAL_ARGUMENT_ERROR));

    AZIOT_ULIB_RESULT result;

    AZIOT_USTREAM_MULTI_DATA* multi_data = (AZIOT_USTREAM_MULTI_DATA*)ustream_interface->inner_buffer->ptr;
    AZIOT_USTREAM* current_ustream = (ustream_interface->inner_current_position < multi_data->ustream_one.length) ?
                                                                &multi_data->ustream_one : &multi_data->ustream_two;

    /*[aziot_ustream_read_compliance_single_buffer_succeed]*/
    /*[aziot_ustream_read_compliance_right_boundary_condition_succeed]*/
    /*[aziot_ustream_read_compliance_boundary_condition_succeed]*/
    /*[aziot_ustream_read_compliance_left_boundary_condition_succeed]*/
    /*[aziot_ustream_read_compliance_single_byte_succeed]*/
    /*[aziot_ustream_read_compliance_get_from_cloned_buffer_succeed]*/
    /*[aziot_ustream_read_compliance_cloned_buffer_right_boundary_condition_succeed]*/
    *size = 0;
    AZIOT_ULIB_RESULT intermediate_result = AZIOT_ULIB_SUCCESS;
    while((intermediate_result == AZIOT_ULIB_SUCCESS) &&
            (*size < buffer_length) &&
            (current_ustream != NULL))
    {
        size_t copied_size;
        size_t remain_size = buffer_length - *size;

        //Critical section to make sure another instance doesn't set_position before this one reads
        az_pal_os_lock_acquire(&multi_data->lock);
        /*[ustream_multi_read_clone_and_original_in_parallel_succeed]*/
        aziot_ustream_set_position(current_ustream, ustream_interface->inner_current_position + *size);
        intermediate_result = aziot_ustream_read(current_ustream, &buffer[*size], remain_size, &copied_size);
        az_pal_os_lock_release(&multi_data->lock);

        switch(intermediate_result)
        {
        case AZIOT_ULIB_SUCCESS:
            *size += copied_size;
        case AZIOT_ULIB_EOF:
            if(*size < buffer_length)
            {
                if(current_ustream == &multi_data->ustream_one)
                {
                    current_ustream = &multi_data->ustream_two;
                    intermediate_result = AZIOT_ULIB_SUCCESS;
                }
                else
                {
                    current_ustream = NULL;
                }
            }
            break;
        default:
            /*[ustream_multi_read_inner_buffer_failed_in_read_with_some_valid_content_succeed]*/
            break;
        }
    }

    if(*size != 0)
    {
        /*[aziot_ustream_concat_read_from_multiple_buffers_succeed]*/
        ustream_interface->inner_current_position += *size;
        result = AZIOT_ULIB_SUCCESS;
    }
    else
    {
        /*[ustream_multi_read_inner_buffer_failed_in_read_failed]*/
        result = intermediate_result;
    }

    return result;
}

static AZIOT_ULIB_RESULT concrete_get_remaining_size(AZIOT_USTREAM* ustream_interface, size_t* const size)
{
    /*[aziot_ustream_get_remaining_size_compliance_null_buffer_failed]*/
    /*[aziot_ustream_get_remaining_size_compliance_buffer_is_not_type_of_buffer_failed]*/
    /*[aziot_ustream_get_remaining_size_compliance_null_size_failed]*/
    AZIOT_UCONTRACT(AZIOT_UCONTRACT_REQUIRE(!AZIOT_USTREAM_IS_NOT_TYPE_OF(ustream_interface, api),
                                            AZIOT_ULIB_ILLEGAL_ARGUMENT_ERROR, "Passed uStream is not the correct type\r\n"),
                    AZIOT_UCONTRACT_REQUIRE_NOT_NULL(size, AZIOT_ULIB_ILLEGAL_ARGUMENT_ERROR));

    /*[aziot_ustream_get_remaining_size_compliance_new_buffer_succeed]*/
    /*[aziot_ustream_get_remaining_size_compliance_new_buffer_with_non_zero_current_position_succeed]*/
    /*[aziot_ustream_get_remaining_size_compliance_cloned_buffer_with_non_zero_current_position_succeed]*/
    *size = ustream_interface->length - ustream_interface->inner_current_position;

    return AZIOT_ULIB_SUCCESS;
}

static AZIOT_ULIB_RESULT concrete_get_position(AZIOT_USTREAM* ustream_interface, offset_t* const position)
{
    /*[ustream_get_current_position_compliance_null_buffer_failed]*/
    /*[ustream_get_current_position_compliance_buffer_is_not_type_of_buffer_failed]*/
    /*[ustream_get_current_position_compliance_null_position_failed]*/
    AZIOT_UCONTRACT(AZIOT_UCONTRACT_REQUIRE(!AZIOT_USTREAM_IS_NOT_TYPE_OF(ustream_interface, api),
                                            AZIOT_ULIB_ILLEGAL_ARGUMENT_ERROR, "Passed uStream is not the correct type\r\n"),
                    AZIOT_UCONTRACT_REQUIRE_NOT_NULL(position, AZIOT_ULIB_ILLEGAL_ARGUMENT_ERROR));

    /*[ustream_get_current_position_compliance_new_buffer_succeed]*/
    /*[ustream_get_current_position_compliance_new_buffer_with_non_zero_current_position_succeed]*/
    /*[ustream_get_current_position_compliance_cloned_buffer_with_non_zero_current_position_succeed]*/
    *position = ustream_interface->inner_current_position + ustream_interface->offset_diff;

    return AZIOT_ULIB_SUCCESS;
}

static AZIOT_ULIB_RESULT concrete_release(AZIOT_USTREAM* ustream_interface, offset_t position)
{
    /*[aziot_ustream_release_compliance_null_buffer_failed]*/
    /*[aziot_ustream_release_compliance_non_type_of_buffer_api_failed]*/
    AZIOT_UCONTRACT(AZIOT_UCONTRACT_REQUIRE(!AZIOT_USTREAM_IS_NOT_TYPE_OF(ustream_interface, api),
                                            AZIOT_ULIB_ILLEGAL_ARGUMENT_ERROR, "Passed uStream is not the correct type\r\n"));
    AZIOT_ULIB_RESULT result;

    AZIOT_USTREAM_MULTI_DATA* multi_data = (AZIOT_USTREAM_MULTI_DATA*)ustream_interface->inner_buffer->ptr;
    offset_t release_position = position - ustream_interface->offset_diff;

    /*[aziot_ustream_release_compliance_release_after_current_failed]*/
    /*[aziot_ustream_release_compliance_release_position_already_released_failed]*/
    if((release_position >= ustream_interface->inner_current_position) ||
            (release_position < ustream_interface->inner_first_valid_position))
    {
        result = AZIOT_ULIB_ILLEGAL_ARGUMENT_ERROR;
    }
    else
    {
        /*[aziot_ustream_release_compliance_succeed]*/
        /*[aziot_ustream_release_compliance_release_all_succeed]*/
        /*[aziot_ustream_release_compliance_run_full_buffer_byte_by_byte_succeed]*/
        /*[aziot_ustream_release_compliance_cloned_buffer_succeed]*/
        /*[aziot_ustream_release_compliance_cloned_buffer_release_all_succeed]*/
        /*[aziot_ustream_release_compliance_cloned_buffer_run_full_buffer_byte_by_byte_succeed]*/
        ustream_interface->inner_first_valid_position = release_position + (offset_t)1;
        result = AZIOT_ULIB_SUCCESS;
    }

    return result;
}

static AZIOT_ULIB_RESULT concrete_clone(AZIOT_USTREAM* ustream_interface_clone, AZIOT_USTREAM* ustream_interface, offset_t offset)
{
    /*[aziot_ustream_clone_compliance_null_buffer_failed]*/
    /*[aziot_ustream_clone_compliance_buffer_is_not_type_of_buffer_failed]*/
    /*[aziot_ustream_clone_compliance_null_buffer_clone_failed]*/
    /*[aziot_ustream_clone_compliance_offset_exceed_size_failed]*/
    AZIOT_UCONTRACT(AZIOT_UCONTRACT_REQUIRE(!AZIOT_USTREAM_IS_NOT_TYPE_OF(ustream_interface, api),
                                            AZIOT_ULIB_ILLEGAL_ARGUMENT_ERROR, "Passed uStream is not the correct type\r\n"),
                    AZIOT_UCONTRACT_REQUIRE_NOT_NULL(ustream_interface_clone, AZIOT_ULIB_ILLEGAL_ARGUMENT_ERROR),
                    AZIOT_UCONTRACT_REQUIRE((offset <= (UINT32_MAX - ustream_interface->length)), AZIOT_ULIB_ILLEGAL_ARGUMENT_ERROR, "offset exceeds max size"));

    /*[aziot_ustream_clone_compliance_new_buffer_cloned_with_zero_offset_succeed]*/
    /*[aziot_ustream_clone_compliance_new_buffer_cloned_with_offset_succeed]*/
    /*[aziot_ustream_clone_compliance_new_buffer_with_non_zero_current_and_released_positions_cloned_with_offset_succeed]*/
    /*[aziot_ustream_clone_compliance_new_buffer_with_non_zero_current_and_released_positions_cloned_with_negative_offset_succeed]*/
    /*[aziot_ustream_clone_compliance_cloned_buffer_with_non_zero_current_and_released_positions_cloned_with_offset_succeed]*/
    /*[aziot_ustream_clone_compliance_no_memory_to_create_instance_failed]*/
    /*[aziot_ustream_clone_compliance_empty_buffer_succeed]*/
    ustream_interface_clone->inner_current_position = ustream_interface->inner_current_position;
    ustream_interface_clone->inner_first_valid_position = ustream_interface->inner_current_position;
    ustream_interface_clone->offset_diff = offset - ustream_interface->inner_current_position;
    ustream_interface_clone->inner_buffer = ustream_interface->inner_buffer;
    ustream_interface_clone->length = ustream_interface->length;

    AZIOT_ULIB_PORT_ATOMIC_INC_W(&(ustream_interface->inner_buffer->ref_count));

    AZIOT_USTREAM_MULTI_DATA* multi_data = (AZIOT_USTREAM_MULTI_DATA*)ustream_interface->inner_buffer->ptr;
    AZIOT_ULIB_PORT_ATOMIC_INC_W(&(multi_data->ustream_one_ref_count));
    AZIOT_ULIB_PORT_ATOMIC_INC_W(&(multi_data->ustream_two_ref_count));

    return AZIOT_ULIB_SUCCESS;
}

static AZIOT_ULIB_RESULT concrete_dispose(AZIOT_USTREAM* ustream_interface)
{
    /*[aziot_ustream_dispose_compliance_null_buffer_failed]*/
    /*[aziot_ustream_dispose_compliance_buffer_is_not_type_of_buffer_failed]*/
    AZIOT_UCONTRACT(AZIOT_UCONTRACT_REQUIRE(!AZIOT_USTREAM_IS_NOT_TYPE_OF(ustream_interface, api),
                                            AZIOT_ULIB_ILLEGAL_ARGUMENT_ERROR, "Passed uStream is not the correct type\r\n"));

    /*[aziot_ustream_dispose_compliance_cloned_instance_disposed_first_succeed]*/
    /*[aziot_ustream_dispose_compliance_cloned_instance_disposed_second_succeed]*/
    /*[aziot_ustream_dispose_compliance_single_instance_succeed]*/
    AZIOT_USTREAM_MULTI_DATA* multi_data = (AZIOT_USTREAM_MULTI_DATA*)ustream_interface->inner_buffer->ptr;
    AZIOT_ULIB_PORT_ATOMIC_DEC_W(&(multi_data->ustream_one_ref_count));
    AZIOT_ULIB_PORT_ATOMIC_DEC_W(&(multi_data->ustream_two_ref_count));
    /*[ustream_multi_dispose_multibuffer_with_buffers_free_all_resources_succeed]*/
    if(multi_data->ustream_one_ref_count == 0 && multi_data->ustream_one.inner_buffer != NULL)
    {
        aziot_ustream_dispose(&(multi_data->ustream_one));
    }
    if(multi_data->ustream_two_ref_count == 0 && multi_data->ustream_two.inner_buffer != NULL)
    {
        aziot_ustream_dispose(&(multi_data->ustream_two));
    }

    AZIOT_USTREAM_INNER_BUFFER* inner_buffer = ustream_interface->inner_buffer;

    AZIOT_ULIB_PORT_ATOMIC_DEC_W(&(inner_buffer->ref_count));
    if(inner_buffer->ref_count == 0)
    {
        destroy_instance(ustream_interface);
    }

    return AZIOT_ULIB_SUCCESS;
}

static void aziot_ustream_multi_init(AZIOT_USTREAM* ustream_interface,
        AZIOT_USTREAM_INNER_BUFFER* inner_buffer, AZIOT_USTREAM_INNER_BUFFER_RELEASE inner_buffer_release,
        AZIOT_USTREAM_MULTI_DATA* multi_data, AZIOT_USTREAM_DATA_RELEASE multi_data_release)
{
    multi_data->ustream_one.inner_buffer = ustream_interface->inner_buffer;
    multi_data->ustream_one.inner_current_position = ustream_interface->inner_current_position;
    multi_data->ustream_one.inner_first_valid_position = ustream_interface->inner_first_valid_position;
    multi_data->ustream_one.length = ustream_interface->length;
    multi_data->ustream_one.offset_diff = ustream_interface->offset_diff;
    multi_data->ustream_one_ref_count = 1;

    az_pal_os_lock_init(&multi_data->lock);

    multi_data->ustream_two.inner_buffer = NULL;
    multi_data->ustream_two.inner_current_position = 0;
    multi_data->ustream_two.inner_first_valid_position = 0;
    multi_data->ustream_two.length = 0;
    multi_data->ustream_two.offset_diff = 0;
    multi_data->ustream_two_ref_count = 0;

    inner_buffer->api = &api;
    inner_buffer->ptr = (void*)multi_data;
    inner_buffer->ref_count = 1;
    inner_buffer->inner_buffer_release = inner_buffer_release;
    inner_buffer->data_release = multi_data_release;

    ustream_interface->inner_buffer = inner_buffer;
}

AZIOT_ULIB_RESULT aziot_ustream_concat(
    AZIOT_USTREAM* ustream_interface,
    AZIOT_USTREAM* ustream_to_append,
    AZIOT_USTREAM_INNER_BUFFER* inner_buffer,
    AZIOT_USTREAM_INNER_BUFFER_RELEASE inner_buffer_release,
    AZIOT_USTREAM_MULTI_DATA* multi_data,
    AZIOT_USTREAM_DATA_RELEASE multi_data_release)
{
    /*[aziot_ustream_concat_null_buffer_to_add_failed]*/
    /*[aziot_ustream_concat_null_interface_failed]*/
    /*[aziot_ustream_concat_null_inner_buffer_failed]*/
    /*[aziot_ustream_concat_null_multi_data_failed]*/
    AZIOT_UCONTRACT(AZIOT_UCONTRACT_REQUIRE_NOT_NULL(ustream_interface, AZIOT_ULIB_ILLEGAL_ARGUMENT_ERROR),
                    AZIOT_UCONTRACT_REQUIRE_NOT_NULL(ustream_to_append, AZIOT_ULIB_ILLEGAL_ARGUMENT_ERROR),
                    AZIOT_UCONTRACT_REQUIRE_NOT_NULL(inner_buffer, AZIOT_ULIB_ILLEGAL_ARGUMENT_ERROR),
                    AZIOT_UCONTRACT_REQUIRE_NOT_NULL(multi_data, AZIOT_ULIB_ILLEGAL_ARGUMENT_ERROR));

    AZIOT_ULIB_RESULT result;

    /*[aziot_ustream_multi_init_succeed]*/
    aziot_ustream_multi_init(ustream_interface, inner_buffer, inner_buffer_release, multi_data, multi_data_release);
    /*[aziot_ustream_concat_append_multiple_buffers_succeed]*/
    if((result = aziot_ustream_clone(&multi_data->ustream_two, ustream_to_append, ustream_interface->length)) == AZIOT_ULIB_SUCCESS)
    {
        size_t remaining_size;
        if((result = aziot_ustream_get_remaining_size(&(multi_data->ustream_two), &remaining_size)) == AZIOT_ULIB_SUCCESS)
        {
            ustream_interface->length += remaining_size;
            AZIOT_ULIB_PORT_ATOMIC_INC_W(&(multi_data->ustream_two_ref_count));
        }
        else
        {
            /*[aziot_ustream_concat_new_inner_buffer_failed_on_get_remaining_size_failed]*/
            aziot_ustream_dispose(&(multi_data->ustream_two));
        }
    }

    return result;
}