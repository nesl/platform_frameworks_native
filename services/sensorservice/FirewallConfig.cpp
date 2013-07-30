#include <string>
#include <cutils/log.h>

#include "FirewallConfigUtils-inl.h"
#include "frameworks/native/services/sensorservice/FirewallConfigMessages.pb.h"

using namespace android_sensorfirewall;
using namespace std;

void MakeTestFirewallConfig(FirewallConfig* firewallConfig) {
    Rule* rule = NULL;
    Action* action = NULL;

    rule = firewallConfig->add_rule();
    rule->set_rulename("Rule1");
    rule->set_sensortype(1);
    rule->set_pkgname("Facebook.com");
    rule->set_pkguid(10025);
    action = rule->mutable_action();
    action->set_actiontype(Action::ACTION_SUPPRESS);

    rule = firewallConfig->add_rule();
    rule->set_rulename("Rule2");
    rule->set_sensortype(2);
    rule->set_pkgname("Twitter.com");
    rule->set_pkguid(10035);
    action = rule->mutable_action();
    action->set_actiontype(Action::ACTION_CONSTANT);
}

int main(int argc, char** argv) {
    ALOGD("=-=-=-=-= FIREWALL CONFIG =-=-=-=-=");
    FirewallConfig firewallConfig_out;
    MakeTestFirewallConfig(&firewallConfig_out);

    ALOGD("====== Writing these rules to /etc/firewall-config====");
    PrintFirewallConfig(firewallConfig_out);
    if (!WriteFirewallConfig(firewallConfig_out)) {
      ALOGE("Failed to write firewall config, returning.");
      return -1;
    }

    ALOGD("====== Reading rules from /etc/firewall-config====");
    FirewallConfig firewallConfig_in;
    if (!ReadFirewallConfig(&firewallConfig_in)) {
      ALOGE("Failed to read firewall config, returning.");
      return -1;
    }

    PrintFirewallConfig(firewallConfig_in);

    ALOGD("=-=-=-=-= DONE =-=-=-=-=");

    return 0;
}
