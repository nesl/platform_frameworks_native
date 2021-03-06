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

#include <fcntl.h>
#include <fstream>
#include <sstream>
#include <math.h>
#include <stdint.h>
#include <string>
#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cutils/properties.h>

#include <utils/SortedVector.h>
#include <utils/KeyedVector.h>
#include <utils/threads.h>
#include <utils/Atomic.h>
#include <utils/Errors.h>
#include <utils/RefBase.h>
#include <utils/Singleton.h>
#include <utils/String16.h>

#include <binder/BinderService.h>
#include <binder/IServiceManager.h>
#include <binder/PermissionCache.h>

#include <gui/ISensorServer.h>
#include <gui/ISensorEventConnection.h>
#include <gui/SensorEventQueue.h>

#include <hardware/sensors.h>

#include "BatteryService.h"
#include "CorrectedGyroSensor.h"
#include "frameworks/native/services/sensorservice/FirewallConfigMessages.pb.h"
#include "FirewallConfigUtils-inl.h"
#include "GravitySensor.h"
#include "LinearAccelerationSensor.h"
#include "OrientationSensor.h"
#include "RotationVectorSensor.h"
#include "SensorFusion.h"
#include "SensorService.h"
#include "PrivacyRules.h"
#include "SensorPerturb.h"
#include "frameworks/native/services/sensorservice/SensorCountMessages.pb.h"

using namespace android_sensorfirewall;

