#include <stdint.h>
#include <math.h>
#include <sys/types.h>
#include <hardware/sensors.h>
#include <utils/Log.h>
#include <math.h>
#include <ctime>

#include "SensorPerturb.h"
#include "frameworks/native/services/sensorservice/FirewallConfigMessages.pb.h"
#include "PrivacyRules.h"

using namespace android_sensorfirewall;
namespace android {
// ---------------------------------------------------------------------------

float SensorPerturb::unifRand(float a, float b) {
    double unifVal = rand()/double(RAND_MAX);
    return (b-a)*unifVal + a;
}

float SensorPerturb::normal(float mean, float stdDev) {
    float u, v;
    u = SensorPerturb::unifRand(0, 1);
    v = SensorPerturb::unifRand(0, 1);
    return mean + stdDev*cos(2*3.14159*u)*sqrt(-log(v));
}

float SensorPerturb::expo(float param) {
    float u = SensorPerturb::unifRand(0, 1);
    return -log(1-u)/param;
}

float SensorPerturb::getNoise(const Perturb* perturb) {
    float num = 0.0;
    float stdDev = sqrt(perturb->variance());
    int distType = perturb->disttype();

    switch(distType) {
        case Perturb::GAUSSIAN:
            num = SensorPerturb::normal(perturb->mean(), stdDev);
            break;
        case Perturb::UNIFORM:
            num = SensorPerturb::unifRand(perturb->unifmin(), perturb->unifmax());
            break;
        case Perturb::EXPONENTIAL:
            num = SensorPerturb::expo(perturb->expparam());
            break;            
    }
    return num;
}

void SensorPerturb::perturbData(
        sensors_event_t* scratch, size_t start_pos, size_t end_pos, 
        const int32_t sensorType, const Param* param) {

    size_t i,j;
    const Perturb* perturb = &(param->perturb());
    srand((unsigned)time(NULL));

    if(perturb) {
        if((sensorType == SENSOR_TYPE_ACCELEROMETER) 
           || (sensorType == SENSOR_TYPE_GRAVITY)
           || (sensorType == SENSOR_TYPE_GYROSCOPE)
           || (sensorType == SENSOR_TYPE_LINEAR_ACCELERATION)
           || (sensorType == SENSOR_TYPE_MAGNETIC_FIELD)
           || (sensorType == SENSOR_TYPE_ORIENTATION)) {
           for(i = start_pos; i <= end_pos; i++) {
               for(j = 0; j < 3; j++) {
                   scratch[i].data[j] = scratch[i].data[j] + 
                       getNoise(perturb);
               }
           }
        }
        if((sensorType == SENSOR_TYPE_LIGHT) 
           || (sensorType == SENSOR_TYPE_PRESSURE)
           || (sensorType == SENSOR_TYPE_TEMPERATURE)
           || (sensorType == SENSOR_TYPE_RELATIVE_HUMIDITY)
           || (sensorType == SENSOR_TYPE_AMBIENT_TEMPERATURE)) {
           for(i = start_pos; i <= end_pos; i++) {
                scratch[i].data[0] = scratch[i].data[0] +
                    getNoise(perturb);
           }
        }
        if(sensorType == SENSOR_TYPE_ROTATION_VECTOR) {
            for(i = start_pos; i <= end_pos; i++) {
                for(j = 0; j < 4; j++) {
                   scratch[i].data[j] = scratch[i].data[j] + 
                       getNoise(perturb);
                }
            }
        }
    }
    else {
        ALOGE("Please set perturbation params. No changes to data.");
    }
}

void SensorPerturb::constantData(
        sensors_event_t* scratch, size_t start_pos, size_t end_pos, 
        const int32_t sensorType, const Param* param) {

    size_t i,j;
    bool useDefault = true;

    if(param) {
        const SensorValue* sensorValue = &(param->constantvalue());
        if(sensorValue) {
            if((sensorType == SENSOR_TYPE_ACCELEROMETER) 
               || (sensorType == SENSOR_TYPE_GRAVITY)
               || (sensorType == SENSOR_TYPE_GYROSCOPE)
               || (sensorType == SENSOR_TYPE_LINEAR_ACCELERATION)
               || (sensorType == SENSOR_TYPE_MAGNETIC_FIELD)
               || (sensorType == SENSOR_TYPE_ORIENTATION)) {
                   if(sensorValue->has_vecval())
                       useDefault = false;
                   if(!useDefault) {
                        const VectorValue& vecVal = sensorValue->vecval();   
                        for(i = start_pos; i <= end_pos; i++) {
                            scratch[i].data[0] = vecVal.x();
                            scratch[i].data[1] = vecVal.y();
                            scratch[i].data[2] = vecVal.z();
                        }
                   }
                   else {
                        for(i = start_pos; i <= end_pos; i++) {
                            for(j = 0; j < 3; j++) 
                                scratch[i].data[j] = sensorValue->defaultval();
                        }
                   }
            }
            if((sensorType == SENSOR_TYPE_LIGHT) 
               || (sensorType == SENSOR_TYPE_PRESSURE)
               || (sensorType == SENSOR_TYPE_TEMPERATURE)
               || (sensorType == SENSOR_TYPE_RELATIVE_HUMIDITY)
               || (sensorType == SENSOR_TYPE_AMBIENT_TEMPERATURE)) {
               if(sensorValue->has_scalarval()) {
                   for(i = start_pos; i <= end_pos; i++) 
                        scratch[i].data[0] = sensorValue->scalarval();
               }
               else {
                    for(i = start_pos; i <= end_pos; i++)
                        scratch[i].data[0] = sensorValue->defaultval();
               }
            }
            if(sensorType == SENSOR_TYPE_ROTATION_VECTOR) {
                if(sensorValue->has_vecval()) 
                    useDefault = false;
                if(!useDefault) {
                    const VectorValue& vecVal = sensorValue->vecval();   
                    for(i = start_pos; i <= end_pos; i++) {
                        scratch[i].data[0] = vecVal.x();
                        scratch[i].data[1] = vecVal.y();
                        scratch[i].data[2] = vecVal.z();
                        scratch[i].data[3] = vecVal.theta();
                    }
                }
                else {
                    for(i = start_pos; i <= end_pos; i++) {
                        for(j = 0; j < 4; j++) 
                            scratch[i].data[j] = sensorValue->defaultval();
                    } 
                }
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
        const ruleKey_t* mKey = mPrivacyRules->generateKey(uid, sensorType, pkgName);
        const Rule* rule = mPrivacyRules->findRule(mKey);
        if(rule) {
            const Action* action = &rule->action();
            const Param* param = &action->param();
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
                    SensorPerturb::perturbData(scratch, start_pos, end_pos, sensorType, param);
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
