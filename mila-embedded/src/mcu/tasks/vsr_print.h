#ifndef VSR_PRINT_H
#define VSR_PRINT_H

#include "vsr/vsr_state.h"

#ifdef __cplusplus
extern "C" {
#endif

// Print one topic by name. Use "all" to print every topic.
void vsr_print_topic(const char* topic_name, const vehicle_status_reg_t* vsr);

// Print the list of supported topic names.
void vsr_print_available_topics_internal(void);

#ifdef __cplusplus
}
#endif

#endif // VSR_PRINT_H
