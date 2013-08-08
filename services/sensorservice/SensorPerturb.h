#ifndef ANDROID_SENSOR_PERTURB_H
#define ANDROID_SENSOR_PERTURB_H

#include <stdint.h>
#include <sys/types.h>
#include "PrivacyRules.h"
#include "frameworks/native/services/sensorservice/FirewallConfigMessages.pb.h"

// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
using namespace android_sensorfirewall;
namespace android {
// ---------------------------------------------------------------------------

class SensorPerturb {
private:
    void constantData(sensors_event_t* scratch, size_t start_pos,
            size_t end_pos, const int32_t sensorType, const Param* param);
    void suppressData(sensors_event_t* scratch, size_t start_pos,
            size_t end_pos, size_t count);
public:
    size_t transformData(uid_t uid, const char* pkgName, sensors_event_t* scratch,
            size_t count, PrivacyRules* mPrivacyRules);
};

}; // namespace android

#endif // ANDROID_SENSOR_PERTURB_H
