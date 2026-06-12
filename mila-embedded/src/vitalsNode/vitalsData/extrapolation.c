#include <stdint.h>
#include "../../programConstants.h"
#include "../vitalsGen/vitalsStructs.h"

static int32_t extrapolateData(const CANFrame* frame, const int data_idx, const int numPoints,
                               const int weight_sub, const int denominator);

int32_t extrap4Func(const CANFrame* frame, int data_idx) {
    // see README for constants derivations
    //surely the compiler will optimize if written like this 💀
    const int numPoints = 4;
    const int weight_subtraction = 2*(numPoints) + 4;
    const int denominator = numPoints*(numPoints-1);
    return extrapolateData(frame, data_idx, numPoints, weight_subtraction, denominator);
}

int32_t extrap8Func(const CANFrame* frame, int data_idx) {
    const int numPoints = 8;
    const int weight_subtraction = 2*(numPoints) + 4;
    const int denominator = numPoints*(numPoints-1);
    return extrapolateData(frame, data_idx, numPoints, weight_subtraction, denominator);
}

static int32_t clamping_i64_to_i32(int64_t value);

//Note: Significantly faster if pointsPerData is a power of 2. (% becomes a mask)
//Standard Ordinary Least Squares extrapolation for x_(numPoints+1), given (1,y1), ..., (numPoints, y_numPoints)
static int32_t extrapolateData(const CANFrame* frame, const int data_idx, const int numPoints,
                               const int weight_sub, const int denominator) {
    int data_buf_index = (frame->dataLocation - numPoints + pointsPerData) % pointsPerData;
    int64_t sum_weighted_y = 0;

    // see README for constants derivations
    for (int i = 1, x_weight = 6; i <= numPoints; i++, x_weight+=6) {
        const int32_t y = frame->data[data_idx][data_buf_index];
        
        // see README for eq
        const int32_t weight = x_weight - weight_sub; 
        sum_weighted_y += (int64_t)weight * y;

        data_buf_index = (data_buf_index + 1) % pointsPerData;
    }
    
    return clamping_i64_to_i32(sum_weighted_y / denominator);
}

static int32_t clamping_i64_to_i32(int64_t value) {
    if (value > INT32_MAX) {
        return INT32_MAX;
    } else if (value < INT32_MIN) {
        return INT32_MIN;
    }
    return (int32_t)value;
}
