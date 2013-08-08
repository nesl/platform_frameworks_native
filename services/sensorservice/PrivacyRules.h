#ifndef ANDROID_PRIVACY_RULES_H
#define ANDROID_PRIVACY_RULES_H

#include <stdint.h>
#include <sys/types.h>
#include <string>
#include <hash_map>
#include <utility>
#include "frameworks/native/services/sensorservice/FirewallConfigMessages.pb.h"


// ---------------------------------------------------------------------------
using namespace android_sensorfirewall;

namespace android {
// ---------------------------------------------------------------------------

typedef struct ruleKey{
    int uid;
    int type;
    char* pkgName;

    ruleKey(int mUid, int mType, const char* mPkgName) {
        uid = mUid;
        type = mType;
        pkgName = new char[strlen(mPkgName)+1];
        strcpy(pkgName, mPkgName);
    }

    ruleKey(const ruleKey& other) {
        uid = other.uid;
        type = other.type;
        pkgName = new char[strlen(other.pkgName)+1];
        strcpy(pkgName, other.pkgName);
    }

} ruleKey_t;

struct hashFunc {
    size_t operator() (const ruleKey_t& toSearchKey) const {
        int i;
        size_t hash = 0;

        int len = strlen(toSearchKey.pkgName);
        for(i = 0; i < len; i++) {
            hash = hash + toSearchKey.pkgName[i];
        }
        hash = hash + toSearchKey.uid;
        hash = hash + toSearchKey.type;
        return hash;
    }
};

struct eqFunc {
    bool operator()(const ruleKey_t& key1, const ruleKey_t& key2) const {
        if((key1.uid == key2.uid) && (key1.type == key2.type) && (strcmp(key1.pkgName, key2.pkgName) == 0))
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
    const ruleKey_t* generateKey(int uid, int type, const char* pkgName);
    void addRule(const ruleKey_t* key, const Rule* value);
    const Rule* findRule(const ruleKey_t* key);
    void clear();
    void printTblEntry(const Rule* rule);
};

}; // namespace android

#endif // ANDROID_PRIVACY_RULES_H
