ifconfig eth0 172.16.20.254/24 up
ifconfig eth1 172.16.21.253/24 up
sysctl net.ipv4.ip_forward=1
sysctl net.ipv4.icmp_echo_ignore_broadcasts=0
route add default gw 172.16.21.254

