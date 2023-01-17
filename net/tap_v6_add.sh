ip tuntap add mode tap user yama name tap1
ip addr add 2001:db8::1/32 dev tap1
ip link set tap1 up