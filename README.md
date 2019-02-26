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

## 2019-1-7 更新
这一段时间主要看了libevent-1.4.3-stable版中的http模块，在其基础上进行了重构，并加入到libevent-cpp中的 src/http下面，将原来的事件处理相关的内容整合到src/event下面，另外与平台有关如Linux接口函数的内容都转移到src/util下面。简要来说这一段时间加入的内容包括：
* http接口，主要包括
  * TCP连接以及http请求的解析及创建
  * http_server和http_client分别管理服客户端的连接
  * 提供uri用户处理函数接口
* 连接优雅关闭
* 处理GET和POST请求
* http请求头的多行处理
* 错误请求处理
* 大量数据的分块发送及获取，即http chunked

## Usage
使用libevent-cpp能够简单方便的创建一个服务端server和客户端client，用法如下所示，具体可参看`test/benchmark/regress_http.cc`中的测试用例
```c++
int main()
{
    string host = "127.0.0.1";
    int port = 8080;
    std::shared_ptr<event_base>base = new epoll_base();

    shared_ptr<http_server> server(new http_server(base)); //server 服务端
    server->set_handle_cb("/test", http_test_cb); //设置处理函数

    server->start(host, port); //启动

    shared_ptr<http_client> client(new http_client(base)); //client 客户端
    int connid = client->make_connection(host, port);

    std::shared_ptr<http_request> req(new http_request()); //请求
    req->output_headers["Host"] = "somehost";
    req->output_headers["Connection"] = "close";
    req->uri = "/test";
    req->type = REQ_GET;
    req->set_cb(http_request_done);

    client->make_request(connid, req); //发送请求

    base->loop();
    delete base;
}
```

### 遇到的困难小结
项目实现中遇到最多的麻烦就是指针造成的各种程序崩溃，当然这些问题现在都一一解决，libevent-1.4.3中的http相关的测试用例全部通过。但是这并不能保证代码一定正确，因为指针的存在，还是有程序崩溃的可能。所以总结了解决bug过程中的如下经验：
#### 1.指针声明时一定记得初始化为nullptr，尤其是在class中声明但是不一定使用的指针
对于这一点，一个惨痛的经历就是由于不确定性造成的程序崩溃的问题，比如class中有一个函数指针cb，如果没有在声明时初始化为nullptr，那么使用g++5.4的编译器随机给cb一个值，但是不一定为0，所以很可能在之后判断cb的时候不为空，然后调用，然后崩溃（gg）。
这个问题在高版本的编译器如g++7.3中有可能不会出现，但是又可能出现其它的问题，所以最直接的方式就是一定要将未使用的指针声明为nullptr
#### 2.资源类由资源管理类进行创建和释放，千万不要自己释放自己如delete this
虽然理论上来说delete this 在有些情况下是可以存在的，但是非常可能因为控制流的不清晰而导致不可名状的问题，所以资源类一定要由资源管理类进行释放。这里比如server使用list管理着几个连接，那么就应该由server来进行连接的创建和释放
#### 3.尽可能的使用智能指针
智能指针能够方便的解决资源泄露问题，而且尽可能多的使用智能指针也能够避免很多可能出现的指针问题，但是智能指针需要注意的是避免资源的多次释放，需要注意的就是析构函数中避免多次删除和释放，避免double free.
#### 4.关于调试segmentation fault: 析构函数是程序执行完却崩溃的重灾区
对于这样的错误调试，简单粗暴的方式就是使用gdb在可能出错的多个析构函数处打断点，然后c到最后一个出错的地方，之后不断单步调试，最后会停在出问题的地方。这种方式要注意的问题就是一定要在开始单步调试之前尽可能的靠近崩溃的地方，因为不这样的话step很容易陷入c++密密麻麻的各种标准库函数中去了。

