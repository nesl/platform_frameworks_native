#include <hardware/sensors.h>
#include <utils/Log.h>
#include <unistd.h>
#include "frameworks/native/services/sensorservice/FirewallConfigMessages.pb.h"
#include "PrivacyRules.h"

namespace android {
// ---------------------------------------------------------------------------

    PrivacyRules::PrivacyRules() {
        configLength = rulesTbl.size();
    }

    const key_t* PrivacyRules::generateKey(int uid, int type, const char* appName) {
        const key_t temp(uid, type, appName);
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
    size_t PrivacyRules::addRule(const key_t* toSearchKey, const Rule* value) {
       if(rulesTbl.add(PrivacyRules::hash_type(toSearchKey), key_value_pair_t<key_t, Rule>(*toSearchKey, *value))) {
           configLength = rulesTbl.size();
           return true;
       } 
       return false;
    }

    const Rule* PrivacyRules::findRule(const key_t* toSearchKey) {
        ssize_t index = -1;
        bool notFound = true;
        
        while(notFound) {
            index = rulesTbl.find(index, PrivacyRules::hash_type(toSearchKey), *toSearchKey);
            if(index != -1) {
                const key_t temp(rulesTbl.entryAt(index).key);
                if((temp.type == toSearchKey->type) && (temp.uid == toSearchKey->uid) && (strcmp(temp.appName, toSearchKey->appName) == 0)) {
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
            configLength = rulesTbl.size();
            return true;
        }
        return false;
    }
    void PrivacyRules::clear() {
        rulesTbl.clear();
    }

    size_t PrivacyRules::getConfigLength() {
        return configLength;
    }

    void PrivacyRules::printTable() {
        int i;
        for(i = 0; i < configLength; i++) {
            const Rule* rule = &rulesTbl.entryAt(i).value;
            const Action* action = &rule->action();
            ALOGD("ruleName = %s, sensorType = %d, pkgName = %s, pkgUid = %d, action=%d\n", rule->rulename().c_str(), rule->sensortype(), rule->pkgname().c_str(), rule->pkguid(), action->actiontype());
        }
    }

    

// ---------------------------------------------------------------------------
}; // namespace android
