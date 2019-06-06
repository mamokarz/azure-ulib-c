// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "ustream.h"
#include "ulib_port.h"
#include "ulib_heap.h"
#include "ulog.h"

typedef uint8_t                                 USTREAM_FLAGS;
#define USTREAM_FLAG_NONE                 (USTREAM_FLAGS)0x00
#define USTREAM_FLAG_RELEASE_ON_DESTROY   (USTREAM_FLAGS)0x01

typedef struct USTREAM_INNER_BUFFER_TAG
{
    uint8_t* ptr;
    size_t length;
    USTREAM_FLAGS flags;
    volatile uint32_t ref_count;
} USTREAM_INNER_BUFFER;

typedef struct USTREAM_INSTANCE_TAG
{
    /* Inner buffer. */
    USTREAM_INNER_BUFFER* inner_buffer;

    /* Instance controls. */
    offset_t offset_diff;
    offset_t inner_current_position;
    offset_t inner_first_valid_position;
} USTREAM_INSTANCE;

static ULIB_RESULT concrete_set_position(USTREAM* ustream_interface, offset_t position);
static ULIB_RESULT concrete_reset(USTREAM* ustream_interface);
static ULIB_RESULT concrete_read(USTREAM* ustream_interface, uint8_t* const buffer, size_t buffer_length, size_t* const size);
static ULIB_RESULT concrete_get_remaining_size(USTREAM* ustream_interface, size_t* const size);
static ULIB_RESULT concrete_get_position(USTREAM* ustream_interface, offset_t* const position);
static ULIB_RESULT concrete_release(USTREAM* ustream_interface, offset_t position);
static USTREAM* concrete_clone(USTREAM* ustream_interface, offset_t offset);
static ULIB_RESULT concrete_dispose(USTREAM* ustream_interface);
static const USTREAM_INTERFACE api =
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

static USTREAM* create_instance(
    USTREAM_INNER_BUFFER* inner_buffer,
    offset_t inner_current_position,
    offset_t offset)
{
    USTREAM* ustream_interface = (USTREAM*)ULIB_CONFIG_MALLOC(sizeof(USTREAM));
    /*[ustream_create_noMemoryToCreateInterfaceFailed]*/
    /*[ustream_const_create_noMemoryToCreateInterfaceFailed]*/
    /*[ustream_clone_noMemoryToCreateInterfaceFailed]*/
    /*[ustream_clone_noMemoryTocreate_instanceFailed]*/
    if(ustream_interface == NULL)
    {
        ULIB_CONFIG_LOG(ULOG_TYPE_ERROR, ULOG_OUT_OF_MEMORY_STRING, "ustream_interface");
    }
    else
    {
        USTREAM_INSTANCE* instance = (USTREAM_INSTANCE*)ULIB_CONFIG_MALLOC(sizeof(USTREAM_INSTANCE));
        /*[az_stdbufferClone_noMemoryTocreate_instanceFailed]*/
        if(instance != NULL)
        {
            ustream_interface->api = &api;
            ustream_interface->handle = (void*)instance;

            instance->inner_current_position = inner_current_position;
            instance->inner_first_valid_position = inner_current_position;
            instance->offset_diff = offset - inner_current_position;
            instance->inner_buffer = inner_buffer;
            ULIB_PORT_ATOMIC_INC_W(&(instance->inner_buffer->ref_count));
        }
        else
        {
            ULIB_CONFIG_LOG(ULOG_TYPE_ERROR, ULOG_OUT_OF_MEMORY_STRING, "uStreamInstance");
            ULIB_CONFIG_FREE(ustream_interface);
            ustream_interface = NULL;
        }
    }

    return ustream_interface;
}

