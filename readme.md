## 程序的原理&攻击步骤

请看下面代码

> 1:  clflush for user_probe[]; // 把user_probe_addr对应的cache全部都flush掉
>
> 2:  u8 index = *(u8 *) attacked _mem_addr; // attacked_mem_addr存放被攻击的地址
>
> 3:  data = user_probe_addr[index * 4096]; // user_probe_addr存放攻击者可以放访问的基地址 
>

 第1行, 把user_probe_addr数组对应的cache全部清除掉。

 第2行, 我们设法访问到attacked_mem_addr中的内容. 由于CPU权限的保护, 我们不能直接取得里面的内容, 但是可以利用它来造成我们可以观察到影响。

 第3行, 我们用访问到的值做偏移, 以4096为单位, 去访问攻击者有权限访问的数组,这样对应偏移处的内存就可以缓存到CPU cache里。

 这样, 虽然我们在第2行处拿到的数据不能直接看到, 但是它的值对CPU cache已经造成了影响。

 接下来可以利用CPU cache来间接拿到这个值. 我们以4096为单位依次访问user_probe_addr对应内存单位的前几个字节,  并且测量这该次内存访问的时间, 就可以观察到时间差异, 如果访问时间短, 那么可以推测访内存已经被cache,  可以反推出示例代码中的index的值。

 在这个例子里, 之所以用4096字节做为访问单位是为了避免内存预读带来的影响, 因为CPU在每次从主存访问内存的时候, 根据局部性原理, 有可能将邻将的内存也读进来。Intel的开发手册上指明 CPU的内存预取不会跨页面, 而每个页面的大小是4096。

[Meltdown](https://meltdownattack.com/meltdown.pdf)论文中给出了他们所做实验的结果, 引用如下：

![image](http://github.com/leitScarlett/4-leitScarlett/raw/master/paper expected result.png)

据此, 他们反推出index的值为84。



如果attached_mem_addr位于内核, 我们就可以利用它来读取内核空间的内容。

如果CPU顺序执行, 在第2行就会发现它访问了一个没有权限地址, 产生page fault (缺页异常), 进而被内核捕获, 第3行就没有机会运行。不幸的是, CPU会乱序执行, 在某些条件满足的情况下, 它取得了attacked _mem_addr里的值, 并在CPU将该指令标记为异常之前将它传递给了下一条指令, 并且随后的指令利用它来触发了内存访问。在指令提交的阶段CPU发现了异常,再将已经乱序执行指令的结果丢弃掉。这样虽然没有对指令的正确性造成影响, 但是乱序执行产生的CPU cache影响依然还是在那里, 并能被利用。详情参见网页https://blog.csdn.net/zdy0_2004/article/details/78998810

 

##                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      测试环境准备

ubuntu 16.04

该版本已有meltdown补丁https://askubuntu.com/questions/991874/how-to-disable-page-table-isolation-to-regain-performance-lost-due-to-intel-cpu

The patch (aka "Page table isolation") will be part of a normal kernel update. However, keeping the kernel up to date is highly recommended, as it also gets a lot of other security fixes. So I would not recommend just using an outdated kernel without the fix.

However, you can effectively disable the patch by adding pti=off (kernel patch adding this option, with more info) to your kernel command line (howto). Note that doing this will result in a less secure system.

关闭PTI

> $sudo nano /etc/default/grub

然后修改添加pti = off

如图：

![image](http://github.com/leitScarlett/4-leitScarlett/raw/master/turn off PTI.png)

之后检查内核

> $ ./spectre-meltdown-checker.sh 

![image](http://github.com/leitScarlett/4-leitScarlett/raw/master/can be attack.png)

为存在meltdown漏洞，可被攻击

## 测试过程

> $make
> gcc -o0 meltdown.c
> $./run.sh# 此脚本引用自：https://gitee.com/idea4good/meltdown/blob/master/run.sh
>
>#源代码参考请间源代码注释

## 期望的结果

![image](http://github.com/leitScarlett/4-leitScarlett/raw/master/expected result.png)

从图片中可以看到，开头几个字符为%s version %s (build... 本程序目的是泄漏version信息