答：

- 执行`ip link show eth0`指令，可以看到35是对应的index。

  ```bash
  root@two-containers:/# ip link show eth0
  35: eth0@if36: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1376 qdisc noqueue state UP mode DEFAULT group default
      link/ether 36:ef:06:ad:c8:28 brd ff:ff:ff:ff:ff:ff link-netnsid 0
  ```

- 接着退出到host，在host下执行指令，查看对应的veth interface。

  ```
  36: vethwepl551c56d@if35: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1376 qdisc noqueue master weave state UP mode DEFAULT group default
  ```

- 我们在host下使用`ip_addr`，结果如下所示：

  ```bash
  1: lo: <LOOPBACK,UP,LOWER_UP> mtu 65536 qdisc noqueue state UNKNOWN mode DEFAULT group default qlen 1000
      link/loopback 00:00:00:00:00:00 brd 00:00:00:00:00:00
  2: ens3: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1450 qdisc fq_codel state UP mode DEFAULT group default qlen 1000
      link/ether fa:16:3e:5e:52:bc brd ff:ff:ff:ff:ff:ff
  3: docker0: <NO-CARRIER,BROADCAST,MULTICAST,UP> mtu 1500 qdisc noqueue state DOWN mode DEFAULT group default
      link/ether 02:42:aa:b2:09:97 brd ff:ff:ff:ff:ff:ff
  4: datapath: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1376 qdisc noqueue state UNKNOWN mode DEFAULT group default qlen 1000
      link/ether 2a:f4:1f:36:ec:23 brd ff:ff:ff:ff:ff:ff
  6: weave: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1376 qdisc noqueue state UP mode DEFAULT group default qlen 1000
      link/ether 32:ed:4f:90:26:3a brd ff:ff:ff:ff:ff:ff
  8: vethwe-datapath@vethwe-bridge: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1376 qdisc noqueue master datapath state UP mode DEFAULT group default
      link/ether 46:21:b7:98:18:58 brd ff:ff:ff:ff:ff:ff
  9: vethwe-bridge@vethwe-datapath: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1376 qdisc noqueue master weave state UP mode DEFAULT group default
      link/ether 46:e2:9c:88:34:12 brd ff:ff:ff:ff:ff:ff
  10: vxlan-6784: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 65535 qdisc noqueue master datapath state UNKNOWN mode DEFAULT group default qlen 1000
      link/ether b6:3d:fb:1c:fa:02 brd ff:ff:ff:ff:ff:ff
  36: vethwepl551c56d@if35: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1376 qdisc noqueue master weave state UP mode DEFAULT group default
      link/ether b2:5e:14:aa:af:6b brd ff:ff:ff:ff:ff:ff link-netnsid 0
  ```

- weave 网络包含两个虚拟交换机： weave 和 datapath， vethwe-bridge 和 vethwe-datapath 将二者连接在一起；weave 和 datapath 分工不同，weave 负责将容器接入 weave 网络，datapath 负责在主机间vxlan 隧道中并收发数据。

- 因此，我们可以绘制下图。**其中红线代表从master节点使用cluster ip访问位于worker节点中的Pod的网络路径。**

  