namespace android {
// ---------------------------------------------------------------------------

/*
 * Notes:
 *
 * - what about a gyro-corrected magnetic-field sensor?
 * - run mag sensor from time to time to force calibration
 * - gravity sensor length is wrong (=> drift in linear-acc sensor)
 *
 */

double SensorService::total_time;
double SensorService::last_time;
int SensorService::count_perturb;
unsigned int SensorService::print_limit;

double getTime();

bool SensorService::is_empty(int index)
{
    if (!buffer[index].cnt)
        return true;
    else
        return false;
}

bool SensorService::is_full(int index)
{
    if (buffer[index].cnt == QUEUE_LENGTH)
        return true;
    else
        return false;
}

sensors_event_t SensorService::_deque(int index)
{
    sensors_event_t buf;
    int &f = buffer[index].f;

    buf = buffer[index].event_queue[f];
    f = (f + 1) % QUEUE_LENGTH;
    buffer[index].cnt--;

    return buf;
}

bool SensorService::deque(int index, sensors_event_t &buf)
{
    if(is_empty(index))
        return false;
    buf = _deque(index);
    return true;
}

// The idea here is that we 'll deque the sensor data but
// will not increment the f pointer. The increment will
// happen only if the data is consumed.
bool SensorService::mod_deque(int index, sensors_event_t &buf)
{
    if(is_empty(index))
        return false;

    int f = buffer[index].f;
    buf = buffer[index].event_queue[f];
    return true;
}

void SensorService::_enque(sensors_event_t event)
{
    int &r = buffer[event.type].r;

    buffer[event.type].event_queue[r] = event;
    r = (r + 1) % QUEUE_LENGTH;
    buffer[event.type].cnt++;
}

bool SensorService::enque(sensors_event_t event)
{
    if (is_full(event.type))
        return false;
    _enque(event);
    return true;
}

int SensorService::pop_unused_perturb_buffer(sensors_event_t buf[])
{
    int i;
    for (i = 0; i < NUMBER_OF_SENSORS; i++) {
        if (buf[i].type != -1 && buf[i].reserved0 == 200)
            deque(i, buf[i]);
    }
    return 0;
}

int SensorService::copy_perturb_buffer(sensors_event_t buf[])
{
    int i;
    for (i = 0; i < NUMBER_OF_SENSORS; i++) {
//      if (deque(i, buf[i]) == false)
        if (mod_deque(i, buf[i]) == false)
            buf[i].type = -1;
        else {
            buf[i].reserved0 = 100;
            ALOGD("IPS:cp timestamp %lld type %d float values =%f %f %f",
                    buf[i].timestamp, buf[i].type,
                    buf[i].data[0], buf[i].data[1], buf[i].data[2]);
        }
    }
    return 0;
}

int
sensor_dup(sensors_event_t *event, ASensorEvent aevent)
{
    int i;

    if (!event)
        return -1;

    event->version = aevent.version;
    event->sensor  = aevent.sensor;
    event->type    = aevent.type;
    event->reserved0 = aevent.reserved0;
    event->timestamp = aevent.timestamp;

    for (i = 0; i < 16; i++)
        event->data[i] = aevent.data[i];

    for (i = 0; i < 4; i++)
        event->reserved1[i] = aevent.reserved1[i];
    return 0;
}

double getNextTime()
{
    return  getTime() + 1;
}

SensorService::SensorService()
    : mInitCheck(NO_INIT)
{
    struct stat buf;
    int ret;

    // Initialize the queue
    for (int i = 0; i < NUMBER_OF_SENSORS; i++) {
        buffer[i].f = 0;
        buffer[i].r = 0;
        buffer[i].cnt = 0;
    }
    nextTime = getNextTime();

    ret = stat(TRUSTED_FILE_STORE, &buf);
    if (ret < 0)
        mtime = buf.st_mtime;
    else
        mtime = 0;
    *trusted_pkgname = '\0';
}

SensorPerturb mSensorPerturb;

bool SensorService::threadLoop_pb()
{
    int ret = 0;
    sensors_event_t event;
    static int x = 0;
    std::string pkgName;
    struct stat buf;

    double cur = getTime();
    if (cur >= nextTime) {
        nextTime = getNextTime();
        ret = stat(TRUSTED_FILE_STORE, &buf);
        if (ret < 0) {
            *trusted_pkgname = '\0';
            sleep(1);
            return false;
        }
        if (mtime != buf.st_mtime) {
            std::ifstream f(TRUSTED_FILE_STORE);
            f >> pkgName;
            if (pkgName.empty()) {
                *trusted_pkgname = '\0';
                ALOGD("IPS: trusted package name changed to none");
            } else {
                ALOGD("IPS: trusted package name: %s", pkgName.data());
                pkgName.copy(trusted_pkgname, pkgName.length());
                trusted_pkgname[pkgName.length()] = '\0';
            }
            mtime = buf.st_mtime;
        }
    }

    if (!*trusted_pkgname) {
        sleep(1);
        return false;
    }

    // receive our events to clients...
    const SortedVector< wp<SensorEventConnection> > activeConnections(
            getActiveConnections());
    size_t numConnections = activeConnections.size();
    for (size_t i=0 ; i<numConnections ; i++) {
        sp<SensorEventConnection> connection(
                activeConnections[i].promote());
        if (connection != 0 &&
                !strcmp(connection->getPkgName(), trusted_pkgname)) {
            ret = connection->recvEvents(&event);
            if (ret == 0) {
                enque(event);
                ALOGD("IPS:timestamp %lld type %d float values =%f %f %f",
                        event.timestamp, event.type,
                        event.data[0], event.data[1], event.data[2]);
            } else if (ret < 0)
                ALOGD("IPS: error %s", strerror(-ret));
        }
    }
    print_limit++;
    if (print_limit % 1000000000 == 0)
        ALOGD("IPS: threadLoop_pb");
    return true;
}

void SensorService::onFirstRef()
{
    ALOGD("nuSensorService starting...");

    SensorDevice& dev(SensorDevice::getInstance());

    if (dev.initCheck() == NO_ERROR) {
        sensor_t const* list;
        ssize_t count = dev.getSensorList(&list);
        if (count > 0) {
            ssize_t orientationIndex = -1;
            bool hasGyro = false;
            uint32_t virtualSensorsNeeds =
                    (1<<SENSOR_TYPE_GRAVITY) |
                    (1<<SENSOR_TYPE_LINEAR_ACCELERATION) |
                    (1<<SENSOR_TYPE_ROTATION_VECTOR);

            mLastEventSeen.setCapacity(count);
            for (ssize_t i=0 ; i<count ; i++) {
                registerSensor( new HardwareSensor(list[i]) );
                switch (list[i].type) {
                    case SENSOR_TYPE_ORIENTATION:
                        orientationIndex = i;
                        break;
                    case SENSOR_TYPE_GYROSCOPE:
                        hasGyro = true;
                        break;
                    case SENSOR_TYPE_GRAVITY:
                    case SENSOR_TYPE_LINEAR_ACCELERATION:
                    case SENSOR_TYPE_ROTATION_VECTOR:
                        virtualSensorsNeeds &= ~(1<<list[i].type);
                        break;
                }
            }

            // it's safe to instantiate the SensorFusion object here
            // (it wants to be instantiated after h/w sensors have been
            // registered)
            const SensorFusion& fusion(SensorFusion::getInstance());

            if (hasGyro) {
                // Always instantiate Android's virtual sensors. Since they are
                // instantiated behind sensors from the HAL, they won't
                // interfere with applications, unless they looks specifically
                // for them (by name).

                registerVirtualSensor( new RotationVectorSensor() );
                registerVirtualSensor( new GravitySensor(list, count) );
                registerVirtualSensor( new LinearAccelerationSensor(list, count) );

                // these are optional
                registerVirtualSensor( new OrientationSensor() );
                registerVirtualSensor( new CorrectedGyroSensor(list, count) );
            }

            // build the sensor list returned to users
            mUserSensorList = mSensorList;

            if (hasGyro) {
                // virtual debugging sensors are not added to mUserSensorList
                registerVirtualSensor( new GyroDriftSensor() );
            }

            if (hasGyro &&
                    (virtualSensorsNeeds & (1<<SENSOR_TYPE_ROTATION_VECTOR))) {
                // if we have the fancy sensor fusion, and it's not provided by the
                // HAL, use our own (fused) orientation sensor by removing the
                // HAL supplied one form the user list.
                if (orientationIndex >= 0) {
                    mUserSensorList.removeItemsAt(orientationIndex);
                }
            }

            // debugging sensor list
            for (size_t i=0 ; i<mSensorList.size() ; i++) {
                switch (mSensorList[i].getType()) {
                    case SENSOR_TYPE_GRAVITY:
                    case SENSOR_TYPE_LINEAR_ACCELERATION:
                    case SENSOR_TYPE_ROTATION_VECTOR:
                        if (strstr(mSensorList[i].getVendor().string(), "Google")) {
                            mUserSensorListDebug.add(mSensorList[i]);
                        }
                        break;
                    default:
                        mUserSensorListDebug.add(mSensorList[i]);
                        break;
                }
            }

            run("SensorService", PRIORITY_URGENT_DISPLAY);
            run_pb("SensorService_playback", PRIORITY_URGENT_DISPLAY);
            mInitCheck = NO_ERROR;
        }
    }

    // init counter in SensorPerturb
    mSensorPerturb.initCounter();
    total_time = 0;
    last_time = 0;
    count_perturb = 0;

}

void SensorService::registerSensor(SensorInterface* s)
{
    sensors_event_t event;
    memset(&event, 0, sizeof(event));

    const Sensor sensor(s->getSensor());
    // add to the sensor list (returned to clients)
    mSensorList.add(sensor);
    // add to our handle->SensorInterface mapping
    mSensorMap.add(sensor.getHandle(), s);
    // create an entry in the mLastEventSeen array
    mLastEventSeen.add(sensor.getHandle(), event);
}

void SensorService::registerVirtualSensor(SensorInterface* s)
{
    registerSensor(s);
    mVirtualSensorList.add( s );
}

SensorService::~SensorService()
{
    for (size_t i=0 ; i<mSensorMap.size() ; i++)
        delete mSensorMap.valueAt(i);
}

static const String16 sDump("android.permission.DUMP");

status_t SensorService::dump(int fd, const Vector<String16>& args)
{
    const size_t SIZE = 1024;
    char buffer[SIZE];
    String8 result;
    if (!PermissionCache::checkCallingPermission(sDump)) {
        snprintf(buffer, SIZE, "Permission Denial: "
                "can't dump SurfaceFlinger from pid=%d, uid=%d\n",
                IPCThreadState::self()->getCallingPid(),
                IPCThreadState::self()->getCallingUid());
        result.append(buffer);
    } else {
        Mutex::Autolock _l(mLock);
        result.append(buffer);
        snprintf(buffer, SIZE, "Sensor List:\n");
        for (size_t i=0 ; i<mSensorList.size() ; i++) {
            const Sensor& s(mSensorList[i]);
            const sensors_event_t& e(mLastEventSeen.valueFor(s.getHandle()));
            snprintf(buffer, SIZE,
                    "%-48s| %-32s | 0x%08x | maxRate=%7.2fHz | "
                    "last=<%5.1f,%5.1f,%5.1f>\n",
                    s.getName().string(),
                    s.getVendor().string(),
                    s.getHandle(),
                    s.getMinDelay() ? (1000000.0f / s.getMinDelay()) : 0.0f,
                    e.data[0], e.data[1], e.data[2]);
            result.append(buffer);
        }
        SensorFusion::getInstance().dump(result, buffer, SIZE);
        SensorDevice::getInstance().dump(result, buffer, SIZE);

        snprintf(buffer, SIZE, "%d active connections\n",
                mActiveConnections.size());
        result.append(buffer);
        snprintf(buffer, SIZE, "Active sensors:\n");
        result.append(buffer);
        for (size_t i=0 ; i<mActiveSensors.size() ; i++) {
            int handle = mActiveSensors.keyAt(i);
            snprintf(buffer, SIZE, "%s (handle=0x%08x, connections=%d)\n",
                    getSensorName(handle).string(),
                    handle,
                    mActiveSensors.valueAt(i)->getNumConnections());
            result.append(buffer);
        }
    }
    write(fd, result.string(), result.size());
    return NO_ERROR;
}

bool SensorService::threadLoop()
{
    ALOGD("nuSensorService thread starting...");

    const size_t numEventMax = 16;
    const size_t minBufferSize = numEventMax + numEventMax * mVirtualSensorList.size();
    sensors_event_t buffer[minBufferSize];
    sensors_event_t scratch[minBufferSize];
    SensorDevice& device(SensorDevice::getInstance());
    const size_t vcount = mVirtualSensorList.size();
    sensors_event_t pbuf[NUMBER_OF_SENSORS];
    int mycnt = 0;

    ssize_t count;
    do {
        count = device.poll(buffer, numEventMax);
        if (count<0) {
            ALOGE("sensor poll failed (%s)", strerror(-count));
            break;
        }

        recordLastValue(buffer, count);

        // handle virtual sensors
        if (count && vcount) {
            sensors_event_t const * const event = buffer;
            const DefaultKeyedVector<int, SensorInterface*> virtualSensors(
                    getActiveVirtualSensors());
            const size_t activeVirtualSensorCount = virtualSensors.size();
            if (activeVirtualSensorCount) {
                size_t k = 0;
                SensorFusion& fusion(SensorFusion::getInstance());
                if (fusion.isEnabled()) {
                    for (size_t i=0 ; i<size_t(count) ; i++) {
                        fusion.process(event[i]);
                    }
                }
                for (size_t i=0 ; i<size_t(count) && k<minBufferSize ; i++) {
                    for (size_t j=0 ; j<activeVirtualSensorCount ; j++) {
                        if (count + k >= minBufferSize) {
                            ALOGE("buffer too small to hold all events: "
                                    "count=%u, k=%u, size=%ld",
                                    count, k, minBufferSize);
                            break;
                        }
                        sensors_event_t out;
                        SensorInterface* si = virtualSensors.valueAt(j);
                        if (si->process(&out, event[i])) {
                            buffer[count + k] = out;
                            k++;
                        }
                    }
                }
                if (k) {
                    // record the last synthesized values
                    recordLastValue(&buffer[count], k);
                    count += k;
                    // sort the buffer by time-stamps
                    sortEventBuffer(buffer, count);
                }
            }
        }

        //make a copy of the perturbed buffer
        copy_perturb_buffer(pbuf);

        // send our events to clients...
        const SortedVector< wp<SensorEventConnection> > activeConnections(
                getActiveConnections());
        size_t numConnections = activeConnections.size();
        for (size_t i=0 ; i<numConnections ; i++) {
            sp<SensorEventConnection> connection(
                    activeConnections[i].promote());
            if (connection != 0) {
                connection->sendEvents(buffer, count, scratch, false, pbuf);
            }
        }

        // remove only those elements from the playback buffer that were used.
        pop_unused_perturb_buffer(pbuf);

    } while (count >= 0 || Thread::exitPending());

    ALOGW("Exiting SensorService::threadLoop => aborting...");
    abort();
    return false;
}

void SensorService::recordLastValue(
        sensors_event_t const * buffer, size_t count)
{
    Mutex::Autolock _l(mLock);

    // record the last event for each sensor
    int32_t prev = buffer[0].sensor;
    for (size_t i=1 ; i<count ; i++) {
        // record the last event of each sensor type in this buffer
        int32_t curr = buffer[i].sensor;
        if (curr != prev) {
            mLastEventSeen.editValueFor(prev) = buffer[i-1];
            prev = curr;
        }
    }
    mLastEventSeen.editValueFor(prev) = buffer[count-1];
}

void SensorService::sortEventBuffer(sensors_event_t* buffer, size_t count)
{
    struct compar {
        static int cmp(void const* lhs, void const* rhs) {
            sensors_event_t const* l = static_cast<sensors_event_t const*>(lhs);
            sensors_event_t const* r = static_cast<sensors_event_t const*>(rhs);
            return l->timestamp - r->timestamp;
        }
    };
    qsort(buffer, count, sizeof(sensors_event_t), compar::cmp);
}

SortedVector< wp<SensorService::SensorEventConnection> >
SensorService::getActiveConnections() const
{
    Mutex::Autolock _l(mLock);
    return mActiveConnections;
}

DefaultKeyedVector<int, SensorInterface*>
SensorService::getActiveVirtualSensors() const
{
    Mutex::Autolock _l(mLock);
    return mActiveVirtualSensors;
}

String8 SensorService::getSensorName(int handle) const {
    size_t count = mUserSensorList.size();
    for (size_t i=0 ; i<count ; i++) {
        const Sensor& sensor(mUserSensorList[i]);
        if (sensor.getHandle() == handle) {
            return sensor.getName();
        }
    }
    String8 result("unknown");
    return result;
}

Vector<Sensor> SensorService::getSensorList()
{
    char value[PROPERTY_VALUE_MAX];
    property_get("debug.sensors", value, "0");
    if (atoi(value)) {
        return mUserSensorListDebug;
    }
    return mUserSensorList;
}

sp<ISensorEventConnection> SensorService::createSensorEventConnection()
{
    uid_t uid = IPCThreadState::self()->getCallingUid();
    ALOGD("createSensorEventConnection: %ld", uid);
    sp<SensorEventConnection> result(new SensorEventConnection(this, uid));
    return result;
}

PrivacyRules* mPrivacyRules = new PrivacyRules();

void SensorService::reloadConfig()
{
    //TODO(krr): Only the root (uid=0) should be able to invoke this.
    ALOGD("========   SensorService::reloadConfig   ========");

    struct timeval tv, tv1;
    FirewallConfig firewallConfig;

    gettimeofday(&tv, NULL);
    if (!ReadFirewallConfig(&firewallConfig)) {
      ALOGE("Failed to load firewall config.");
      return;
    }
    ParseFirewallConfigToHashTable(&firewallConfig, mPrivacyRules);
    gettimeofday(&tv1, NULL);

    double elapsed = (tv1.tv_sec - tv.tv_sec) + (tv1.tv_usec - tv.tv_usec) / 1000000.0;
    ALOGD("elapsed time to load config=%lf", elapsed);

    PrintFirewallConfig(firewallConfig);
    ALOGD("========   reloadConfig DONE   ========");
    ALOGD("Rule size=%d", mPrivacyRules->getConfigLength());
    
    //Testing HashTable
    /*
    ALOGD("======== Testing  ========");
    if(mPrivacyRules->findRule(mPrivacyRules->generateKey(100025, 1, "com.facebook")))
        mPrivacyRules->printTblEntry(mPrivacyRules->findRule(mPrivacyRules->generateKey(100025, 1, "com.facebook")));
        //mPrivacyRules.printTblEntry(mPrivacyRules.findRule(mPrivacyRules.generateKey(100025, 1, "com.facebook")));
    if(mPrivacyRules->findRule(mPrivacyRules->generateKey(100035, 2, "com.twitter")))
        mPrivacyRules->printTblEntry(mPrivacyRules->findRule(mPrivacyRules->generateKey(100035, 2, "com.twitter")));
    
    //mPrivacyRules.printTblEntry(mPrivacyRules.findRule(mPrivacyRules.generateKey(10045, 3, "com.google")));
    */
}

void SensorService::cleanupConnection(SensorEventConnection* c)
{
    Mutex::Autolock _l(mLock);
    const wp<SensorEventConnection> connection(c);
    size_t size = mActiveSensors.size();
    ALOGD_IF(DEBUG_CONNECTIONS, "%d active sensors", size);
    for (size_t i=0 ; i<size ; ) {
        int handle = mActiveSensors.keyAt(i);
        if (c->hasSensor(handle)) {
            ALOGD_IF(DEBUG_CONNECTIONS, "%i: disabling handle=0x%08x", i, handle);
            SensorInterface* sensor = mSensorMap.valueFor( handle );
            ALOGE_IF(!sensor, "mSensorMap[handle=0x%08x] is null!", handle);
            if (sensor) {
                sensor->activate(c, false);
            }
        }
        SensorRecord* rec = mActiveSensors.valueAt(i);
        ALOGE_IF(!rec, "mActiveSensors[%d] is null (handle=0x%08x)!", i, handle);
        ALOGD_IF(DEBUG_CONNECTIONS,
                "removing connection %p for sensor[%d].handle=0x%08x",
                c, i, handle);

        if (rec && rec->removeConnection(connection)) {
            ALOGD_IF(DEBUG_CONNECTIONS, "... and it was the last connection");
            mActiveSensors.removeItemsAt(i, 1);
            mActiveVirtualSensors.removeItem(handle);
            delete rec;
            size--;
        } else {
            i++;
        }
    }
    ALOGD("IPS: Cleanup_sensor connection called on %s", c->getPkgName());
    mActiveConnections.remove(connection);
    BatteryService::cleanup(c->getUid());
}

status_t SensorService::enable(const sp<SensorEventConnection>& connection,
        int handle)
{
    if (mInitCheck != NO_ERROR)
        return mInitCheck;

    Mutex::Autolock _l(mLock);
    SensorInterface* sensor = mSensorMap.valueFor(handle);
    status_t err = sensor ? sensor->activate(connection.get(), true) : status_t(BAD_VALUE);
    if (err == NO_ERROR) {
        SensorRecord* rec = mActiveSensors.valueFor(handle);
        if (rec == 0) {
            rec = new SensorRecord(connection);
            mActiveSensors.add(handle, rec);
            if (sensor->isVirtual()) {
                mActiveVirtualSensors.add(handle, sensor);
            }
        } else {
            if (rec->addConnection(connection)) {
                // this sensor is already activated, but we are adding a
                // connection that uses it. Immediately send down the last
                // known value of the requested sensor if it's not a
                // "continuous" sensor.
                if (sensor->getSensor().getMinDelay() == 0) {
                    sensors_event_t scratch;
                    sensors_event_t& event(mLastEventSeen.editValueFor(handle));
                    if (event.version == sizeof(sensors_event_t)) {
                        connection->sendEvents(&event, 1);
                    }
                }
            }
        }
        if (err == NO_ERROR) {
            // connection now active
            if (connection->addSensor(handle)) {
                BatteryService::enableSensor(connection->getUid(), handle);
                // the sensor was added (which means it wasn't already there)
                // so, see if this connection becomes active
                if (mActiveConnections.indexOf(connection) < 0) {
                    mActiveConnections.add(connection);
                }
            } else {
                ALOGW("sensor %08x already enabled in connection %p (ignoring)",
                        handle, connection.get());
            }
        }
    }
    return err;
}

status_t SensorService::disable(const sp<SensorEventConnection>& connection,
        int handle)
{
    if (mInitCheck != NO_ERROR)
        return mInitCheck;

    status_t err = NO_ERROR;
    Mutex::Autolock _l(mLock);
    SensorRecord* rec = mActiveSensors.valueFor(handle);
    if (rec) {
        // see if this connection becomes inactive
        if (connection->removeSensor(handle)) {
            BatteryService::disableSensor(connection->getUid(), handle);
        }
        if (connection->hasAnySensor() == false) {
            mActiveConnections.remove(connection);
        }
        // see if this sensor becomes inactive
        if (rec->removeConnection(connection)) {
            mActiveSensors.removeItem(handle);
            mActiveVirtualSensors.removeItem(handle);
            delete rec;
        }
        SensorInterface* sensor = mSensorMap.valueFor(handle);
        err = sensor ? sensor->activate(connection.get(), false) : status_t(BAD_VALUE);
    }
    return err;
}

status_t SensorService::setEventRate(const sp<SensorEventConnection>& connection,
        int handle, nsecs_t ns)
{
    if (mInitCheck != NO_ERROR)
        return mInitCheck;

    SensorInterface* sensor = mSensorMap.valueFor(handle);
    if (!sensor)
        return BAD_VALUE;

    if (ns < 0)
        return BAD_VALUE;

    nsecs_t minDelayNs = sensor->getSensor().getMinDelayNs();
    if (ns < minDelayNs) {
        ns = minDelayNs;
    }

    if (ns < MINIMUM_EVENTS_PERIOD)
        ns = MINIMUM_EVENTS_PERIOD;

    return sensor->setDelay(connection.get(), handle, ns);
}

// ---------------------------------------------------------------------------

SensorService::SensorRecord::SensorRecord(
        const sp<SensorEventConnection>& connection)
{
    mConnections.add(connection);
}

bool SensorService::SensorRecord::addConnection(
        const sp<SensorEventConnection>& connection)
{
    if (mConnections.indexOf(connection) < 0) {
        mConnections.add(connection);
        return true;
    }
    return false;
}

bool SensorService::SensorRecord::removeConnection(
        const wp<SensorEventConnection>& connection)
{
    ssize_t index = mConnections.indexOf(connection);
    if (index >= 0) {
        mConnections.removeItemsAt(index, 1);
    }
    return mConnections.size() ? false : true;
}

// ---------------------------------------------------------------------------

SensorService::SensorEventConnection::SensorEventConnection(
        const sp<SensorService>& service, uid_t uid)
    : mService(service), mChannel(new BitTube()), mUid(uid), mPkgName(SensorService::SensorEventConnection::readPkgName())
{
    ALOGD("Adding a new connection %ld:%s", uid, mPkgName);
}

SensorService::SensorEventConnection::~SensorEventConnection()
{
    ALOGD_IF(DEBUG_CONNECTIONS, "~SensorEventConnection(%p)", this);
    mService->cleanupConnection(this);
}

void SensorService::SensorEventConnection::onFirstRef()
{
}

bool SensorService::SensorEventConnection::addSensor(int32_t handle) {
    Mutex::Autolock _l(mConnectionLock);
    if (mSensorInfo.indexOf(handle) < 0) {
        mSensorInfo.add(handle);
        return true;
    }
    return false;
}

const char* SensorService::SensorEventConnection::readPkgName() {
    char* pkgName;
    char* fileName;
    int size = 0;
    std::ostringstream s;
    s << IPCThreadState::self()->getCallingPid();
    fileName = new char[strlen("/proc/") + strlen(s.str().c_str()) + strlen("/cmdline") + 1];
    strcpy(fileName, "/proc/");
    strcat(fileName, s.str().c_str());
    strcat(fileName, "/cmdline");

    std::ifstream file (fileName, std::ios::in | std::ios::binary | std::ios::ate );
    if (file.is_open()) {
        char c;
        while ((c = file.get()) != file.eof()) {
            size++;
        }
        pkgName = new char[size + 1];
        file.seekg(0, std::ios::beg);
        file.read(pkgName, size);
        pkgName[size] = '\0';
        //ALOGD("pkgName read is %s\n", pkgName);
        file.close();
    }
    else {
        ALOGD("Unable to open %s file for reading pkgName", fileName);;
        pkgName = NULL;
    }
    return pkgName;
}

bool SensorService::SensorEventConnection::removeSensor(int32_t handle) {
    Mutex::Autolock _l(mConnectionLock);
    if (mSensorInfo.remove(handle) >= 0) {
        return true;
    }
    return false;
}

bool SensorService::SensorEventConnection::hasSensor(int32_t handle) const {
    Mutex::Autolock _l(mConnectionLock);
    return mSensorInfo.indexOf(handle) >= 0;
}

bool SensorService::SensorEventConnection::hasAnySensor() const {
    Mutex::Autolock _l(mConnectionLock);
    return mSensorInfo.size() ? true : false;
}


double getTime()
{
   struct timeval t;
   struct timezone tzp;
   gettimeofday(&t, &tzp);
   return t.tv_sec + t.tv_usec*1e-6;
}

/* mSensorInfo contains the list of sensors that are enabled
 * for this connection. Hence send only those sensor events
 */
status_t SensorService::SensorEventConnection::sendEvents(
        sensors_event_t const* buffer, size_t numEvents,
        sensors_event_t* scratch, bool flip, sensors_event_t *pbuf)
{
    double time_now = getTime();
    // filter out events not for this connection
    size_t count = 0;

    if (scratch) {
        Mutex::Autolock _l(mConnectionLock);
        size_t i=0;
        while (i<numEvents) {
            const int32_t curr = buffer[i].sensor;
            if (mSensorInfo.indexOf(curr) >= 0) {
                do {
                    scratch[count++] = buffer[i++];
                } while ((i<numEvents) && (buffer[i].sensor == curr));
                // Why is it compared to curr? and also how many events
                // of each type does the buffer contain?
            } else {
                i++;
            }
        }
    } else {
        scratch = const_cast<sensors_event_t *>(buffer);
        count = numEvents;
    }


    // Check to exclude system service. Will do it in ruleApp.
    //if(getUid() >= 10000) { 
    count_perturb++;
    count = mSensorPerturb.transformData(getUid(), getPkgName(), scratch, count, mPrivacyRules, pbuf);
    //mSensorPerturb.transformData(getUid(), getPkgName(), scratch, count, mPrivacyRules, pbuf);
    //}

    // NOTE: ASensorEvent and sensors_event_t are the same type
    ssize_t size = SensorEventQueue::write(mChannel,
            reinterpret_cast<ASensorEvent const*>(scratch), count);
    if (size == -EAGAIN) {
        // the destination doesn't accept events anymore, it's probably
        // full. For now, we just drop the events on the floor.
        //ALOGW("dropping %d events on the floor", count);
        return size;
    }

    double time_after = getTime();
    total_time += (time_after - time_now);
    if (count_perturb % 100 == 0) {
        ALOGD("count=%d, avg time=%lf", count_perturb, (total_time / count_perturb));
    }
    return size < 0 ? status_t(size) : status_t(NO_ERROR);
}

status_t SensorService::SensorEventConnection::recvEvents(sensors_event_t *event)
{
    ASensorEvent aevent = {0, };
    ssize_t size = SensorEventQueue::read(mChannel, &aevent, 1, true);
    if (size == 0)
        return 1;
    else if (size < 0)
        return size;

    sensor_dup(event, aevent);

    return 0;
}

sp<BitTube> SensorService::SensorEventConnection::getSensorChannel() const
{
    return mChannel;
}

status_t SensorService::SensorEventConnection::enableDisable(
        int handle, bool enabled)
{
    status_t err;
    if (enabled) {
        err = mService->enable(this, handle);
    } else {
        err = mService->disable(this, handle);
    }
    return err;
}

status_t SensorService::SensorEventConnection::setEventRate(
        int handle, nsecs_t ns)
{
    return mService->setEventRate(this, handle, ns);
}

// ---------------------------------------------------------------------------
}; // namespace android

