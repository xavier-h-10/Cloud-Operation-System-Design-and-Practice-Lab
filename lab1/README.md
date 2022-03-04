# Lab 1 - Reliable Data Transport Protocol

### 519021910913 黄喆敏

#### xtommy@sjtu.edu.cn

### 1. 网络包设计

- 对于sender，包的结构如下：

  ```
  |<-  2 bytes  ->|<-  1 byte  ->|<-  4 bytes  ->|<-               the rest           ->|
  |   checksum    | payload size |   seq number  |                 payload              |
  ```

- 对于receiver，需要发送ack包，包的结构如下：

  ```
  |<-  2 bytes  ->|<-  4 bytes  ->|<-                        the rest                   ->|
  |   checksum    |   seq number  |                          payload                      |
  ```

### 2. sender 收包 & 发包机制

我们采用Go-Back-N的机制进行重发包。



### 3.receiver 收包 & 发包机制

当收到的包seq>ack时，我们会将该数据包存到buffer中。以便当后续收到seq=ack的包时，可以迅速移动滑动窗口，减少发包数量。

### 5. checksum计算方法

Checksum采用了tcp checksum的计算方法。首先将checksum位置0，将整个包划分成一个个16进制数，然后将这些数逐个相加，最后将得到的结果取反。由于我们的包长度为偶数，因此不需要考虑补0的问题。

对于一个packet，先将其他部分填入，最后再计算checksum。

代码如下所示：

```c++
unsigned short calc_checksum(struct packet *pkt) {
    long sum = 0;
    for (int i = 2; i < RDT_PKTSIZE; i += 2) {
        sum += *(unsigned short *) (&(pkt->data[i]));
    }
    while (sum >> 16) {
        sum = (sum & 0xffff) + (sum >> 16);
    }
    return ~sum;
}
```



### 测试结果

测试结果如下所示。对于文档中给的所有样例，均进行了20次以上的测试，没有发现错误。此外，采用了`rdt_sim 1000 0.1 100 0.7 0.7 0.7 0`，来测试错误概率较高的情况，也没有发现错误。

对于文档中给出参考吞吐量的两个样例，我们进行了如下测试，吞吐量在合理范围内。



### Reference

[1] TCP checksum的计算 https://blog.csdn.net/breakout_alex/article/details/102515371