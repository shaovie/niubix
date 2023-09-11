#ifndef NBX_ROUTER_H_
#define NBX_ROUTER_H_

#include <cstdint>

// Forward declarations
class app::conf;

// 负责将新链接路由到指定的app模块
// 1. 通过port首先尝试, 如果能唯一锁定, 那就直接绑定, 当收到第1个请求进行二次确认
// 2. 如果1未锁定, 通过第1个数据包中http-header中的 Host + port来唯一锁定
class router {
public:
    static int match_backend(const app::conf *cf);
}:

#endif // NBX_ROUTER_H_
