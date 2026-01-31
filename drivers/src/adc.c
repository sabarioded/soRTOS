#include "adc.h"
#include "adc_hal.h"
#include "allocator.h"
#include "utils.h"

struct adc_context {
    void *hal_handle;
};

size_t adc_get_context_size(void) {
    return sizeof(struct adc_context);
}

adc_port_t adc_init(void *memory_block, void *hal_handle, void *config) {
    if (!memory_block || !hal_handle) {
        return NULL;
    }

    adc_port_t port = (adc_port_t)memory_block;
    utils_memset(port, 0, sizeof(struct adc_context));
    
    port->hal_handle = hal_handle;

    /* Initialize hardware via HAL */
    if (adc_hal_init(port->hal_handle, config) != 0) {
        return NULL;
    }

    return port;
}

adc_port_t adc_create(void *hal_handle, void *config) {
    void *mem = allocator_malloc(sizeof(struct adc_context));
    if (!mem) {
        return NULL;
    }
    adc_port_t port = adc_init(mem, hal_handle, config);
    if (!port) {
        allocator_free(mem);
    }
    return port;
}

void adc_destroy(adc_port_t port) {
    if (port) {
        allocator_free(port);
    }
}

int adc_read_channel(adc_port_t port, uint32_t channel, uint16_t *value) {
    if (!port || !port->hal_handle || !value) return -1;
    return adc_hal_read(port->hal_handle, channel, value);
}