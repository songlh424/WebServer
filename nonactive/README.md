### 定时器处理非活动连接
由于非活跃连接占用了连接资源，严重影响服务器的性能，通过实现了一个服务器定时器，处理非活跃链接，释放资源。
利用alarm函数周期性地触发SIGALRM信号，该信号的信号处理函数利用管道通知主循环执行定时器链表上的定时任务。