#!/bin/sh
#
# create_iptables.sh
#
# Usage: source this file, and run /sbin/service iptables restart
#
#############################################################################

# declare variables
EXT_IF="eth0"
LOOPBACK="lo"
IPTABLES="/sbin/iptables"
MKDIR="/bin/mkdir"
CP="/bin/cp"
TIMESTAMP=`/bin/date +%m%d%Y_%H%M%S`
BACKUPS_DIR="/etc/sysconfig/iptables_backups"

# Hosts
HOSTA="10.10.10.2"
HOSTB="10.10.10.3"

# Set up kernel
echo 1 > /proc/sys/net/ipv4/tcp_syncookies


# Flush (-F) all specific rules
${IPTABLES} -F INPUT
${IPTABLES} -F FORWARD
${IPTABLES} -F OUTPUT
${IPTABLES} --delete-chain


# build ${IPTABLES}
${IPTABLES} -P INPUT DROP
${IPTABLES} -P OUTPUT ACCEPT

## Filtering
# Allow all inputs to firewall from the internal network and local interfaces
${IPTABLES} -A INPUT -i ${LOOPBACK} -s 0/0 -d 0/0 -j ACCEPT
${IPTABLES} -A INPUT -i ${EXT_IF} -p icmp --icmp-type 8 -s 0/0 -d 0/0 -j ACCEPT
${IPTABLES} -A INPUT -m state --state ESTABLISHED,RELATED -j ACCEPT


# Allow remote connections
${IPTABLES} -A INPUT -i ${EXT_IF} -p tcp -m state --state NEW -d ${HOSTA} --dport 22 -j ACCEPT
${IPTABLES} -A INPUT -i ${EXT_IF} -s ${HOSTB} -d ${HOSTA} -j ACCEPT
${IPTABLES} -A INPUT -i ${EXT_IF} -p tcp -m state --state NEW -d 0/0 --dport 3000 -j ACCEPT
${IPTABLES} -A INPUT -i ${EXT_IF} -p udp -m state --state NEW -d 0/0 --dport 3000 -j ACCEPT
${IPTABLES} -A INPUT -i ${EXT_IF} -p tcp -m state --state NEW -d ${HOSTA} --dport 6000 -j ACCEPT

# Deny any packet coming in on the public internet interface eth0
# which has a spoofed source address from our local networks:
${IPTABLES} -A INPUT -i ${EXT_IF} -s 192.168.1.0/24 -j DROP
${IPTABLES} -A INPUT -i ${EXT_IF} -s 127.0.0.0/8 -j DROP
${IPTABLES} -A INPUT -i ${EXT_IF} -d 224.0.0.0/8 -j DROP
${IPTABLES} -A INPUT -i ${EXT_IF} -p udp --dport 137 -j DROP
${IPTABLES} -A INPUT -i ${EXT_IF} -p udp --dport 138 -j DROP

# Drop bogus packets lying about their state
${IPTABLES} -A INPUT -p tcp ! --syn -m state --state NEW -j DROP

# Drop noise
${IPTABLES} -A INPUT -i ${EXT_IF} -s 10.10.10.250 -d 255.255.255.255 -j DROP
${IPTABLES} -A INPUT -i ${EXT_IF} -s 10.10.10.251 -d 255.255.255.255 -j DROP

# Log dropped packets
${IPTABLES} -A INPUT -j LOG --log-prefix "** Firewall Deny **" --log-tcp-options --log-ip-options
${IPTABLES} -A INPUT -j DROP
if test ! -d ${BACKUPS_DIR}
then
        ${MKDIR} -p ${BACKUPS_DIR}
fi
${CP} -r /etc/sysconfig/iptables ${BACKUPS_DIR}/iptables.${TIMESTAMP}
${IPTABLES}-save > /etc/sysconfig/iptables
/sbin/service iptables restart
