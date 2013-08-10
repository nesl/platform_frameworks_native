#ifndef ANDROID_SENSOR_PERTURB_H
#define ANDROID_SENSOR_PERTURB_H

#include <stdint.h>
#include <sys/types.h>
#include "PrivacyRules.h"
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
	SensorCounter *counter;
    void constantData(sensors_event_t* scratch, size_t start_pos,
            size_t end_pos, const int32_t sensorType, const Param* param);
    void suppressData(sensors_event_t* scratch, size_t start_pos,
            size_t end_pos, size_t count);
public:
	void initCounter();
    size_t transformData(uid_t uid, const char* pkgName, sensors_event_t* scratch,
            size_t count, PrivacyRules* mPrivacyRules);
};

}; // namespace android

#endif // ANDROID_SENSOR_PERTURB_H
