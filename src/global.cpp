#include "global.h"

int g::pid = 0;
int g::shutdown_child_pid = 0;
int g::child_pid = 0;
reactor *g::conn_reactor = nullptr;
reactor *g::g_reactor = nullptr;
