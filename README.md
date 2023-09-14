# NiubiX

Just a reverse proxy service

实验性项目，NiubiX 只提供反向代理功能，大家轻拍有不好的地方可以留言或提 issue/pr.  觉得好就点个 star ，我会持续完善它

与 Nginx/Haproxy 对比测试 QPS可以达到3倍以上  

测试环境:
> Linux 5.19.0-1030-gcp #32~22.04.1-Ubuntu  
> Instacne 1 GCP cloud VM, 2 cores, 4GB RAM 10.146.0.2 (nginx,haproxy, niubix run at here)   
> Instacne 2 GCP cloud VM, 2 cores, 4GB RAM 10.146.0.3 (backend, wrk run at here)  

**nginx version config**
```
nginx version: nginx/1.24.0 (Ubuntu)

upstream backend {
	server 10.146.0.3:8080;
	keepalive 16;
}
server {
	listen       8082 reuseport;
	server_name  localhost;

	access_log  off;
	error_log 	off;

	location / {
		proxy_pass http://backend;
		proxy_http_version 1.1;
		proxy_set_header Connection "";
		proxy_set_header X-Real-IP $remote_addr;
		proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
	}
}

root         516       1  0 Aug24 ?        00:00:00 nginx: master process /usr/sbin/nginx -g daemon on; master_process on;
www-data  417322     516  0 12:13 ?        00:00:06 nginx: worker process
www-data  417323     516  0 12:13 ?        00:00:08 nginx: worker process
```

**haproxy version config**
```
HAProxy version 2.4.22-0ubuntu0.22.04.2 2023/08/14

listen niubix
    bind 0.0.0.0:8083
    mode http
    option forwardfor
    server s1 10.146.0.3:8080

ps -eLf | grep haproxy
root      449421       1  449421  0    1 15:11 ?        00:00:00 /usr/sbin/haproxy -Ws -f /etc/haproxy/haproxy.cfg -p /run/haproxy.pid -S /run/haproxy-master.sock
haproxy   449423  449421  449423  0    2 15:11 ?        00:00:05 /usr/sbin/haproxy -Ws -f /etc/haproxy/haproxy.cfg -p /run/haproxy.pid -S /run/haproxy-master.sock
haproxy   449423  449421  449429  0    2 15:11 ?        00:00:05 /usr/sbin/haproxy -Ws -f /etc/haproxy/haproxy.cfg -p /run/haproxy.pid -S /run/haproxy-master.sock
```

**单独测试后端程序处理能力, 确保不存在吞吐量瓶颈**
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

