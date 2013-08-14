#include <stdint.h>
#include <math.h>
#include <sys/types.h>
#include <hardware/sensors.h>
#include <utils/Log.h>
#include <time.h>
#include <iostream>
#include <fstream>

#include "SensorPerturb.h"
#include "frameworks/native/services/sensorservice/FirewallConfigMessages.pb.h"
#include "PrivacyRules.h"
#include "frameworks/native/services/sensorservice/SensorCountMessages.pb.h"

using namespace android_sensorfirewall;
namespace android {
// ---------------------------------------------------------------------------
void SensorPerturb::initCounter() 
{
    // should also add code to read previously existing file
    counter = new SensorCounter();
    start_time = (long)time(NULL);

}

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

bool SensorPerturb::WriteStringToFile(const char* filename, const std::string& data) {
    std::fstream ofs(filename, std::ios::out | std::ios::binary);
    if (!ofs) {
        ALOGE("Failed to open file %s", filename);
        return false;
    }

    ofs << data;
    ofs.close();
    return true;
}

bool SensorPerturb::WriteFirewallConfig() {
    std::string data;
    if (!counter->SerializeToString(&data)) {
        ALOGE("SensorCounter: Failed to serialize to string.");
        return false;
    }

    if (!WriteStringToFile("/data/sensor-counter", data)) {
      ALOGE("SensorCounter: Failed to write serialized string to file.");
      return false;
    }

    return true;
}


void SensorPerturb::PrintFirewallConfig() {
    for (int ii = 0; ii < counter->appentry_size(); ++ii) {
        const AppEntry& appentry = counter->appentry(ii);
        ALOGD("pkgName = %s: pkgUid = %d:", appentry.pkgname().c_str(), appentry.uid());
        for (int j = 0; j < appentry.sensorentry_size(); ++j) {
            ALOGD("sensorType = %d, count = %lld", j, appentry.sensorentry(j).count());
        }
    }
}

size_t SensorPerturb::transformData(
        uid_t uid, const char* pkgName, sensors_event_t* scratch, 
        size_t count, PrivacyRules* mPrivacyRules ) {
    size_t start_pos, end_pos;
    size_t i=0;
    ALOGD("transformData: uid = %d, pkgName = %s, count = %d\n", uid, pkgName, count);

    int ii = 0;
    bool flag = false;

    while (i < count) {
        const int32_t sensorType = scratch[i].type;
        start_pos = i;
        while((i < count) && (scratch[i].type == sensorType)) {
                i++;
        }
        end_pos = i-1;

        // Update count in SensorCounter here
        flag = false;
        for (ii = 0; ii < counter->appentry_size(); ii++) {
            if ((counter->appentry(ii).uid() == uid) 
                && (strcmp(counter->appentry(ii).pkgname().c_str(), pkgName) == 0)) {
                counter->mutable_appentry(ii)->mutable_sensorentry(sensorType)->set_count(
                    counter->appentry(ii).sensorentry(sensorType).count() 
                    + (end_pos - start_pos + 1));
                cur_time = (long)time(NULL);
                counter->mutable_appentry(ii)->set_lastupdate(cur_time);
                flag = true;

                ALOGD("package %s sensor %d count=%lld\n", pkgName, sensorType, counter->appentry(ii).sensorentry(sensorType).count());
            }                
        }

        if ((cur_time - start_time) >= 60) {
            PrintFirewallConfig();
            WriteFirewallConfig();
            start_time = cur_time;
        }

        if (!flag) {
            AppEntry *aEntry = counter->add_appentry();
            aEntry->set_uid(uid);
            aEntry->set_pkgname(pkgName);
            aEntry->set_lastupdate((long)time(NULL));

            SensorEntry *sEntry = NULL;
            for (ii = 0; ii < NUM_SENSORTYPE; ii++) {
                sEntry = aEntry->add_sensorentry();
                sEntry->set_count(0);
            }
        }

        // Transfrom data
        //ALOGD("transformData: sensortype = %d\n", sensorType);
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
