#include <fstream>
#include <string>

#include <cutils/log.h>

#include <google/protobuf/io/zero_copy_stream_impl.h>

#include "frameworks/native/services/sensorservice/FirewallConfigMessages.pb.h"

using android_sensorfirewall::FirewallConfig;
using android_sensorfirewall::Rule;
using android_sensorfirewall::Action;

const char* kFirewallConfigFileName = "/etc/firewall-config";

int main(int argc, char** argv) {
    ALOGD("=-=-=-=-= FIREWALL CONFIG =-=-=-=-=");

    FirewallConfig firewallConfig;
    Rule* rule = NULL;
    Action* action = NULL;

    rule = firewallConfig.add_rule();
    rule->set_rulename("Rule1");
    rule->set_sensortype(1);
    rule->set_pkgname("Facebook.com");
    rule->set_pkguid(10025);
    action = rule->mutable_action();
    action->set_actiontype(Action::ACTION_SUPPRESS);

    rule = firewallConfig.add_rule();
    rule->set_rulename("Rule2");
    rule->set_sensortype(2);
    rule->set_pkgname("Twitter.com");
    rule->set_pkguid(10035);
    action = rule->mutable_action();
    action->set_actiontype(Action::ACTION_CONSTANT);

    //firewallConfig.set_debug_info("EUREKA! AND FOO BAR TO YOU.");

    std::fstream outputStream(kFirewallConfigFileName,
            std::ios::out | std::ios::binary);

    if (outputStream.fail()) {
        ALOGE("Failed to open file.");
        return -1;
    }

    // DebugString() is only supported with 'Message' not 'MessageLite'
    // ALOGD("Firewall config: %s", firewallConfig.DebugString().c_str());

    google::protobuf::io::OstreamOutputStream zerocopyOutputStream(&outputStream);
    if (!firewallConfig.SerializeToZeroCopyStream(&zerocopyOutputStream)) {
        ALOGE("Failed to serialize to file.");
        return -1;
    }

    ALOGD("Flush and close %s", kFirewallConfigFileName);
    outputStream << std::flush;
    outputStream.close();

    return 0;
}
