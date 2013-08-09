#include <stdint.h>
#include <math.h>
#include <sys/types.h>
#include <hardware/sensors.h>
#include <utils/Log.h>

#include "SensorPerturb.h"
#include "frameworks/native/services/sensorservice/FirewallConfigMessages.pb.h"
#include "PrivacyRules.h"

using namespace android_sensorfirewall;
namespace android {
// ---------------------------------------------------------------------------
void SensorPerturb::constantData(
        sensors_event_t* scratch, size_t start_pos, size_t end_pos, 
        const int32_t sensorType, const Param* param)
{
    size_t i,j;
    
    if( param ) {
        if((sensorType == SENSOR_TYPE_ACCELEROMETER) 
           || (sensorType == SENSOR_TYPE_GRAVITY)
           || (sensorType == SENSOR_TYPE_GYROSCOPE)
           || (sensorType == SENSOR_TYPE_LINEAR_ACCELERATION)
           || (sensorType == SENSOR_TYPE_MAGNETIC_FIELD)
           || (sensorType == SENSOR_TYPE_ORIENTATION)) {
           for(i = start_pos; i <= end_pos; i++) {
               for(j = 0; j < 3; j++) 
                   scratch[i].data[j] = param->constantvalue();
           } 
        }
        if((sensorType == SENSOR_TYPE_LIGHT) 
           || (sensorType == SENSOR_TYPE_PRESSURE)
           || (sensorType == SENSOR_TYPE_TEMPERATURE)
           || (sensorType == SENSOR_TYPE_RELATIVE_HUMIDITY)
           || (sensorType == SENSOR_TYPE_AMBIENT_TEMPERATURE)) {
            for(i = start_pos; i <= end_pos; i++) {
                scratch[i].data[0] = param->constantvalue();
            }
        }
        if(sensorType == SENSOR_TYPE_ROTATION_VECTOR) {
           for(i = start_pos; i <= end_pos; i++) {
               for(j = 0; j < 4; j++) 
                   scratch[i].data[j] = param->constantvalue();
           } 
        }
    }
    else {
        ALOGE("Passing null parameter to constantValue. No changes made");
    }
}

void SensorPerturb::suppressData(
        sensors_event_t* scratch, size_t start_pos, size_t end_pos, size_t count)
{
    size_t i, j;
    i = start_pos;
    j = end_pos + 1;
    while(j < count) {
        scratch[i] = scratch[j];
        i++;
        j++;
    }
}

size_t SensorPerturb::transformData(
        uid_t uid, const char* pkgName, sensors_event_t* scratch, 
        size_t count, PrivacyRules* mPrivacyRules )
{
    size_t start_pos, end_pos;
    size_t i=0;

    ALOGD("transformData: uid = %d, pkgName = %s, count = %d\n", uid, pkgName, count);

    while (i < count) {
        const int32_t sensorType = scratch[i].type;
        start_pos = i;
        while((i < count) && (scratch[i].type == sensorType)) {
                i++;
        }
        end_pos = i-1;
        ALOGD("transformData: sensortype = %d\n", sensorType);
        const ruleKey_t* mKey = mPrivacyRules->generateKey(uid, sensorType, pkgName);
        const Rule* rule = mPrivacyRules->findRule(mKey);
        if(rule) {
            const Action* action = &rule->action();
            const Param* param = &action->param();
            ALOGD("transformData::actionType = %d\n", action->actiontype());
            switch(action->actiontype()) {
                case Action::ACTION_SUPPRESS: 
                    SensorPerturb::suppressData(scratch, start_pos, end_pos, count);
                    i = start_pos;
                    count = count - (end_pos - start_pos + 1);
                    ALOGD("Suppressing Data");
                    break;
                case Action::ACTION_CONSTANT: 
                    SensorPerturb::constantData(scratch, start_pos, end_pos, sensorType, param);
                    ALOGD("Constant Data");
                    break;
                case Action::ACTION_DELAY:
                    ALOGD("Delay Data");
                    break;
                case Action::ACTION_PERTURB:
                    ALOGD("Perturb Data");
                    break;
                case Action::ACTION_PASSTHROUGH:
                    ALOGD("No changes applied");
                    break;
                default:
                    ALOGD("No Changes applied");
            }
        }
   }
    return count;
}

// ---------------------------------------------------------------------------
}; // namespace android
