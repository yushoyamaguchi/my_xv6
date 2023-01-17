ip link set tap1 up
ip addr del 2001:db8::1/32 dev tap1
ip tuntap del mode tap name tap1