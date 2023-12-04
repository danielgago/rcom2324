# Configuration and Study of a Network

## Experience 1 - Configure an IP Network

### Steps

#### 1. Disconnect the switch from netlab (PY.1). Connect tuxY3 and tuxY4 to the switch

![image](imgs/i1.png)

#### 2. Configure tuxY3 and tuxY4 using ifconfig and route commands

- Reset the Mikrotik switch
    - Open GTKterm
    - Set Baudrate to 115200
    - Username - admin
    - Password - (blank)
    - /system reset-configuration
        - y
        - [ENTER]

- Configure tuxY3
``` bash
ifconfig eth0 up
ifconfig eth0 172.16.20.1/24
```

- Configure tuxY4
``` bash
ifconfig eth0 up
ifconfig eth0 172.16.20.254/24
```

#### 3. Register the IP and MAC addresses of network interfaces

- tuxY3:
![image](imgs/i2.png)

- tuxY4:
![image](imgs/i3.png)

#### 4. Use ping command to verify connectivity between these computers

- tuxY3:
``` bash
ping 172.16.20.254
```

#### 5. Inspect forwarding (route -n) and ARP (arp -a) tables
#### 6. Delete ARP table entries in tuxY3 (arp -d ipaddress)

- tuxY3:
```
arp -a
arp -d [INSERT IP HERE]
```

#### 7. Start Wireshark in tuxY3.eth0 and start capturing packets

- tuxY3:
    - Start **Wireshark**
    - Double click eth0

#### 8. In tuxY3, ping tuxY4 for a few seconds

``` bash
ping 172.16.20.254
```

#### 9. Stop capturing packets

#### 10. Save the log and study it at home

- Available [here](logs/log1.pcapng).

### Questions

#### What are the commands required to configure this experience?

#### What are the ARP packets and what are they used for?

#### What are the MAC and IP addresses of ARP packets and why?

#### What packets does the ping command generate?

#### What are the MAC and IP addresses of the ping packets?

#### How to determine if a receiving Ethernet frame is ARP, IP, ICMP?

#### How to determine the length of a receiving frame?

#### What is the loopback interface and why is it important?

## Experience 2 - Implement two bridges in a switch

### Steps

#### 1. Connect and configure tuxY2 and register its IP and MAC addresses

![image](imgs/i4.png)

- Configure tuxY2
``` bash
ifconfig eth0 up
ifconfig eth0 172.16.21.1/24
```

#### 2. Create two bridges in the switch: bridgeY0 and bridgeY1

- Mikrotik:
```
/interface bridge add name=bridgeY0
/interface bridge add name=bridgeY1
/interface bridge print
```
![image](imgs/i5.png)

#### 3. Remove the ports where tuxY3, tuxY4 and tuxY2 are connected from the default bridge (bridge) and add them the corresponding ports to bridgeY0 and bridgeY1

- Mikrotik:
```
/interface bridge port print brief
/interface bridge port remove [find interface=ether1]
/interface bridge port remove [find interface=ether2]
/interface bridge port remove [find interface=ether4]
/interface bridge port add bridge=bridgeY0 interface=ether1
/interface bridge port add bridge=bridgeY0 interface=ether2
/interface bridge port add bridge=bridgeY1 interface=ether4
```

![image](imgs/i6.png)

#### 4. Start the capture at tuxY3.eth0

- tuxY3:
    - Start **Wireshark**
    - Double click eth0

#### 5. In tuxY3, ping tuxY4 and then ping tuxY2

- tuxY3:
``` bash
ping 172.16.20.254
ping 172.16.21.1
```

#### 6. Stop the capture and save the log

#### 7. Start new captures in tuxY2.eth0, tuxY3.eth0, tuxY4.eth0
#### 8. In tuxY3, do ping broadcast for a few seconds

- tuxY2/tuxY3/tuxY4:
    - Start **Wireshark**
    - Double click eth0

- tuxY3:
``` bash
ping -b 172.16.21.255
```

#### 9. Observe the results, stop the captures and save the logs

#### 10. Repeat steps 7, 8 and 9, but now ping broadcast in tuxY2 (ping -b 172.16.Y1.255)

- tuxY2/tuxY3/tuxY4:
    - Start **Wireshark**
    - Double click eth0

- tuxY2:
``` bash
ping -b 172.16.21.255
```

### Questions

#### How to configure bridgeY0?

#### How many broadcast domains are there? How can you conclude it from the logs?

## Experience 3 - Configure a Router in Linux

### Steps

#### 1. Transform tuxY4 (Linux) into a router

##### Configure also tuxY4.eth1 and add it to bridgeY1

![image](imgs/i7.png)

- Configure tuxY4.eth1:
``` bash
ifconfig eth1 down
ifconfig eth1 172.16.21.253/24
```

- Mikrotik
```
/interface bridge port remove [find interface=ether3]
/interface bridge port add bridge=bridgeY1 interface=ether3
```

##### Enable IP forwarding
``` bash
sysctl net.ipv4.ip_forward=1
```

##### Disable ICMP 
echo-ignore-broadcast
``` bash
sysctl net.ipv4.icmp_echo_ignore_broadcasts=0
```

#### 2. Observe MAC addresses and IP addresses in tuxY4.eth0 and tuxY4.eth1

![image](imgs/i11.png)

#### 3. Reconfigure tuxY3 and tuxY2 so that each of them can reach the other

No tuxY2:
```
route add -net 172.16.21.0/24 gw 172.16.20.254
```

No tuxY2:
```
route add -net 172.16.20.0/24 gw 172.16.21.253
```

#### 4. Observe the routes available at the 3 tuxes (route -n)

tuxY2:
![image](imgs/i8.png)

tuxY3:
![image](imgs/i9.png)

tuxY4:
![image](imgs/i10.png)

#### 5. Start capture at tuxY3

- tuxY3:
    - Start **Wireshark**
    - Double click eth0

#### 6. From tuxY3, ping the other network interfaces (172.16.Y0.254, 172.16.Y1.253, 172.16.Y1.1) and verify if there is connectivity

- tuxY3:
``` bash
ping 172.16.20.254
ping 172.16.21.253
ping 172.16.21.1
```

#### 7. Stop the capture and save the logs

#### 8. Start capture in tuxY4; use 2 instances of Wireshark, one per network interface

- tuxY4:
    - Start **Wireshark**
    - Double click eth0
    - Start another instance of **Wireshark**
    - Double click eth1

#### 9. Clean the ARP tables in the 3 tuxes

```
arp -a
arp -d [INSERT IP HERE]
```

#### 10. In tuxY3, ping tuxY2 for a few seconds.

- tuxY3:
``` bash
ping 172.16.21.1
```

#### 11. Stop captures in tuxY4 and save logs

### Questions

#### What are the commands required to configure this experience?

#### What routes are there in the tuxes? What are their meaning?

#### What information does an entry of the forwarding table contain?

#### What ARP messages, and associated MAC addresses, are observed and why?

#### What ICMP packets are observed and why?

#### What are the IP and MAC addresses associated to ICMP packets and why? 