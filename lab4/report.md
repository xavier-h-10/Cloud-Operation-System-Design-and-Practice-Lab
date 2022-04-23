# Kubernetes实践 实验报告

### 519021910913 黄喆敏

**Q1**: 请记录所有安装步骤的指令，并简要描述其含义。

答：



**Q2**: 在两个节点上分别使用`ps aux | grep kube`列出所有和k8s相关的进程，记录其输出，并简要说明各个进程的作用。

答：



**Q3**: 在两个节点中分别使用显示所有正常运行的docker容器，记录其输出，并简要说明各个容器所包含的k8s组件，以及那些k8s组件未运行在容器中。

答：



**Q4**: 请采用声明式接口对Pod进行部署，并将部署所需的yml文件记录在实践文档中。

答：



**Q5**: 请在worker节点上，在部署Pod的前后分别采用`docker ps`查看所有运行中的容器并对比两者的区别。请将创建该Pod所创建的全部新容器列举出来，并一一解释其作用。

答：



**Q6**: 请结合博客 https://blog.51cto.com/u_15069443/4043930 的内容，将容器中的veth与host机器上的veth匹配起来，并采用ip link 和 ip addr指令找到位于host机器中的所有网络设备及其之间的关系。结合两者的输出，试绘制出worker节点中涉及新部署Pod的所有网络设备和其网络结构，并在图中 标注出从master节点中使用cluster ip访问位于worker节点中的Pod的网络路径。

答：



**Q7**: 请采用声明式接口对Service进行部署，并将部署所需的yml文件记录在实践文档中。

答：



**Q8**: 请在master节点中使用`iptables-save`指令输出所有的iptables规则，将其中与Service访问相关的iptable规则记录在实践文档中，并解释网络流量是如何采用基于iptables的方式被从对Service的cluster IP的访问定向到实际的Pod中的。

答：



**Q9**: kube-proxy组件在整个service的定义与实现过程中起到了什么作用?请自行查找资料，并解释在iptables模式下，kube-proxy的功能。

答：



**Q10**: 请采用声明式接口对Deployment进行部署，并将Deployment所需要的yml文件记录在文档中。

答：



**Q11**: 请在master节点中使用`iptables-save`指令输出所有的iptables规则，将其中与Service访问相关的iptable规则记录在实践文档中，并解释网络流量是如何采用基于iptable的方式被从对Service的cluster ip的访问，以随机且负载均衡的方式，定向到Deployment所创建的实际的Pod中的。

答：



**Q12**: 在该使用Deployment的部署方式下，不同Pod之间的文件是否共享？该情况会在实际使用文件下载与共享服务时产生怎样的实际效果与问题？应如何解决这一问题？

答：

