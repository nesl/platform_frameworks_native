#include <fstream>
#include <string>

#include <cutils/log.h>

#include "frameworks/native/services/sensorservice/FirewallConfigMessages.pb.h"

using android_sensorfirewall::FirewallConfig;
using android_sensorfirewall::Rule;
using android_sensorfirewall::Action;

const char* kFirewallConfigFileName = "/etc/firewall-config";

bool get_file_contents(const char *filename, std::string* contents) {
  std::string& local = *contents;
  std::ifstream ifs(filename, std::ios::in | std::ios::binary);
  if (!ifs)
    return false;
  ifs.seekg(0, std::ios::end);
  local.resize(ifs.tellg());
  ifs.seekg(0, std::ios::beg);
  ifs.read(&local[0], local.size());
  ifs.close();
  return true;
}

void PrintFirewallConfig(const FirewallConfig& firewallConfig) {
    int i;
    for (int ii = 0; ii < firewallConfig.rule_size(); ++ii) {
        const Rule& rule = firewallConfig.rule(ii);
        ALOGD("rule_name = %s: sensorType = %d: pkgName = %s: pkgUid = %d:",
            rule.rulename().c_str(), rule.sensortype(), rule.pkgname().c_str(),
            rule.pkguid());
        ALOGD("actionType = %d", rule.action().actiontype());
    }
}

int main(int argc, char** argv) {
    ALOGD("=-=-=-=-= FIREWALL CONFIG =-=-=-=-=");

    FirewallConfig firewallConfig_out;
    Rule* rule = NULL;
    Action* action = NULL;

    rule = firewallConfig_out.add_rule();
    rule->set_rulename("Rule1");
    rule->set_sensortype(1);
    rule->set_pkgname("Facebook.com");
    rule->set_pkguid(10025);
    action = rule->mutable_action();
    action->set_actiontype(Action::ACTION_SUPPRESS);

    rule = firewallConfig_out.add_rule();
    rule->set_rulename("Rule2");
    rule->set_sensortype(2);
    rule->set_pkgname("Twitter.com");
    rule->set_pkguid(10035);
    action = rule->mutable_action();
    action->set_actiontype(Action::ACTION_CONSTANT);

    ALOGD("====== Writing these rules to /etc/firewall-config====");
    PrintFirewallConfig(firewallConfig_out);

    std::fstream outputStream(kFirewallConfigFileName,
            std::ios::out | std::ios::binary);

    if (outputStream.fail()) {
        ALOGE("Failed to open file.");
        return -1;
    }

    std::string serialized_proto_out;
    if (!firewallConfig_out.SerializeToString(&serialized_proto_out)) {
        ALOGE("Failed to serialize to string.");
        return -1;
    }

    outputStream << serialized_proto_out;

    ALOGD("Flush and close %s", kFirewallConfigFileName);
    //outputStream << std::flush;
    outputStream.close();


    ALOGD("====== Reading rules from /etc/firewall-config====");
    std::fstream inputStream(kFirewallConfigFileName,
            std::ios::in | std::ios::binary);

    if (!inputStream.is_open()) {
        ALOGE("Failed to open file.");
        return -1;
    }

    //TODO(krr): lock the config file while reading via fnctl.

    std::string serialized_proto_in;
    if (!get_file_contents(kFirewallConfigFileName, &serialized_proto_in)) {
        ALOGE("Failed to read from file.");
        return -1;
    }

    FirewallConfig firewallConfig_in;
    if (!firewallConfig_in.ParseFromString(serialized_proto_in)) {
        ALOGE("Failed to parse string.");
        return -1;
    }

    inputStream.close();

    PrintFirewallConfig(firewallConfig_in);
    

    return 0;
}
