# NiubiX

Just a reverse proxy service that surpasses Nignx

### 与Nginx 对比测试

> Test environment GCP cloud VM, 2 cores, 4GB RAM

**nginx config**
```
server {
    listen       8082;
    server_name  localhost;

    access_log  off;
    error_log off;

    location / {
        proxy_pass http://127.0.0.1:8080;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
    }
}

root         516       1  0 Aug24 ?        00:00:00 nginx: master process /usr/sbin/nginx -g daemon on; master_process on;
www-data  417322     516  0 12:13 ?        00:00:06 nginx: worker process
www-data  417323     516  0 12:13 ?        00:00:08 nginx: worker process
```

**nginx 测试反向代理能力**
```
wrk -t 2 -c 10 -d 10s  http://127.0.0.1:8082/xxx
Running 10s test @ http://127.0.0.1:8082/xxx
  2 threads and 10 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     1.16ms  425.71us   9.03ms   71.12%
    Req/Sec     4.34k   416.69     5.55k    77.23%
  87284 requests in 10.10s, 14.15MB read
Requests/sec:   8642.17
Transfer/sec:      1.40MB
```

**niubix 测试反向代理能力, 响应速度和并发处理能力表现都不错, 吞吐能力是nginx的近5位**
```
wrk -t 2 -c 10 -d 10s  http://127.0.0.1:8081/xxx
Running 10s test @ http://127.0.0.1:8081/xxx
  2 threads and 10 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency   443.05us  457.43us  21.74ms   98.95%
    Req/Sec    21.05k     2.30k   42.63k    95.52%
  421099 requests in 10.10s, 61.85MB read
Requests/sec:  41693.22
Transfer/sec:      6.12MB
```

**单独测试后端程序处理能力, 不存在吞吐量瓶颈**

```
wrk -t 2 -c 10 -d 10s  http://127.0.0.1:8080/xxx
Running 10s test @ http://127.0.0.1:8080/xxx
  2 threads and 10 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency   123.60us  308.52us  12.35ms   99.39%
    Req/Sec    30.00k     1.15k   36.99k    86.07%
  600135 requests in 10.10s, 88.14MB read
Requests/sec:  59418.82
Transfer/sec:      8.73MB
```
### 测试声明

* niubix 支持X-Real-IP,  X-Forwarded-For, 其他Header并没有解析  
* http parser只是一个非常简单的解析, 并没有完全实现
* 只是初步测试, 并没有做冒烟测试和稳定性测试以及多条件下复杂测试
* 全部都是在单机完成, 测试环境有限, 也没做多个backend均衡测试
* niubix仅提供反向代理功能
* 以上测试niubix均衡测试使用的是roundrobin(别的也还没实现呢)
* backend 测试程序[code](https://github.com/shaovie/reactor/blob/main/example/techempower.cpp)
* niubix刚刚完成, 只实现了http协议, 也没有做优化工作
* 我相信这是一个好的开始

## Development Roadmap

- [x] Gracefully reload (like nginx reload)
- [ ] Better HTTP parser
- [ ] TCP protocol + Proxy Protocol
- [ ] Https
