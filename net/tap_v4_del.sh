ip link set tap0 up
ip addr del 192.0.2.1/24 dev tap0
ip tuntap del mode tap name tap0