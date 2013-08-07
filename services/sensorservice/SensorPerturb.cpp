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

sensors_event_t* SensorPerturb::suppressData(
        int32_t type, privacy_vec_t* param)
{
    return NULL;
}

sensors_event_t* SensorPerturb::transformData(
        int32_t type, sensors_event_t* buffer, size_t count, privacy_vec_t* param)
{
    ALOGD(" In function transformData ");

    if(param->action == ACTION_CONSTANT) {
        return SensorPerturb::constantData(type, buffer, count, param);
    }
    else if(param->action == ACTION_SUPPRESS) {
        return SensorPerturb::suppressData(type, param);
    }
    else 
        return buffer;
}

// ---------------------------------------------------------------------------
}; // namespace android