static USTREAM_INNER_BUFFER* create_inner_buffer(
    const uint8_t* const buffer, 
    size_t buffer_length,
    bool take_ownership,
    bool release_on_destroy)
{
    uint8_t* ptr;
    USTREAM_INNER_BUFFER* inner_buffer;

    if(take_ownership)
    {
        ptr = (uint8_t*)buffer;
    }
    else
    {
        if((ptr = (uint8_t*)ULIB_CONFIG_MALLOC(buffer_length * sizeof(uint8_t))) != NULL)
        {
            (void)memcpy(ptr, buffer, buffer_length);
        }
        else
        {
            ULIB_CONFIG_LOG(ULOG_TYPE_ERROR, ULOG_OUT_OF_MEMORY_STRING, "inner buffer");
        }
    }

    if(ptr == NULL)
    {
        inner_buffer = NULL;
    }
    else if((inner_buffer = (USTREAM_INNER_BUFFER*)ULIB_CONFIG_MALLOC(sizeof(USTREAM_INNER_BUFFER))) == NULL)
    {
        ULIB_CONFIG_LOG(ULOG_TYPE_ERROR, ULOG_OUT_OF_MEMORY_STRING, "inner buffer control");
        if(!take_ownership)
        {
            ULIB_CONFIG_FREE(ptr);
        }
    }
    else
    {
        inner_buffer->ptr = ptr;
        inner_buffer->length = buffer_length;
        inner_buffer->flags = (release_on_destroy ? USTREAM_FLAG_RELEASE_ON_DESTROY : USTREAM_FLAG_NONE);
        inner_buffer->ref_count = 0;
    }

    return inner_buffer;
}

static void destroy_inner_buffer(USTREAM_INNER_BUFFER* inner_buffer)
{
    if((inner_buffer->flags & USTREAM_FLAG_RELEASE_ON_DESTROY) == USTREAM_FLAG_RELEASE_ON_DESTROY)
    {
        ULIB_CONFIG_FREE(inner_buffer->ptr);
    }
    ULIB_CONFIG_FREE(inner_buffer);
}

static ULIB_RESULT concrete_set_position(USTREAM* ustream_interface, offset_t position)
{
    ULIB_RESULT result;

    if(USTREAM_IS_NOT_TYPE_OF(ustream_interface, api))
    {
        /*[ustream_set_position_complianceNullBufferFailed]*/
        /*[ustream_set_position_complianceNonTypeOfBufferAPIFailed]*/
        ULIB_CONFIG_LOG(ULOG_TYPE_ERROR, ULOG_REQUIRE_TYPE_OF_USTREAM_STRING);
        result = ULIB_ILLEGAL_ARGUMENT_ERROR;
    }
    else
    {
        USTREAM_INSTANCE* instance = (USTREAM_INSTANCE*)ustream_interface->handle;
        USTREAM_INNER_BUFFER* inner_buffer = instance->inner_buffer;
        offset_t innerPosition = position - instance->offset_diff;

        if((innerPosition > (offset_t)(inner_buffer->length)) || 
                (innerPosition < instance->inner_first_valid_position))
        {
            /*[ustream_set_position_complianceForwardOutOfTheBufferFailed]*/
            /*[ustream_set_position_complianceBackBeforeFirstValidPositionFailed]*/
            result = ULIB_NO_SUCH_ELEMENT_ERROR;
        }
        else
        {
            /*[ustream_set_position_complianceBackToBeginningSucceed]*/
            /*[ustream_set_position_complianceBackPositionSucceed]*/
            /*[ustream_set_position_complianceForwardPositionSucceed]*/
            /*[ustream_set_position_complianceForwardToTheEndPositionSucceed]*/
            /*[ustream_set_position_complianceRunFullBufferByteByByteSucceed]*/
            /*[ustream_set_position_complianceRunFullBufferByteByByteReverseOrderSucceed]*/
            /*[ustream_set_position_complianceClonedBufferBackToBeginningSucceed]*/
            /*[ustream_set_position_complianceClonedBufferBackPositionSucceed]*/
            /*[ustream_set_position_complianceClonedBufferForwardPositionSucceed]*/
            /*[ustream_set_position_complianceClonedBufferForwardToTheEndPositionSucceed]*/
            /*[ustream_set_position_complianceClonedBufferRunFullBufferByteByByteSucceed]*/
            /*[ustream_set_position_complianceClonedBufferRunFullBufferByteByByteReverseOrderSucceed]*/
            instance->inner_current_position = innerPosition;
            result = ULIB_SUCCESS;
        }
    }

    return result;
}

