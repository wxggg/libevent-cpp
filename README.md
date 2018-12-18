# libevent-cpp
libevent library reimplemented with c++

* [libevent-cpp 基本架构及信号机制](docs/1-libevent-cpp-0.0.1-signal.md)
* [libevent-cpp 事件重构及超时事件](docs/2-libevent-cpp-0.0.2-time.md)
* [libevent-cpp 读写事件及Linux内核select机制原理](docs/3-libevent-cpp-0.0.3-select.md)
* [libevent-cpp poll封装及Linux内核实现原理](docs/4-libevent-cpp-0.0.4-poll.md)
* [libevent-cpp epoll机制及其在Linux内核中的实现](docs/5-libevent-cpp-0.0.5-epoll.md)

## 2018-12-18 更新
基本完成libevent-1.1b版本的cpp实现，主要包括如下内容:
* 三种类型事件，分别为时间事件、信号事件以及读写事件
* 另外还有针对文件描述符读写的缓冲事件及缓冲区，本质上还是读写事件
* 三种Linux下的io多路复用机制，分别为select、poll及epoll
* 各个测试程序迁移实现及调试通过，其中性能测试程序test/benchmark/bench的性能测试接近libevent-c版本
