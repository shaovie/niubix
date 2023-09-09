#include "global.h"
#include "conf.h"
#include "log.h"
#include "worker.h"
#include "leader.h"

int g::pid = 0;
int g::shutdown_child_pid = 0;
int g::child_pid = 0;
worker *g::main_worker = nullptr;
leader *g::g_leader = nullptr;

int g::init(const conf *cf) {
    g::main_worker = new worker();
    if (g::main_worker->open(nullptr, cf) != 0) {
        log::error("main worker open fail!");
        return -1;
    }
    g::g_leader = new leader();
    if (g::g_leader->open(cf) != 0) {
        log::error("leader open fail!");
        return -1;
    }
    // 以上只是线程启动完成, 并没有附加事件处理
    return 0;
}
