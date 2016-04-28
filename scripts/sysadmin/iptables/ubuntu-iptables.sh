#!/bin/sh
#
#
#############################################################################

# declare variables
MGMT="em1"
LOOPBACK="lo"
IPTABLES="/sbin/iptables"
MKDIR="/bin/mkdir"
CP="/bin/cp"
TIMESTAMP=`/bin/date +%m%d%Y_%H%M%S`
BACKUPS_DIR="/etc/iptables_backups"

# Hosts
SELF="192.168.1.1"
OTHER="192.168.2.1"

# Set up kernel
echo 0 > /proc/sys/net/ipv4/ip_forward

# Flush (-F) all specific rules
${IPTABLES} -F INPUT
${IPTABLES} -F FORWARD
${IPTABLES} -F OUTPUT
${IPTABLES} -F -t nat
${IPTABLES} --delete-chain

# build ${IPTABLES}
${IPTABLES} -P INPUT DROP
${IPTABLES} -P FORWARD DROP
${IPTABLES} -P OUTPUT ACCEPT

## Filtering
${IPTABLES} -A INPUT -i ${LOOPBACK} -s 0/0 -d 0/0 -j ACCEPT
${IPTABLES} -A INPUT -m state --state ESTABLISHED,RELATED -j ACCEPT

# Allow remote connections
${IPTABLES} -A INPUT -i ${MGMT} -p all -s ${OTHER} -d ${SELF} -j ACCEPT

# Log dropped packets
${IPTABLES} -A INPUT -j LOG --log-prefix "** Firewall Deny **" --log-tcp-options --log-ip-options
${IPTABLES} -A INPUT -j DROP

# Save rules to live ruleset
if test ! -d ${BACKUPS_DIR}
then
        ${MKDIR} -p ${BACKUPS_DIR}
fi
${CP} -r /etc/iptables.rules ${BACKUPS_DIR}/iptables.${TIMESTAMP}
${IPTABLES}-save > /etc/iptables.rules
