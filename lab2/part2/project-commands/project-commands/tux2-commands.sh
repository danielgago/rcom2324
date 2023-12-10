ifconfig eth0 172.16.21.1/24 up
route add -net 172.16.20.0/24 gw 172.16.21.253
route add default gw 172.16.21.254
sysctl net.ipv4.conf.eth0.accept_redirects=0
sysctl net.ipv4.conf.all.accept_redirects=0

