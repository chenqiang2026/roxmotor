automotive_vehicle

camera APP (DVR,OMS,DMS)
camera Framework
camera HAL
回掉V4L2(Linux 内核)
AIS client
AIS server 
QNX camera 驱动
camera
![](https://raw.githubusercontent.com/chenqiang2026/picgo-github/main/test00/Kowloon_City%252C_Mainland%252C_opposite_Hong_Kong.jpg)

1.有了 copy_to_user() 和copy_from_user() ，为什么还要引入mmap？
2：Andrid HAL 是怎么调用 V4L2的，V4L2是怎么调用AIS的?
3：调用Android的接口打开camera,ais V4L2 给的是编码的视频数据还是raw 视频数据？
4: select/poll/epoll/mmap/copy_to_user/copy_from_user/
6: 高通8155 的AVM 4个环视camera接在ADC上面，高通8295的AVM 4个环视接在IVI上面,处理 AVM矫正,拼接的算法和接口需要从ADC 移到  CDC上面
 CDC上面是QNX 处理 AVM的视频流 还是Android 处理呢？ QNX的启动时间比Android 快18-20S左右。
