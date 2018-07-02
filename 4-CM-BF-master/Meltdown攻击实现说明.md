# Meltdown攻击实现说明

标签（空格分隔）： meltdown 攻击

---

[TOC]

##实现原理
```
mov rax byte[x]
shl rax 12
mov rbx pages[rax]
```
观察以上代码，由于cpu的预测实现，在第一行代码抛出异常前先预测执行了接下来几行代码。
虽然异常处理后rax会消失，可以由于执行了第三行代码，因此第二行代码：rax*4096，即pages数组中的第rax块位置被调入了cache，因此，此块的访问时间会显著少于pages中其他块的访问时间。
此块的块号就是访问到的信息。

##攻击步骤
* 设置好基本异常抛出信号，和设置单cpu运作
* 首先测试本台电脑访问cache和访问内存的时间（实际上只需要访问cache所需的时间，但要了解两者时间差来做一个最大cache访问时间的评估）。根据测试结果，访问内存几乎比访问cache慢了10倍。因此取多次访问cache的平均时间的两倍作为cache访问的判定上限（即访问块所花时间小于这个上限则可认为访问的是cache）。
* 然后开始攻击：1.将pages数组全部刷出cache。
* 2.由于这是一个抢时间的漏洞，所以我们不能让我们访问的内容真正的在内存中，即在把它泄漏出来前，先非法访问一次，让其进入cache中（accelerate），这样才能及时在抛出异常前拿到数据，然后把数据值对应的pages块放到cache中。
* 3.执行实现原理中的汇编代码（使用c语言中的内联汇编，格式有所差异），把pages块放到cache中。
* 4.之后进行pages的遍历，判断哪个块在cache中，这个块号即是我们想要的信息x。因为由于是拼时间的漏洞，不是每一次都成功，因此，对应每个信息，我们都会进行多次访问，确保成功泄漏。
* 5.输出信息

##期望结果

![image_1chamcti51pt019ctlpfve2bao9.png-176.7kB][1]
从图片中可以看到，开头几个字符为%s version %s (build...
本程序目的是泄漏version信息

##测试过程
```
$make
$./run.sh # 此脚本引用自：https://gitee.com/idea4good/meltdown/blob/master/run.sh
用于查找version banner的物理地址
```

##测试环境：Ubuntu 16.02（禁用PTI）

#####作者：PB16060674-归舒睿


  [1]: http://static.zybuluo.com/Citrine/vvvp7ah0tnq4yz4qk99uqbcj/image_1chamcti51pt019ctlpfve2bao9.png