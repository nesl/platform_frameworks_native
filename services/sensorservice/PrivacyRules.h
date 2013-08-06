#ifndef ANDROID_PRIVACY_RULES_H
#define ANDROID_PRIVACY_RULES_H

#include <stdint.h>
#include <sys/types.h>
#include <string>
#include <hash_map>
#include <utility>
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
        appName = new char[strlen(other.appName)+1];
        strcpy(appName, other.appName);
    }

} ruleKey_t;

struct hashFunc {
    size_t operator() (const ruleKey_t& toSearchKey) const {
        int i;
        size_t hash = 0;

        int len = strlen(toSearchKey.appName);
        for(i = 0; i < len; i++) {
            hash = hash + toSearchKey.appName[i];
        }
        hash = hash + toSearchKey.uid;
        hash = hash + toSearchKey.type;
        return hash;
    }
};

struct eqFunc {
    bool operator()(const ruleKey_t& key1, const ruleKey_t& key2) const {
        if((key1.uid == key2.uid) && (key1.type == key2.type) && (strcmp(key1.appName, key2.appName) == 0))
            return true;
        else
            return false;
    }
};

typedef std::hash_map<ruleKey_t, Rule, hashFunc, eqFunc> firewallConfigTable;

class PrivacyRules {
firewallConfigTable* rulesTbl;
size_t configLength;

public:
    PrivacyRules();
    size_t getConfigLength();
    const ruleKey_t* generateKey(int uid, int type, const char* appName);
    void addRule(const ruleKey_t* key, const Rule* value);
    const Rule* findRule(const ruleKey_t* key);
    void clear();
    void printTblEntry(const Rule* rule);
};

}; // namespace android

#endif // ANDROID_PRIVACY_RULES_H