连续测试数据
```
(base) root@instance-1:~# wrk -t 2 -c 100 -d 10s -H 'Connection: keep-alive' http://10.146.0.2:8081/niubix
Running 10s test @ http://10.146.0.2:8081/xxx
  2 threads and 100 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     1.22ms  800.49us  19.71ms   94.57%
    Req/Sec    26.67k     1.92k   29.53k    76.00%
  530996 requests in 10.01s, 77.99MB read
Requests/sec:  53032.21
Transfer/sec:      7.79MB
(base) root@instance-1:~# wrk -t 2 -c 100 -d 10s -H 'Connection: keep-alive' http://10.146.0.2:8082/nginx
Running 10s test @ http://10.146.0.2:8082/xxx
  2 threads and 100 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency    10.16ms   13.47ms  93.49ms   85.88%
    Req/Sec     8.64k     7.59k   23.31k    68.50%
  172028 requests in 10.01s, 26.41MB read
Requests/sec:  17188.44
Transfer/sec:      2.64MB
(base) root@instance-1:~# wrk -t 2 -c 100 -d 10s -H 'Connection: keep-alive' http://10.146.0.2:8083/haproxy
Running 10s test @ http://10.146.0.2:8083/xxx
  2 threads and 100 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency     6.49ms    8.64ms 141.16ms   97.99%
    Req/Sec     8.89k     1.25k   13.48k    85.86%
  176005 requests in 10.00s, 21.82MB read
Requests/sec:  17598.35
Transfer/sec:      2.18MB
```
![](https://picx.zhimg.com/80/v2-80ddd7903e85bffbacb4c4b071241f01_1440w.png)

```
07:29:07.171557 IP 10.146.0.2.48798 > 10.146.0.3.8080: Flags [.], ack 1, win 511, options [nop,nop,TS val 1952514973 ecr 3339282563], length 0
	0x0000:  4500 0034 614f 4000 4006 c44c 0a92 0002  E..4aO@.@..L....
	0x0010:  0a92 0003 be9e 1f90 553e 4bb9 90f5 ce3d  ........U>K....=
	0x0020:  8010 01ff d9b0 0000 0101 080a 7461 039d  ............ta..
	0x0030:  c709 6883                                ..h.
07:29:07.171651 IP 10.146.0.2.48798 > 10.146.0.3.8080: Flags [P.], seq 1:135, ack 1, win 511, options [nop,nop,TS val 1952514973 ecr 3339282563], length 134: HTTP: GET /xxx HTTP/1.1
	0x0000:  4500 00ba 6150 4000 4006 c3c5 0a92 0002  E...aP@.@.......
	0x0010:  0a92 0003 be9e 1f90 553e 4bb9 90f5 ce3d  ........U>K....=
	0x0020:  8018 01ff 1467 0000 0101 080a 7461 039d  .....g......ta..
	0x0030:  c709 6883 4745 5420 2f78 7878 2048 5454  ..h.GET./xxx.HTT
	0x0040:  502f 312e 310d 0a58 2d52 6561 6c2d 4950  P/1.1..X-Real-IP
	0x0050:  3a20 3130 2e31 3436 2e30 2e32 0d0a 582d  :.10.146.0.2..X-
	0x0060:  466f 7277 6172 6465 642d 466f 723a 2031  Forwarded-For:.1
	0x0070:  302e 3134 362e 302e 320d 0a48 6f73 743a  0.146.0.2..Host:
	0x0080:  2031 302e 3134 362e 302e 323a 3830 3831  .10.146.0.2:8081
	0x0090:  0d0a 5573 6572 2d41 6765 6e74 3a20 6375  ..User-Agent:.cu
	0x00a0:  726c 2f37 2e38 312e 300d 0a41 6363 6570  rl/7.81.0..Accep
	0x00b0:  743a 202a 2f2a 0d0a 0d0a                 t:.*/*....
07:29:07.171661 IP 10.146.0.3.8080 > 10.146.0.2.48798: Flags [.], ack 135, win 505, options [nop,nop,TS val 3339282564 ecr 1952514973], length 0
	0x0000:  4500 0034 9f4e 4000 4006 864d 0a92 0003  E..4.N@.@..M....
	0x0010:  0a92 0002 1f90 be9e 90f5 ce3d 553e 4c3f  ...........=U>L?
	0x0020:  8010 01f9 154f 0000 0101 080a c709 6884  .....O........h.
	0x0030:  7461 039d                                ta..
07:29:07.171808 IP 10.146.0.3.8080 > 10.146.0.2.48798: Flags [P.], seq 1:155, ack 135, win 505, options [nop,nop,TS val 3339282564 ecr 1952514973], length 154: HTTP: HTTP/1.1 200 OK
	0x0000:  4500 00ce 9f4f 4000 4006 85b2 0a92 0003  E....O@.@.......
	0x0010:  0a92 0002 1f90 be9e 90f5 ce3d 553e 4c3f  ...........=U>L?
	0x0020:  8018 01f9 15e9 0000 0101 080a c709 6884  ..............h.
	0x0030:  7461 039d 4854 5450 2f31 2e31 2032 3030  ta..HTTP/1.1.200
	0x0040:  204f 4b0d 0a43 6f6e 6e65 6374 696f 6e3a  .OK..Connection:
	0x0050:  206b 6565 702d 616c 6976 650d 0a53 6572  .keep-alive..Ser
	0x0060:  7665 723a 2067 6f65 760d 0a43 6f6e 7465  ver:.goev..Conte
	0x0070:  6e74 2d54 7970 653a 2074 6578 742f 706c  nt-Type:.text/pl
	0x0080:  6169 6e0d 0a44 6174 653a 2054 7565 2c20  ain..Date:.Tue,.
	0x0090:  3037 2053 6570 2035 3536 3636 2032 313a  07.Sep.55666.21:
	0x00a0:  3036 3a32 3520 474d 540d 0a43 6f6e 7465  06:25.GMT..Conte
	0x00b0:  6e74 2d4c 656e 6774 683a 2031 330d 0a0d  nt-Length:.13...
	0x00c0:  0a48 656c 6c6f 2c20 576f 726c 6421       .Hello,.World!
```
![](https://pica.zhimg.com/80/v2-d5ae358121a2c93e4544cbea1925a020_1440w.png?source=d16d100b)
tcpdump tcp port 8080 抓包查看 niubix 实际数据，包含 X-Real-IP, XFF ，并且响应在微秒级

#### 目前具备功能：
* master/worker 模式，worker 采用多线程，支持配置优雅的 Reload(像 nginx 一样)，master 还是守护进程，当 worker 进程异常会马上 fork 一个新的
* 只支持 Linux （将来也不准备跨平台）
* 主体逻辑无锁，简单高效，可靠
* 优雅的 acceptor/connector ，高效实现异步监听+连接

#### 测试声明
* niubix 仅提供反向代理功能
* niubix 支持 X-Real-IP,  X-Forwarded-For, 其他 Header 并没有解析  
* http parser 只是简单的解析, 并没有完全实现
* 只是初步测试, 并没有做冒烟测试和稳定性测试以及多条件下复杂测试
* niubix 均衡策略使用的是 roundrobin(别的也还没实现呢), haproxy 也是一样的策略
* backend 测试程序[code](https://github.com/shaovie/reactor/blob/main/example/techempower.cpp)
* niubix 不解析 response 内容
* 功能逐步完善中, 基本框架是过硬的, 我相信这是一个好的开始

## Development Roadmap

- [x] Gracefully reload (like nginx reload)
- [x] Health check
- [ ] POST/DELETE/PUT/HEAD/... support
- [ ] Admin api
- [ ] More balance prolicy
- [ ] Better HTTP parser
- [ ] TCP protocol + Proxy Protocol
- [ ] Https
