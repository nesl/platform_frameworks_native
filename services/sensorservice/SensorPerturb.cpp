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

#include <stdint.h>
#include <math.h>
#include <sys/types.h>
#include <hardware/sensors.h>
#include <utils/Log.h>

#include "SensorPerturb.h"

namespace android {
// ---------------------------------------------------------------------------

SensorPerturb::SensorPerturb()
{
}

sensors_event_t* SensorPerturb::constantData(
        int32_t type, sensors_event_t* buffer, size_t count, privacy_vec_t* param)
{
    size_t i = 0, j = 0;
    ALOGD("type = %d",type);

    if((type == SENSOR_TYPE_ACCELEROMETER) 
       || (type == SENSOR_TYPE_GRAVITY)
       || (type == SENSOR_TYPE_GYROSCOPE)
       || (type == SENSOR_TYPE_LINEAR_ACCELERATION)
       || (type == SENSOR_TYPE_MAGNETIC_FIELD)
       || (type == SENSOR_TYPE_ORIENTATION)) {
       for(i = 0; i < count; i++) {
           for(j = 0; j < 3; j++) 
               buffer[i].data[j] = param->constantValue;
       } 
    }
    if((type == SENSOR_TYPE_LIGHT) 
       || (type == SENSOR_TYPE_PRESSURE)
       || (type == SENSOR_TYPE_TEMPERATURE)
       || (type == SENSOR_TYPE_RELATIVE_HUMIDITY)
       || (type == SENSOR_TYPE_AMBIENT_TEMPERATURE)) {
        for(i = 0; i < count; i++) {
            buffer[i].data[0] = param->constantValue;
        }
    }
    if(type == SENSOR_TYPE_ROTATION_VECTOR) {
       for(i = 0; i < count; i++) {
           for(j = 0; j < 4; j++) 
               buffer[i].data[j] = param->constantValue;
       } 
    }
    return buffer;
}

sensors_event_t* SensorPerturb::transformData(
        int32_t type, sensors_event_t* buffer, size_t count, privacy_vec_t* param)
{
    ALOGD(" In function transformData ");

    if(param->action == ACTION_CONSTANT) {
        return SensorPerturb::constantData(type, buffer, count, param);
    }
    else 
        return buffer;
}

// ---------------------------------------------------------------------------
}; // namespace android