static ULIB_RESULT concrete_reset(USTREAM* ustream_interface)
{
    ULIB_RESULT result;

    if(USTREAM_IS_NOT_TYPE_OF(ustream_interface, api))
    {
        /*[ustream_reset_complianceNullBufferFailed]*/
        /*[ustream_reset_complianceNonTypeOfBufferAPIFailed]*/

        result = ULIB_ILLEGAL_ARGUMENT_ERROR;
    }
    else
    {
        USTREAM_INSTANCE* instance = (USTREAM_INSTANCE*)ustream_interface->handle;

        /*[ustream_reset_complianceBackToBeginningSucceed]*/
        /*[ustream_reset_complianceBackPositionSucceed]*/
        instance->inner_current_position = instance->inner_first_valid_position;
        result = ULIB_SUCCESS;
    }

    return result;
}

static ULIB_RESULT concrete_read(
        USTREAM* ustream_interface,
        uint8_t* const buffer,
        size_t buffer_length,
        size_t* const size)
{
    ULIB_RESULT result;

    if(USTREAM_IS_NOT_TYPE_OF(ustream_interface, api))
    {
        /*[ustream_read_complianceNullBufferFailed]*/
        /*[ustream_read_complianceNonTypeOfBufferAPIFailed]*/
        ULIB_CONFIG_LOG(ULOG_TYPE_ERROR, ULOG_REQUIRE_TYPE_OF_USTREAM_STRING);
        result = ULIB_ILLEGAL_ARGUMENT_ERROR;
    }
    else if((buffer == NULL) || (size == NULL))
    {
        /*[ustream_read_complianceNullReturnBufferFailed]*/
        /*[ustream_read_complianceNullReturnSizeFailed]*/
        ULIB_CONFIG_LOG(
            ULOG_TYPE_ERROR,
            ULOG_REQUIRE_NOT_NULL_STRING, 
            (buffer == NULL ? "buffer" : "size"));
        result = ULIB_ILLEGAL_ARGUMENT_ERROR;
    }
    else if(buffer_length == 0)
    {
        /*[ustream_read_complianceBufferWithZeroSizeFailed]*/
        ULIB_CONFIG_LOG(ULOG_TYPE_ERROR, ULOG_REQUIRE_NOT_EQUALS_STRING, "buffer_length", "0");
        result = ULIB_ILLEGAL_ARGUMENT_ERROR;
    }
    else
    {
        USTREAM_INSTANCE* instance = (USTREAM_INSTANCE*)ustream_interface->handle;
        USTREAM_INNER_BUFFER* inner_buffer = instance->inner_buffer;

        if(instance->inner_current_position >= inner_buffer->length)
        {
            /*[ustream_read_complianceSucceed_3]*/
            *size = 0;
            result = ULIB_EOF;
        }
        else
        {
            /*[ustream_read_complianceSucceed_2]*/
            size_t remainSize = inner_buffer->length - (size_t)instance->inner_current_position;
            *size = (buffer_length < remainSize) ? buffer_length : remainSize;
            /*[ustream_read_complianceSucceed_1]*/
            /*[ustream_read_complianceSingleBufferSucceed]*/
            /*[ustream_read_complianceRightBoundaryConditionSucceed]*/
            /*[ustream_read_complianceBoundaryConditionSucceed]*/
            /*[ustream_read_complianceLeftBoundaryConditionSucceed]*/
            /*[ustream_read_complianceSingleByteSucceed]*/
            /*[ustream_read_complianceGetFromClonedBufferSucceed]*/
            /*[ustream_read_complianceClonedBufferRightBoundaryConditionSucceed]*/
            (void)memcpy(buffer, inner_buffer->ptr + instance->inner_current_position, *size);
            instance->inner_current_position += *size;
            result = ULIB_SUCCESS;
        }
    }

    return result;
}

