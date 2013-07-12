/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
