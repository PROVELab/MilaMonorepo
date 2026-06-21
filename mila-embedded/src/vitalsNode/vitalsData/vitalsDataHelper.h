#ifndef VITALS_DATA_HELPER
#define VITALS_DATA_HELPER
#include <stdbool.h>
#include "freertos/timers.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "../../pecan/pecan.h"
#include "../vitalsGen/vitalsStructs.h"
#include "../vitalsGen/vitalsPacketSendLUT.h"

void vTimerCallback(TimerHandle_t xTimer);

//****** Data Modifier  ***********//
extern QueueHandle_t dataModifierQueue; //queue to send new stuff on
// Enum for the event types
typedef enum {
    ADD_FRAME,
    MISSING_FRAME,
    ATTEMPT_ENABLE_CONTACTORS,
    UPDATE_TELEMETRY_DIVIDER
} DataModifierEventType;

typedef struct {
    CANFrame* frame;            //which frame the update is for
    uint32_t canFrameNumber;    //the index of this frame
    CANPacket packet;           //the received data
} dataUpdateRequest;

typedef struct {
    CANFrame* frame;
    uint32_t newDivider;
} telemDividerRequest;


typedef union {
    dataUpdateRequest request;
    telemDividerRequest dividerRequest;
} DataModifierEventPayload;

// Struct to hold the event payload
typedef struct {
    DataModifierEventType eventType;
    DataModifierEventPayload payload;
} DataModifierEvent;

void initializeDataModifier(const UBaseType_t priority);
void vTimerCallback(TimerHandle_t xTimer); // callback for CanFrame Timeouts


//****** Extrapolation  ***********//

int32_t extrap4Func(const CANFrame* frame, int data_idx);
int32_t extrap8Func(const CANFrame* frame, int data_idx);

//****** Bounds Checking  ***********//
void get_outlier_bounds(const int32_t* arr, int32_t* out_lower_bound, int32_t* out_upper_bound);
bool is_outlier(const int32_t* arr, int32_t new_value);
bool critical_out_of_bounds(dataPoint* dp, senddataWarningArgs* args, int32_t value);
bool mightBeCritical(dataPoint* dp);
void evaluate_bounds_and_send_warning(dataPoint* dp, CANFrame* frame, int data_idx, int32_t current_value);
void processNonCriticalPoint(dataPoint* dp, senddataWarningArgs* args, int32_t value);

#endif // VITALS_DATA_HELPER
