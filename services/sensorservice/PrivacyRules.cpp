#include <hardware/sensors.h>
#include <utils/Log.h>
#include <unistd.h>
#include "frameworks/native/services/sensorservice/FirewallConfigMessages.pb.h"
#include "PrivacyRules.h"

namespace android {
// ---------------------------------------------------------------------------

    PrivacyRules::PrivacyRules() {
        rulesTbl = new firewallConfigTable();
        configLength = rulesTbl->size();
    }

    size_t PrivacyRules::getConfigLength() {
        return configLength;
    }

    const ruleKey_t* PrivacyRules::generateKey(int uid, int type, const char* pkgName) {
        const ruleKey_t* temp = new ruleKey_t(uid, type, pkgName);
        return temp;
    }
    
    void PrivacyRules::addRule(const ruleKey_t* key, const Rule* value) {
        ALOGD("Inserting ruleName = %s\n", value->rulename().c_str());
        rulesTbl->insert(std::make_pair(*key, *value));
        configLength = rulesTbl->size();
        ALOGD("Number of entries in the table = %d\n", configLength);
    }

    const Rule* PrivacyRules::findRule(const ruleKey_t* toSearchKey) {
        firewallConfigTable::iterator mIter;
        mIter = rulesTbl->find(*toSearchKey);
        if(mIter == rulesTbl->end()) {
            //ALOGD("No Match Found");
            return NULL;
        }
        else {
            //ALOGD("Match Found");
            return &mIter->second;
        }
    }

    void PrivacyRules::clear() {
        if(rulesTbl->size() != 0) {
            rulesTbl->clear();
            configLength = rulesTbl->size();
        }
    }

    void PrivacyRules::printTblEntry(const Rule* rule) {
        if(rule) {
                const Action* action = &rule->action();
                if(action) {
                    ALOGD("ruleName = %s, sensorType = %d, pkgName = %s, pkgUid = %d, action=%d\n", rule->rulename().c_str(), rule->sensortype(), rule->pkgname().c_str(), rule->pkguid(), action->actiontype());
                }
                else {
                    ALOGE("Malformed Rule: action is not set");
                }
        }
        else {
             ALOGE("Malformed Rule: action is not set");
        }
     }
// ---------------------------------------------------------------------------
}; // namespace android
