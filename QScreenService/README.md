Version1: CMakeLists.txt  和屏幕代码，屏幕相关的代码 大体上的 思路，没有 经过代码review.
Version2 : 
    1: 修复 CMakeLists.txt ,比如 QNX set 编译环境放在 project之前。
    2: 修复 poll方法中 poll 结构体中的fds.fd的 重置 
    3: 通过回调cb函数将命令放到转发队中后，清空 设备节点fd的内容
    4: main函数中,  新增 /pps ,/tmp/pps,
    5:针对 在linux上使用交叉编译链 编译 成QNX 程序，然后推送车机，但是编译的时候unique_ptr的使用会报错。
     将unique_ptr 换成普通指针.
Version3:
    1:0112：传输bin文件中特定地址的固件数据，每次传输128字节，CRC16(128),和Excel表中的有区别;
    2:提取bootloader模式下 每个方法都有读写while循环。待优化，建议去掉2个while循环
    


