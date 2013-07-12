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
#include <hardware/sensors.h>
#include <utils/Log.h>
#include <unistd.h>
#include "PrivacyRules.h"

namespace android {
// ---------------------------------------------------------------------------

    key_t PrivacyRules::generateKey(uid_t uid, int32_t type, String16 appName) {
        key_t temp; 
        temp.uid = uid;
        temp.type = type;
        temp.appName = appName;
        return temp;
    }
    
    // need to figure out a way to include appName
    // or to use a builtin hash function
    uint32_t PrivacyRules::hash_type(key_t key) { 
        uint32_t hash = 0;
        /*
        int len = strlen16(key.appName);
        String16* str = &key.appName;

        while (len--)
            hash = hash * 31 + *str++;
        */
        hash = hash + key.uid;
        //hash = hash + key.type;
        return hash;
    }

    size_t PrivacyRules::addRule(key_t key, privacy_vec_t value) {
       return rules.add(PrivacyRules::hash_type(key), key_value_pair_t<key_t,privacy_vec_t>(key, value));
    }

    const privacy_vec_t* PrivacyRules::findRule(key_t key) {
        ssize_t index = -1;
        index = rules.find(index, PrivacyRules::hash_type(key), key);
        if(index != -1)
            return &rules.entryAt(index).value;
        else
            return NULL;
    }

    bool PrivacyRules::removeRule(key_t key) {
        ssize_t index = rules.find(index, PrivacyRules::hash_type(key), key);
        if(index != -1) {
            rules.removeAt(index);
            return true;
        }
        return false;
    }

// ---------------------------------------------------------------------------
}; // namespace android
