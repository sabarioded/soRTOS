#ifndef EVENT_GROUP_H
#define EVENT_GROUP_H

#include <stdint.h>
#include "scheduler.h"
#include "spinlock.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EVENT_WAIT_ANY      0x00  /* Wait for any specified bit */
#define EVENT_WAIT_ALL      0x01  /* Wait for all specified bits */
#define EVENT_CLEAR_ON_EXIT 0x02  /* Clear bits after waking */

/* Opaque handle to an event group */
typedef struct event_group event_group_t;

/**
 * @brief Create a new event group.
 * @return Pointer to the event group handle, or NULL on failure.
 */
event_group_t* event_group_create(void);

/**
 * @brief Delete an event group and free resources.
 * @param eg Pointer to the event group structure.
 */
void event_group_delete(event_group_t *eg);

/**
 * @brief Set event bits.
 * 
 * Sets one or more event bits in the event group. This may unblock tasks
 * waiting for these bits.
 * 
 * @param eg Pointer to the event group structure.
 * @param bits_to_set Bitmask of bits to set (1 = set).
 * @return The value of the event group bits at the time the call returned.
 */
uint32_t event_group_set_bits(event_group_t *eg, uint32_t bits_to_set);

/**
 * @brief Set event bits from an Interrupt Service Routine (ISR).
 * 
 * @param eg Pointer to the event group structure.
 * @param bits_to_set Bitmask of bits to set.
 * @return The value of the event group bits at the time the call returned.
 */
uint32_t event_group_set_bits_from_isr(event_group_t *eg, uint32_t bits_to_set);

/**
 * @brief Clear event bits.
 * 
 * @param eg Pointer to the event group structure.
 * @param bits_to_clear Bitmask of bits to clear (1 = clear).
 * @return The value of the event group bits after the bits were cleared.
 */
uint32_t event_group_clear_bits(event_group_t *eg, uint32_t bits_to_clear);

/**
 * @brief Wait for event bits to be set.
 * 
 * Blocks the current task until the specified bits are set or a timeout occurs.
 * 
 * @param eg Pointer to the event group structure.
 * @param bits_to_wait Bitmask of bits to wait for.
 * @param flags Options: EVENT_WAIT_ALL, EVENT_WAIT_ANY, EVENT_CLEAR_ON_EXIT.
 * @param timeout_ticks Maximum time to wait in ticks.
 * @return The value of the event group bits when the task was unblocked.
 */
uint32_t event_group_wait_bits(event_group_t *eg, uint32_t bits_to_wait, 
                               uint8_t flags, uint32_t timeout_ticks);

/**
 * @brief Get the current event bits.
 * 
 * @param eg Pointer to the event group structure.
 * @return Current value of the event bits.
 */
uint32_t event_group_get_bits(event_group_t *eg);

#ifdef __cplusplus
}
#endif

#endif /* EVENT_GROUP_H */
