#ifndef ANDROID_SENSOR_PERTURB_H
#define ANDROID_SENSOR_PERTURB_H

#include <stdint.h>
#include <sys/types.h>

// ---------------------------------------------------------------------------

namespace android {
// ---------------------------------------------------------------------------

const uint8_t ACTION_SUPPRESS = 1;
const uint8_t ACTION_CONSTANT = 2;
const uint8_t ACTION_PERTURB = 3;
const uint8_t ACTION_DELAY = 4;
const uint8_t ACTION_PASSTHROUGH = 5;

typedef enum {GAUSSIAN = 1, UNIFORM = 2, EXPONENTIAL = 3} distribution_t;

typedef struct {
    union {
        struct {
            float constantValue;
        };
        struct {
            float mean;
            float variance;
            float high;
            float low;
            distribution_t name;
        };
        struct {
            float delay;
        };
    };
    int8_t action;
} privacy_vec_t;

class SensorPerturb {
private:
    sensors_event_t* constantData(int32_t type, sensors_event_t* buffer, size_t count, privacy_vec_t* param);
    sensors_event_t* suppressData(int32_t type, privacy_vec_t* param);
public:
    SensorPerturb();
    sensors_event_t* transformData(int32_t type, sensors_event_t* buffer, size_t count, privacy_vec_t* param);
};

}; // namespace android

#endif // ANDROID_SENSOR_PERTURB_H
