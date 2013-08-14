#ifndef ANDROID_SENSOR_PERTURB_H
#define ANDROID_SENSOR_PERTURB_H

#include <stdint.h>
#include <sys/types.h>
#include "PrivacyRules.h"
#include <string>
#include "frameworks/native/services/sensorservice/FirewallConfigMessages.pb.h"
#include "frameworks/native/services/sensorservice/SensorCountMessages.pb.h"

#define NUM_SENSORTYPE 20

// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
using namespace android_sensorfirewall;
namespace android {
// ---------------------------------------------------------------------------

class SensorPerturb {
private:
<<<<<<< HEAD
	SensorCounter *counter;
	long start_time;
	long cur_time;
    float unifRand(float a, float b);
    float normal(float mean, float stdDev);
    float expo(float param);
    float getNoise(const Perturb* perturb);
    void perturbData(sensors_event_t* scratch, size_t start_pos,
            size_t end_pos, const int32_t sensorType, const Param* param);
    void constantData(sensors_event_t* scratch, size_t start_pos,
            size_t end_pos, const int32_t sensorType, const Param* param);
    void suppressData(sensors_event_t* scratch, size_t start_pos,
            size_t end_pos, size_t count);
    void PrintFirewallConfig();
    bool WriteStringToFile(const char* filename, const std::string& data);
    bool WriteFirewallConfig();
public:
	void initCounter();
    size_t transformData(uid_t uid, const char* pkgName, sensors_event_t* scratch,
            size_t count, PrivacyRules* mPrivacyRules);
};

}; // namespace android

#endif // ANDROID_SENSOR_PERTURB_H