static ULIB_RESULT concrete_get_remaining_size(USTREAM* ustream_interface, size_t* const size)
{
    ULIB_RESULT result;

    if(USTREAM_IS_NOT_TYPE_OF(ustream_interface, api))
    {
        /*[ustream_get_remaining_size_complianceNullBufferFailed]*/
        /*[ustream_get_remaining_size_complianceBufferIsNotTypeOfBufferFailed]*/
        ULIB_CONFIG_LOG(ULOG_TYPE_ERROR, ULOG_REQUIRE_TYPE_OF_USTREAM_STRING);
        result = ULIB_ILLEGAL_ARGUMENT_ERROR;
    }
    else if(size == NULL)
    {
        /*[ustream_get_remaining_size_complianceNullSizeFailed]*/
        ULIB_CONFIG_LOG(ULOG_TYPE_ERROR, ULOG_REQUIRE_NOT_NULL_STRING, "size");
        result = ULIB_ILLEGAL_ARGUMENT_ERROR;
    }
    else
    {
        USTREAM_INSTANCE* instance = (USTREAM_INSTANCE*)ustream_interface->handle;

        /*[ustream_get_remaining_size_complianceNewBufferSucceed]*/
        /*[ustream_get_remaining_size_complianceNewBufferWithNonZeroCurrentPositionSucceed]*/
        /*[ustream_get_remaining_size_complianceClonedBufferWithNonZeroCurrentPositionSucceed]*/
        *size = instance->inner_buffer->length - instance->inner_current_position;
        result = ULIB_SUCCESS;
    }

    return result;
}

static ULIB_RESULT concrete_get_position(USTREAM* ustream_interface, offset_t* const position)
{
    ULIB_RESULT result;

    if(USTREAM_IS_NOT_TYPE_OF(ustream_interface, api))
    {
        /*[ustream_get_position_complianceNullBufferFailed]*/
        /*[ustream_get_position_complianceBufferIsNotTypeOfBufferFailed]*/
        ULIB_CONFIG_LOG(ULOG_TYPE_ERROR, ULOG_REQUIRE_TYPE_OF_USTREAM_STRING);
        result = ULIB_ILLEGAL_ARGUMENT_ERROR;
    }
    else if(position == NULL)
    {
        /*[ustream_get_position_complianceNullPositionFailed]*/
        ULIB_CONFIG_LOG(ULOG_TYPE_ERROR, ULOG_REQUIRE_NOT_NULL_STRING, "position");
        result = ULIB_ILLEGAL_ARGUMENT_ERROR;
    }
    else
    {
        USTREAM_INSTANCE* instance = (USTREAM_INSTANCE*)ustream_interface->handle;

        /*[ustream_get_position_complianceNewBufferSucceed]*/
        /*[ustream_get_position_complianceNewBufferWithNonZeroCurrentPositionSucceed]*/
        /*[ustream_get_position_complianceClonedBufferWithNonZeroCurrentPositionSucceed]*/
        *position = instance->inner_current_position + instance->offset_diff;
        result = ULIB_SUCCESS;
    }

    return result;
}

