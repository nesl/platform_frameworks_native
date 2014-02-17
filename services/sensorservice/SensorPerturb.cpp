#include <stdint.h>
#include <math.h>
#include <sys/types.h>
#include <hardware/sensors.h>
#include <utils/Log.h>
#include <time.h>
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#include "SensorPerturb.h"
#include "frameworks/native/services/sensorservice/FirewallConfigMessages.pb.h"
#include "PrivacyRules.h"
#include "frameworks/native/services/sensorservice/SensorCountMessages.pb.h"

using namespace android_sensorfirewall;
namespace android {
// ---------------------------------------------------------------------------

void SensorPerturb::getDayTime(uint32_t* currDay, uint32_t* currHour, uint32_t* currMin) {
    char day[2], hour[3], min[3];
    time_t t = time(NULL);
    struct tm *timeInfo;
    timeInfo = localtime(&t); 
    strftime(day, sizeof(day), "%w", timeInfo); 
    strftime(hour, sizeof(hour), "%H", timeInfo); 
    strftime(min, sizeof(min), "%M", timeInfo); 
    *currDay = atoi(day); 
    *currHour = atoi(hour); 
    *currMin = atoi(min);
}

bool SensorPerturb::applyAtThisTime(uint32_t currHour, uint32_t currMin, const DateTime* dateTime) {
    bool applyRule;
    uint32_t currTime, fromTime, toTime;

    currTime = currHour*60 + currMin;
    fromTime = dateTime->fromhr()*60 + dateTime->frommin();
    toTime = dateTime->tohr()*60 + dateTime->tomin();

    if( dateTime->tohr() >= dateTime->fromhr() ) {
        if((currTime >= fromTime) && (currTime <= toTime))
            applyRule = true;
        else
            applyRule = false;
    }
    else {
        if((currTime >= toTime) && (currTime <= fromTime))
            applyRule = false;
        else
            applyRule = true;
    }
    return applyRule;
}

bool SensorPerturb::applyOnThisDay(uint32_t currDay, const DateTime* dateTime) {
    bool applyRule = false;
    int i = 0;
    while((i < dateTime->dayofweek_size()) && (!applyRule)) {
        if(currDay == dateTime->dayofweek(i)) {
            applyRule = true;
        }
        else
            i++;
    }
    return applyRule;
}

bool SensorPerturb::isRuleTimeApplicable(const Rule* rule) {
    bool applyRule = true;
    uint32_t currDay, currHour, currMin;
    SensorPerturb::getDayTime(&currDay, &currHour, &currMin);
    if(rule) {
        if(rule->has_datetime()) {
            const DateTime* dateTime = &(rule->datetime());
            int numDays = dateTime->dayofweek_size();
            if(numDays != 0) {
               if(SensorPerturb::applyOnThisDay(currDay, dateTime)) {
                    if((dateTime->has_fromhr()) && (dateTime->has_tohr())) {
                        if(!SensorPerturb::applyAtThisTime(currHour, currMin, dateTime)) {
                            applyRule = false;
                        }
                    }
                    else {
                        ALOGD("Time is not set. Apply rule at all times");
                    }
                   // if time is not properly set will apply for all
                   // times in that day. UI check should ensure the from
                   // and to fields are both set.
               }
               else { // dont apply rule today
                   applyRule = false;
               } 
            }
            else { // consider the rule to be set for every day of the week
                if((dateTime->has_fromhr()) && (dateTime->has_tohr())) {
                    if(!SensorPerturb::applyAtThisTime(currHour, currMin, dateTime)) {
                        applyRule = false;
                    }
                }
                // apply this rule for every day for all times
            }
        }
        // apply this rule for every day for all times
    }
    return applyRule;
}

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

int SensorPerturb::playbackData(
        sensors_event_t* scratch, size_t start_pos, size_t end_pos, 
        const int32_t sensorType, sensors_event_t* pbuf, size_t count) {
    int suppressCount = 0;
    if(pbuf[sensorType].type == -1) {
        SensorPerturb::suppressData(scratch, start_pos, end_pos, count);
        suppressCount = end_pos - start_pos + 1;
    } else {
        // check this part as right now buffer has only one element
        scratch[start_pos] = pbuf[sensorType];
        start_pos++;
        SensorPerturb::suppressData(scratch, start_pos, end_pos, count);
        suppressCount = end_pos - start_pos + 1;
    }
    return suppressCount;
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
   
    {
        Mutex::Autolock _l(mLock);
        std::fstream ofs(filename, std::ios::out | std::ios::binary);
        if (!ofs) {
            ALOGE("Failed to open file %s", filename);
            return false;
        }

        ofs << data;
        ofs.close();
    }

    char mode[] = "0755";
    int i = strtol(mode, 0, 8);
    if (chmod (filename, i) < 0) {
        ALOGE("Failed to chmod for file %s", filename);
    } else {
        ALOGD("Chmod 755 successfully %s", filename);
    }
    return true;
}

//TODO: put this in the configUtils.h file
bool SensorPerturb::WriteSensorCounters() {
    std::string data;
    const char* counterFileName = "/data/sensor-counter";
    if (!counter->SerializeToString(&data)) {
        ALOGE("SensorCounter: Failed to serialize to string.");
        return false;
    }

    if (!SensorPerturb::WriteStringToFile(counterFileName, data)) {
      ALOGE("SensorCounter: Failed to write serialized string to file.");
      return false;
    }

    return true;
}

//TODO: put this in the configUtils.h file
void SensorPerturb::PrintSensorCounters() {
    for (int ii = 0; ii < counter->appentry_size(); ++ii) {
        const AppEntry& appentry = counter->appentry(ii);
        ALOGD("pkgName = %s: pkgUid = %d:", appentry.pkgname().c_str(), appentry.uid());
        for (int j = 0; j < appentry.sensorentry_size(); ++j) {
            ALOGD("sensorType = %d, count = %lld", j, appentry.sensorentry(j).count());
        }
    }
}

void SensorPerturb::updateCounters(size_t count, size_t start_pos, 
        size_t end_pos, uid_t uid, const char* pkgName, const int32_t sensorType) {

    int ii = 0;
    bool flag = false;

    // Update count in SensorCounter here
    flag = false;
    for (ii = 0; ii < counter->appentry_size(); ii++) {
        if ((counter->appentry(ii).uid() == uid) 
            && (strcmp(counter->appentry(ii).pkgname().c_str(), pkgName) == 0)) {
            counter->mutable_appentry(ii)->mutable_sensorentry(sensorType)->set_count(
                counter->appentry(ii).sensorentry(sensorType).count() 
                + (end_pos - start_pos + 1));
            curr_time = (long)time(NULL);
            counter->mutable_appentry(ii)->set_lastupdate(curr_time);
            flag = true;
            //ALOGD("package %s sensor %d count=%lld\n", pkgName, sensorType, counter->appentry(ii).sensorentry(sensorType).count());
        }                
    }

    if ((curr_time - start_time) >= 60) {
        //SensorPerturb::PrintSensorCounters();
        SensorPerturb::WriteSensorCounters();
        start_time = curr_time;
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
}

size_t SensorPerturb::transformData(
        uid_t uid, const char* pkgName, sensors_event_t* scratch,
        size_t count, PrivacyRules* mPrivacyRules, sensors_event_t* pbuf) {

    size_t start_pos, end_pos;
    size_t i = 0, suppressCount ;
    bool toUpdateCounter;

    //ALOGD("transformData: uid = %d, pkgName = %s, count = %d\n", uid, pkgName, count);

    while (i < count) {
        const int32_t sensorType = scratch[i].type;
        start_pos = i;
        toUpdateCounter = true;

        while((i < count) && (scratch[i].type == sensorType)) {
                i++;
        }
        end_pos = i-1;

        // Transfrom data
        const ruleKey_t* mKey = mPrivacyRules->generateKey(uid, sensorType, pkgName);
        const Rule* rule = mPrivacyRules->findRule(mKey);
        if(rule) {
            if(SensorPerturb::isRuleTimeApplicable(rule)) {
                const Action* action = &rule->action();
                const Param* param = &action->param();
                switch(action->actiontype()) {
                    case Action::ACTION_SUPPRESS: 
                        SensorPerturb::suppressData(scratch, start_pos, end_pos, count);
                        i = start_pos;
                        count = count - (end_pos - start_pos + 1);
                        toUpdateCounter = false;
                        //ALOGD("Suppressing Data");
                        break;
                    case Action::ACTION_CONSTANT: 
                        SensorPerturb::constantData(scratch, start_pos, end_pos, sensorType, param);
                        //ALOGD("Constant Data");
                        break;
                    case Action::ACTION_PLAYBACK:
                        suppressCount = SensorPerturb::playbackData(scratch, start_pos, end_pos,
                                sensorType, pbuf, count);
                        //suppressCount is equal to number of deletions
                        i = end_pos - suppressCount + 1;
                        count = count - suppressCount;
                        break;
                    case Action::ACTION_PERTURB:
                        SensorPerturb::perturbData(scratch, start_pos, end_pos, sensorType, param);
                        //ALOGD("Perturb Data");
                        break;
                    case Action::ACTION_PASSTHROUGH:
                        //ALOGD("No changes applied");
                        break;
                    default:
                        break;
                        //ALOGD("No Changes applied");
                }
            }
        }

        if(toUpdateCounter) {
            SensorPerturb::updateCounters(count, start_pos, end_pos, uid, pkgName, sensorType);
        }
   }
    return count;
}

// ---------------------------------------------------------------------------
}; // namespace android
