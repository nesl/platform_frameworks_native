#include <hardware/sensors.h>
#include <utils/Log.h>
#include <unistd.h>
#include "frameworks/native/services/sensorservice/FirewallConfigMessages.pb.h"
#include "PrivacyRules.h"

namespace android {
// ---------------------------------------------------------------------------

    key_t* PrivacyRules::generateKey(int uid, int type, const char* appName) {
        key_t temp; 
        temp.uid = uid;
        temp.type = type;
        strcpy(temp.appName, appName);
        return &temp;
    }
    
    // need to figure out a way to include appName
    // or to use a builtin hash function
    uint32_t PrivacyRules::hash_type(const key_t* toSearchKey) { 
        int i;
        uint32_t hash = 0;
        //int len = strlen(toSearchKey->appName);
        int len = strlen(toSearchKey->appName);

        for(i = 0; i < len; i++) {
            hash = hash + toSearchKey->appName[i];
        }
        hash = hash + toSearchKey->uid;
        hash = hash + toSearchKey->type;
        return hash;
    }
    size_t PrivacyRules::addRule(const key_t* toSearchKey, Rule* value) {
       return rulesTbl.add(PrivacyRules::hash_type(toSearchKey), key_value_pair_t<key_t, Rule>(*toSearchKey, *value));
    }

    const Rule* PrivacyRules::findRule(const key_t* toSearchKey) {
        ssize_t index = -1;
        bool notFound = true;
        key_t temp;

        while(notFound) {
            index = rulesTbl.find(index, PrivacyRules::hash_type(toSearchKey), *toSearchKey);
            if(index != -1) {
                temp = rulesTbl.entryAt(index).key;
                if((temp.type == toSearchKey->type) && (temp.uid = toSearchKey->uid) && (strcmp(temp.appName, toSearchKey->appName) == 0)) {
                    notFound = false;
                }
            } 
            else
                return NULL;
        }
        return &rulesTbl.entryAt(index).value;
    }

    bool PrivacyRules::removeRule(const key_t* toSearchKey) {
        ssize_t index = rulesTbl.find(index, PrivacyRules::hash_type(toSearchKey), *toSearchKey);
        if(index != -1) {
            rulesTbl.removeAt(index);
            return true;
        }
        return false;
    }

// ---------------------------------------------------------------------------
}; // namespace android
