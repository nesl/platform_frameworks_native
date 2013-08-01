
#ifndef ANDROID_PRIVACY_RULES_H
#define ANDROID_PRIVACY_RULES_H

#include <stdint.h>
#include <sys/types.h>
#include <utils/BasicHashtable.h>
#include "frameworks/native/services/sensorservice/FirewallConfigMessages.pb.h"


// ---------------------------------------------------------------------------
using namespace android_sensorfirewall;

namespace android {
// ---------------------------------------------------------------------------

typedef struct ruleKey{
    int uid;
    int type;
    char* appName;

    ruleKey(int mUid, int mType, const char* mAppName) {
        uid = mUid;
        type = mType;
        appName = new char[strlen(mAppName)+1];
        strcpy(appName, mAppName);
    }

    ruleKey(const ruleKey& other) {
        uid = other.uid;
        type = other.type;
        appName = other.appName;
    }

    bool operator ==(const ruleKey& other) const {
        if((other.uid == uid) && (other.type == type) && (strcmp(appName, other.appName) == 0))
            return true;
        else
            return false;
    }
    bool operator !=(const ruleKey& other) const {
        if((other.uid == uid) && (other.type == type) && (strcmp(appName, other.appName) == 0))
            return false;
        else
            return true;
    }
} key_t;

typedef key_value_pair_t<key_t, Rule> entry;
typedef BasicHashtable<key_t, entry> firewallConfigTable;

class PrivacyRules {
firewallConfigTable rulesTbl;
size_t configLength;
public:
    PrivacyRules();
    const key_t* generateKey(int uid, int type, const char* appName);
    uint32_t hash_type(const key_t* key);

    size_t addRule(const key_t* key, const Rule* value);
    const Rule* findRule(const key_t* key);
    bool removeRule(const key_t* key);
    void clear();
    size_t getConfigLength();
    void printTable();
};

}; // namespace android

#endif // ANDROID_PRIVACY_RULES_H
