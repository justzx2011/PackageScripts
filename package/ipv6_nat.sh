#!/bin/bash

# This is an example script for launching ipv6_nat

# You may need to change LANIF and WANIF.
LANIF="eth0"
WANIF=("eth1" "eth2")

echo 0 > /proc/sys/net/ipv6/conf/all/forwarding

for wan in ${WANIF[@]}
do
	ip6tables -D OUTPUT -p tcp --tcp-flags RST RST -j DROP -o $wan
	ip6tables -A OUTPUT -p tcp --tcp-flags RST RST -j DROP -o $wan
done

ip6tables -D OUTPUT -p icmpv6 --icmpv6-type neighbour-advertisement -j DROP -o $LANIF
ip6tables -D OUTPUT -p icmpv6 --icmpv6-type destination-unreachable -j DROP
ip6tables -A OUTPUT -p icmpv6 --icmpv6-type neighbour-advertisement -j DROP -o $LANIF
ip6tables -A OUTPUT -p icmpv6 --icmpv6-type destination-unreachable -j DROP

while [ 1 ]
do
	ipv6_nat $LANIF -w ${#WANIF[@]} ${WANIF[@]}
	sleep 5
done
