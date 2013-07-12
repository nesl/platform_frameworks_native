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

#ifndef ANDROID_PRIVACY_RULES_H
#define ANDROID_PRIVACY_RULES_H

#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <utils/BasicHashtable.h>
#include <utils/String16.h>
#include "SensorPerturb.h"

// ---------------------------------------------------------------------------

namespace android {
// ---------------------------------------------------------------------------

typedef struct ruleKey {
    uid_t uid;
    int32_t type;
    char appName[32];

    bool operator ==(const ruleKey& other) const {
        if((other.uid == uid) && (other.type == type) && (strcmp(other.appName, appName) == 0))
            return true;
        else
            return false;
    }
    bool operator !=(const ruleKey& other) const {
        if((other.uid == uid) && (other.type == type) && (strcmp(other.appName, appName) == 0))
            return false;
        else
            return true;
    }
} key_t;

typedef key_value_pair_t<key_t, privacy_vec_t> entry;
typedef BasicHashtable<key_t, entry> RulesHashTable;

class PrivacyRules {
RulesHashTable rules;
public:
    key_t generateKey(uid_t uid, int32_t type, const char* appName);
    size_t addRule(key_t key, privacy_vec_t value);
    const privacy_vec_t* findRule(key_t key);
    bool removeRule(key_t key);
    uint32_t hash_type(key_t key);
};

}; // namespace android

#endif // ANDROID_PRIVACY_RULES_H