static ULIB_RESULT concrete_release(USTREAM* ustream_interface, offset_t position)
{
    ULIB_RESULT result;

    if(USTREAM_IS_NOT_TYPE_OF(ustream_interface, api))
    {
        /*[ustream_release_complianceNullBufferFailed]*/
        /*[ustream_release_complianceNonTypeOfBufferAPIFailed]*/
        ULIB_CONFIG_LOG(ULOG_TYPE_ERROR, ULOG_REQUIRE_TYPE_OF_USTREAM_STRING);
        result = ULIB_ILLEGAL_ARGUMENT_ERROR;
    }
    else
    {
        USTREAM_INSTANCE* instance = (USTREAM_INSTANCE*)ustream_interface->handle;
        offset_t innerPosition = position - instance->offset_diff;

        if((innerPosition >= instance->inner_current_position) ||
                (innerPosition < instance->inner_first_valid_position))
        {
            /*[ustream_release_complianceReleaseAfterCurrentFailed]*/
            /*[ustream_release_complianceReleasePositionAlreayReleasedFailed]*/
            result = ULIB_ILLEGAL_ARGUMENT_ERROR;
        }
        else
        {
            /*[ustream_release_complianceSucceed]*/
            /*[ustream_release_complianceReleaseAllSucceed]*/
            /*[ustream_release_complianceRunFullBufferByteByByteSucceed]*/
            /*[ustream_release_complianceClonedBufferSucceed]*/
            /*[ustream_release_complianceClonedBufferReleaseAllSucceed]*/
            /*[ustream_release_complianceClonedBufferRunFullBufferByteByByteSucceed]*/
            instance->inner_first_valid_position = innerPosition + (offset_t)1;
            result = ULIB_SUCCESS;
        }
    }

    return result;
}

static USTREAM* concrete_clone(USTREAM* ustream_interface, offset_t offset)
{
    USTREAM* interfaceResult;

    if(USTREAM_IS_NOT_TYPE_OF(ustream_interface, api))
    {
        /*[ustream_clone_complianceNullBufferFailed]*/
        /*[ustream_clone_complianceBufferIsNotTypeOfBufferFailed]*/
        ULIB_CONFIG_LOG(ULOG_TYPE_ERROR, ULOG_REQUIRE_TYPE_OF_USTREAM_STRING);
        interfaceResult = NULL;
    }
    else
    {
        USTREAM_INSTANCE* instance = (USTREAM_INSTANCE*)ustream_interface->handle;

        if(offset > (UINT32_MAX - instance->inner_buffer->length))
        {
            /*[ustream_clone_complianceOffsetExceedSizeFailed]*/
            interfaceResult = NULL;
        }
        else
        {
            /*[ustream_clone_complianceNewBufferClonedWithZeroOffsetSucceed]*/
            /*[ustream_clone_complianceNewBufferClonedWithOffsetSucceed]*/
            /*[ustream_clone_complianceNewBufferWithNonZeroCurrentAndReleasedPositionsClonedWithOffsetSucceed]*/
            /*[ustream_clone_complianceNewBufferWithNonZeroCurrentAndReleasedPositionsClonedWithNegativeOffsetSucceed]*/
            /*[ustream_clone_complianceClonedBufferWithNonZeroCurrentAndReleasedPositionsClonedWithOffsetSucceed]*/
            /*[ustream_clone_complianceNoMemoryTocreate_instanceFailed]*/
            /*[ustream_clone_complianceEmptyBufferSucceed]*/
            interfaceResult = create_instance(instance->inner_buffer, instance->inner_current_position, offset);
        }
    }

    return interfaceResult;
}

