![](res/libevent-cpp.png)

**Libevent-cpp 是使用C++重构libevent的高性能多线程网络库，在重写libevent的基础上增加了对多线程的支持，能很方便的实现一个高并发的http服务器**

[![Build Status](https://api.travis-ci.com/wxggg/libevent-cpp.svg)](https://travis-ci.com/wxggg/libevent-cpp)

## Warning:  目前还存在bug可能会导致崩溃，主要是当进行长时间的高并发压力测试的时候会产生指针访问异常，导致程序崩溃，经过重构后的项目为 https://github.com/wxggg/libio， 清晰而且基本没有bug和内存泄漏
## Warning: There are still bugs that may cause a crash. The main reason is that when a long-term high-concurrency stress test is performed, a pointer access exception will occur, causing the program to crash. The refactored project is https://github.com/wxggg/libio , clear and basically no bugs and memory leaks

# Features
* 使用IO多路复用技术，支持select、poll及epoll
* 基于异步事件，支持信号事件、超时事件及读写事件
* 使用线程池来支持多线程
* 使用c++11及c++14新特性，基本上全部使用智能指针进行资源管理
* 支持http协议1.0/1.1，支持长短连接，并能优雅关闭连接
* 仅支持Linux
* 使用连接池来复用连接以提高效率
* 增加异步日志系统，利用空闲缓冲池队列来进行异步日志系统性能优化

**WARNING: There's bugs in this project**
> When benched with webbench.c for high concurrency for more than 60 seconds, the code may corupt.

# Documentations
* [libevent-cpp 基本架构及信号机制](docs/1-libevent-cpp-0.0.1-signal.md)
* [libevent-cpp 事件重构及超时事件](docs/2-libevent-cpp-0.0.2-time.md)
* [libevent-cpp 读写事件及Linux内核select机制原理](docs/3-libevent-cpp-0.0.3-select.md)
* [libevent-cpp poll封装及Linux内核实现原理](docs/4-libevent-cpp-0.0.4-poll.md)
* [libevent-cpp epoll机制及其在Linux内核中的实现](docs/5-libevent-cpp-0.0.5-epoll.md)
* [遇到的困难](docs/trouble.md)

# Building
```bash
make -j4
cd ../test
make -j4
```

# Usage
使用libevent-cpp能够非常方便的创建一个http服务器，比如如下例子创建一个简单的静态http文件服务器
```c++
#include <http_server.hh>

#include <memory>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <string>
#include <map>

using namespace std;
using namespace eve;

void send_file(http_request *req, string path)
{
    if (path.at(path.length() - 1) == '/')
        path += "index.html";
    map<string, string> m;
    m["html"] = "text/html";
    m["css"] = "text/css";
    m["xml"] = "text/xml";
    m["png"] = "png";
    m["js"] = "application/javascript";
    path = "static/" + path;
    auto pos = path.find_last_of('.');
    if (pos == string::npos)
    {
        req->send_not_found();
        return;
    }
    auto filename = path.substr(0, pos);
    auto filetype = path.substr(pos + 1);
    string type = "text/html";
    if (m.count(filetype))
        type = m[filetype];
    req->output_headers["Content-Type"] = type + "; charset=utf-8";
    req->output_headers["Accept-Ranges"] = "bytes";

    if (filetype == "js")
        req->output_headers["Content-Type"] = type;

    cout << "visit file path=" << path << endl;

    stringstream bufferstr;
    {
        ifstream ifs(path);
        if (!ifs)
        {
            req->send_not_found();
            return;
        }
        bufferstr << ifs.rdbuf();
    }

    auto buf = std::make_unique<buffer>();
    buf->push_back_string(bufferstr.str());
    req->send_reply(HTTP_OK, "Everything is fine", req->input_headers["Empty"].empty() ? std::move(buf) : nullptr);
}

void home(http_request *req)
{
    send_file(req, "index.html");
}
void general_cb(http_request *req)
{
    string path = req->uri.substr(1);
    send_file(req, path);
}

int main(int argc, char const *argv[])
{
    auto server = make_shared<http_server>();

    server->set_handle_cb("/", home);
    server->set_gen_cb(general_cb);
    server->resize_thread_pool(4);
    server->start("localhost", 8080);

    return 0;
}
```

# Changelog

## [v0.3.0](https://github.com/wxggg/libevent-cpp/releases/tag/v0.3.0)
* 使用c++11/c++14重构
* 使用智能指针进行资源管理，解决大部分资源泄漏问题

## [v0.2.1](https://github.com/wxggg/libevent-cpp/releases/tag/v0.2.1)
* 增加了线程池
* http_server支持多线程

## [v0.2.0](https://github.com/wxggg/libevent-cpp/releases/tag/v0.2.0)
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

## [v0.1.0](https://github.com/wxggg/libevent-cpp/releases/tag/0.1.0)
基本完成libevent-1.1b版本的cpp实现，主要包括如下内容:
* 三种类型事件，分别为时间事件、信号事件以及读写事件
* 另外还有针对文件描述符读写的缓冲事件及缓冲区，本质上还是读写事件
* 三种Linux下的io多路复用机制，分别为select、poll及epoll
* 各个测试程序迁移实现及调试通过

