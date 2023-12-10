ifconfig eth0 172.16.20.1/24 up
route add -net 172.16.21.0/24 gw 172.16.20.254
route add default gw 172.16.20.254

