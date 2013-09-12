#ifndef ANDROID_FIREWALL_CONFIG_UTILS_INL_H
#define ANDROID_FIREWALL_CONFIG_UTILS_INL_H

#include <fstream>

#include <string>
#include <hash_map>

#include <utility>
#include <cutils/log.h>

#include "frameworks/native/services/sensorservice/FirewallConfigMessages.pb.h"
#include "PrivacyRules.h"

// ---------------------------------------------------------------------------

using android_sensorfirewall::FirewallConfig;
using android_sensorfirewall::Rule;
using android_sensorfirewall::Action;

const char* kFirewallConfigFilename = "/data/firewall-config";

namespace android_sensorfirewall {
// ---------------------------------------------------------------------------

bool ReadFileIntoString(const char* filename, std::string* contents) {
  std::string& local = *contents;
  std::ifstream ifs(filename, std::ios::in | std::ios::binary);
  if (!ifs) {
    ALOGE("Failed to open file %s", filename);
    return false;
  }

  // TODO(krr): Use fnctl to lock the file.

  ifs.seekg(0, std::ios::end);
  int filesize = ifs.tellg();

  if (filesize == 0)
    ALOGW("File is empty: %s", filename);
  local.resize(filesize);
  ifs.seekg(0, std::ios::beg);
  ifs.read(&local[0], local.size());
  ifs.close();
  return true;
}

bool WriteStringToFile(const char* filename, const std::string& data) {
    std::fstream ofs(filename, std::ios::out | std::ios::binary);
    if (!ofs) {
        ALOGE("Failed to open file %s", filename);
        return false;
    }

    ofs << data;
    ofs.close();
    return true;
}

bool ReadFirewallConfig(FirewallConfig* firewallConfig) {
  std::string data;
  if (!ReadFileIntoString(kFirewallConfigFilename, &data)) {
    ALOGE("Failed to read file contents into string.");
    return false;
  }

  if (!firewallConfig->ParseFromString(data)) {
    ALOGE("Failed to parse serialized string.");
    return false;
  }

  return true;
}

bool WriteFirewallConfig(const FirewallConfig& firewallConfig) {
    std::string data;
    if (!firewallConfig.SerializeToString(&data)) {
        ALOGE("Failed to serialize to string.");
        return false;
    }

    if (!WriteStringToFile(kFirewallConfigFilename, data)) {
      ALOGE("Failed to write serialized string to file.");
      return false;
    }

    return true;
}

void PrintFirewallConfig(const FirewallConfig& firewallConfig) {
    for (int ii = 0; ii < firewallConfig.rule_size(); ++ii) {
        const Rule& rule = firewallConfig.rule(ii);
        ALOGD("rule_name = %s: sensorType = %d: pkgName = %s: pkgUid = %d:",
            rule.rulename().c_str(), rule.sensortype(), rule.pkgname().c_str(),
            rule.pkguid());
        ALOGD("actionType = %d", rule.action().actiontype());
    }
}

void ParseFirewallConfigToHashTable(const FirewallConfig* firewallConfig, android::PrivacyRules* mPrivacyRules) {
    mPrivacyRules->clear();
    for (int i = 0; i < firewallConfig->rule_size(); i++) {
        const Rule& rule = firewallConfig->rule(i);
        const android::ruleKey_t* key = mPrivacyRules->generateKey(rule.pkguid(), rule.sensortype(), rule.pkgname().c_str());
        Rule* temp = new Rule();
        temp->CopyFrom(rule);
        mPrivacyRules->addRule(key, temp);
    }
    //ALOGD("============= All Rules Added =============");
}

// ---------------------------------------------------------------------------
}; // namespace android_sensorfirewall


#endif // ANDROID_FIREWALL_CONFIG_UTILS_INL_H