static ULIB_RESULT concrete_dispose(USTREAM* ustream_interface)
{
    ULIB_RESULT result;

    if(USTREAM_IS_NOT_TYPE_OF(ustream_interface, api))
    {
        /*[ustream_dispose_complianceNullBufferFailed]*/
        /*[ustream_dispose_complianceBufferIsNotTypeOfBufferFailed]*/
        ULIB_CONFIG_LOG(ULOG_TYPE_ERROR, ULOG_REQUIRE_TYPE_OF_USTREAM_STRING);
        result = ULIB_ILLEGAL_ARGUMENT_ERROR;
    }
    else
    {
        USTREAM_INSTANCE* instance = (USTREAM_INSTANCE*)ustream_interface->handle;
        USTREAM_INNER_BUFFER* inner_buffer = instance->inner_buffer;

        /*[ustream_dispose_complianceClonedInstanceDisposedFirstSucceed]*/
        /*[ustream_dispose_complianceClonedInstanceDisposedSecondSucceed]*/
        /*[ustream_dispose_complianceSingleInstanceSucceed]*/
        ULIB_PORT_ATOMIC_DEC_W(&(inner_buffer->ref_count));
        if(inner_buffer->ref_count == 0)
        {
            destroy_inner_buffer(inner_buffer);
        }
        ULIB_CONFIG_FREE(instance);
        ULIB_CONFIG_FREE(ustream_interface);
        result = ULIB_SUCCESS;
    }

    return result;
}

USTREAM* ustream_create(
        const uint8_t* const buffer, 
        size_t buffer_length,
        bool take_ownership)
{
    USTREAM* interfaceResult;

    if(buffer == NULL)
    {
        /*[ustream_create_NULLBufferFailed]*/
        ULIB_CONFIG_LOG(ULOG_TYPE_ERROR, ULOG_REQUIRE_NOT_NULL_STRING, "buffer");
        interfaceResult = NULL;
    }
    else if(buffer_length == 0)
    {
        /*[ustream_create_zeroLengthFailed]*/
        ULIB_CONFIG_LOG(ULOG_TYPE_ERROR, ULOG_REQUIRE_NOT_EQUALS_STRING, "buffer_length", "0");
        interfaceResult = NULL;
    }
    else
    {
        /*[ustream_create_succeed]*/
        USTREAM_INNER_BUFFER* inner_buffer = create_inner_buffer(buffer, buffer_length, take_ownership, true);
        /*[ustream_create_noMemoryToCreateProtectedBufferFailed]*/
        /*[ustream_create_noMemoryToCreateInnerBufferFailed]*/
        if(inner_buffer == NULL)
        {
            interfaceResult = NULL;
        }
        else
        {
            interfaceResult = create_instance(inner_buffer, 0, 0);
            /*[ustream_create_noMemoryTocreate_instanceFailed]*/
            if(interfaceResult == NULL)
            {
                if(take_ownership)
                {
                    ULIB_CONFIG_FREE(inner_buffer);
                }
                else
                {
                    destroy_inner_buffer(inner_buffer);
                }
            }
        }
    }

    return interfaceResult;
}

USTREAM* ustream_const_create(
    const uint8_t* const buffer,
    size_t buffer_length)
{
    USTREAM* interfaceResult;

    if(buffer == NULL)
    {
        /*[ustream_const_create_NULLBufferFailed]*/
        ULIB_CONFIG_LOG(ULOG_TYPE_ERROR, ULOG_REQUIRE_NOT_NULL_STRING, "buffer");
        interfaceResult = NULL;
    }
    else if(buffer_length == 0)
    {
        /*[ustream_const_create_zeroLengthFailed]*/
        ULIB_CONFIG_LOG(ULOG_TYPE_ERROR, ULOG_REQUIRE_NOT_EQUALS_STRING, "buffer_length", "0");
        interfaceResult = NULL;
    }
    else
    {
        /*[ustream_const_create_succeed]*/
        USTREAM_INNER_BUFFER* inner_buffer = create_inner_buffer(buffer, buffer_length, true, false);
        /*[ustream_const_create_noMemoryToCreateProtectedBufferFailed]*/
        /*[ustream_const_create_noMemoryToCreateInnerBufferFailed]*/
        if(inner_buffer == NULL)
        {
            interfaceResult = NULL;
        }
        else
        {
            interfaceResult = create_instance(inner_buffer, 0, 0);
            /*[ustream_const_create_noMemoryTocreate_instanceFailed]*/
            if(interfaceResult == NULL)
            {
                destroy_inner_buffer(inner_buffer);
            }
        }
    }

    return interfaceResult;
}