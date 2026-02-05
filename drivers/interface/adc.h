#ifndef ADC_H
#define ADC_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque Handle for the ADC port.
 */
typedef struct adc_context* adc_port_t;

/**
 * @brief Create and initialize an ADC port.
 * Allocates memory for the ADC context.
 * 
 * @param hal_handle Pointer to the low-level hardware handle.
 * @param config Pointer to the configuration structure (defined in adc_hal.h).
 * @return adc_port_t Handle to the initialized ADC port, or NULL on failure.
 */
adc_port_t adc_create(void *hal_handle, void *config);

/**
 * @brief Initialize an ADC context in a user-provided memory block.
 * 
 * @param memory_block Pointer to a block of memory of size adc_get_context_size().
 * @param hal_handle Pointer to the low-level hardware handle.
 * @param config Pointer to the configuration structure.
 * @return adc_port_t Handle to the initialized ADC port.
 */
adc_port_t adc_init(void *memory_block, void *hal_handle, void *config);

/**
 * @brief Destroy the ADC port and free resources.
 * @param port Handle to the ADC port.
 */
void adc_destroy(adc_port_t port);

/**
 * @brief Read a single conversion from a specific channel.
 * @param port Handle to the ADC port.
 * @param channel The ADC channel number.
 * @param value Pointer to store the result.
 * @return int 0 on success, -1 on error (timeout or invalid param).
 */
int adc_read_channel(adc_port_t port, uint32_t channel, uint16_t *value);

/**
 * @brief Get the size of the ADC context structure.
 * @return size_t Size in bytes.
 */
size_t adc_get_context_size(void);

#ifdef __cplusplus
}
#endif

#endif /* ADC_H */
