/ip address add address=172.16.2.29/24 interface=ether1;
/ip address add address=172.16.21.254/24 interface=ether2;
/ip route add dst-address=172.16.20.0/24 gateway=172.16.21.253;
/ip route add dst-address=0.0.0.0/0 gateway=172.16.21.254;
/ip route add dst-address=0.0.0.0/0 gateway=172.16.2.254;

