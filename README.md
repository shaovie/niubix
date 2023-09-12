# NiubiX

Just a reverse proxy service that surpasses Nignx

### 与Nginx 对比测试

> Test environment
> Instacne 1 GCP cloud VM, 2 cores, 4GB RAM 10.146.0.2 (nginx, niubix run at here)
> Instacne 2 GCP cloud VM, 2 cores, 4GB RAM 10.146.0.3 (backend, wrk run at here)

**nginx config**
```
server {
    listen       8082 reuseport;
    server_name  localhost;

    access_log  off;
    error_log off;

    location / {
        proxy_pass http://10.146.0.3:8080;
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
run at 10.146.0.3

wrk -t 2 -c 100 -d 10s  http://10.146.0.2:8082/xxx
Running 10s test @ http://10.146.0.2:8082/xxx
  2 threads and 100 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    55.71ms   52.58ms 226.45ms   70.15%
    Req/Sec     1.06k     2.29k   10.04k    92.00%
  21059 requests in 10.01s, 3.41MB read
Requests/sec:   2103.96
Transfer/sec:    349.22KB
```
nginx测试局域网环境,数据很差,目前还没找到原因,cpu也能跑满,但是就是qps上不去  
```
如果换到本机 proxy_pass http://127.0.0.1:8080; nginx qps 能有所提升, 但还是不理想

wrk -t 2 -c 100 -d 10s  http://10.146.0.2:8082/xxx
Running 10s test @ http://10.146.0.2:8082/xxx
  2 threads and 100 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    10.57ms    6.26ms 108.02ms   67.57%
    Req/Sec     4.81k   335.06     5.68k    74.00%
  95756 requests in 10.00s, 15.52MB read
Requests/sec:   9573.99
Transfer/sec:      1.55MB
```


**niubix 测试反向代理能力, 响应速度和并发处理能力表现都不错, 吞吐能力超过nginx的2倍**
```
run at 10.146.0.3

wrk -t 2 -c 100 -d 10s  http://10.146.0.2:8081/xxx
Running 10s test @ http://10.146.0.2:8081/xxx
  2 threads and 100 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     2.20ms    1.43ms  20.03ms   66.98%
    Req/Sec    36.82k     1.90k   39.80k    82.50%
  736359 requests in 10.06s, 108.15MB read
  Socket errors: connect 0, read 7, write 0, timeout 0
Requests/sec:  73222.56
Transfer/sec:     10.75MB

```

**单独测试后端程序处理能力, 不存在吞吐量瓶颈**

```
run at 10.146.0.2

wrk -t 2 -c 100 -d 10s  http://10.146.0.3:8080/xxx
Running 10s test @ http://10.146.0.3:8080/xxx
  2 threads and 100 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency   520.95us  203.98us   4.09ms   68.03%
    Req/Sec    59.25k     2.68k   63.62k    52.50%
  1179133 requests in 10.00s, 173.17MB read
Requests/sec: 117888.45
Transfer/sec:     17.31MB
```
### 测试声明

* niubix仅提供反向代理功能
* niubix 支持X-Real-IP,  X-Forwarded-For, 其他Header并没有解析  
* http parser只是一个非常简单的解析, 并没有完全实现
* 只是初步测试, 并没有做冒烟测试和稳定性测试以及多条件下复杂测试
* niubix均衡策略使用的是roundrobin(别的也还没实现呢), nginx也是一样的策略
* backend 测试程序[code](https://github.com/shaovie/reactor/blob/main/example/techempower.cpp)
* 功能逐步完善中, 基本框架是过硬的, 我相信这是一个好的开始

## Development Roadmap

- [x] Gracefully reload (like nginx reload)
- [ ] Better HTTP parser
- [ ] TCP protocol + Proxy Protocol
- [ ] Https
