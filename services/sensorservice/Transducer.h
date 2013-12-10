#ifndef ANDROID_TRANSDUCER_H
#define ANDROID_TRANSDUCER_H

#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>

#include <utils/Singleton.h>

#include <gui/BitTube.h>

namespace android {
// ---------------------------------------------------------------------------

class Transducer : public Singleton<Transducer> {
    friend class Singleton<Transducer>;

    Transducer() : mInputChannel(new BitTube()) {
      ALOGD("Transducer::Transducer()");
    }

    void onFirstRef() {
    }

    sp<BitTube> const mInputChannel;

public:
    sp<BitTube> getInputChannel() {
        return mInputChannel;
    }

    ssize_t poll(sensors_event_t* buffer, size_t numMaxEvents) {
        sleep(1);
        ssize_t numEvents = BitTube::recvObjects(mInputChannel, buffer, numMaxEvents);
        return numEvents;
    }

    void fillWithSyntheticEvents(sensors_event_t* buffer, size_t numMaxEvents) const {
        for (size_t ii = 0; ii < numMaxEvents/2; ++ii) {
            sensors_event_t& event_out = buffer[ii];
            event_out.version = sizeof(struct sensors_event_t);
            // FIXME: The .sensor field should be an id,
            // not its type.
            event_out.sensor = SENSOR_TYPE_ACCELEROMETER;
            event_out.type = SENSOR_TYPE_ACCELEROMETER;
            event_out.acceleration.x = -1.3;
            event_out.acceleration.y = 1.8;
            event_out.acceleration.z = -0.7;
            event_out.timestamp = now_nanosec();
        }
    }

    int64_t now_nanosec() const {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        return (int64_t) now.tv_sec*1000000000LL + now.tv_nsec;
    }
};

ANDROID_SINGLETON_STATIC_INSTANCE(Transducer)

// ---------------------------------------------------------------------------
}; // namespace android

#endif
