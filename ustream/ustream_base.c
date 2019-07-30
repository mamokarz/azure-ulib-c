// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "ustream.h"


ULIB_RESULT ustream_append(
    USTREAM* ustream_interface, 
    USTREAM* ustream_to_append)
{
    ULIB_RESULT result;

    if((ustream_to_append == NULL) || 
        (ustream_interface == NULL)) 
    {
        /*[ustream_append_null_buffer_to_add_failed]*/
        /*[ustream_append_null_interface_failed]*/
        result = ULIB_ILLEGAL_ARGUMENT_ERROR;
    }
    /*[ustream_append_starting_from_multibuffer_with_not_enough_memory_failed]*/
    else if((result = ustream_multi_append(ustream_interface, ustream_to_append)) == ULIB_ILLEGAL_ARGUMENT_ERROR)
    {
        USTREAM* new_multi_buffer = ustream_multi_create();
        if(new_multi_buffer == NULL)
        {
            /*[ustream_append_not_enough_memory_to_create_multibuffer_failed]*/
            result = ULIB_OUT_OF_MEMORY_ERROR;
        }
        else if ((result = ustream_multi_append(new_multi_buffer, ustream_interface)) != ULIB_SUCCESS)
        {
            /*[ustream_append_not_enough_memory_to_append_first_buffer_failed]*/
            ustream_dispose(new_multi_buffer);
        }
        else if ((result = ustream_multi_append(new_multi_buffer, ustream_to_append)) != ULIB_SUCCESS)
        {
            /*[ustream_append_not_enough_memory_to_append_second_buffer_failed]*/
            ustream_dispose(new_multi_buffer);
        }
        else
        {
            /*[ustream_append_start_from_empty_multibuffer_succeed]*/
            /*[ustream_append_append_multiple_buffers_succeed]*/
            USTREAM aux;
            aux.api = ustream_interface->api;
            aux.handle = ustream_interface->handle;

            ustream_interface->api = new_multi_buffer->api;
            ustream_interface->handle = new_multi_buffer->handle;

            new_multi_buffer->api = aux.api;
            new_multi_buffer->handle = aux.handle;

            ustream_dispose(new_multi_buffer);
        }
    }

    return result;
}
