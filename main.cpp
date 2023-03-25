#include <unistd.h>
#include "server/webserver.h"

int main() {
    /* 守护进程 后台运行 */
    // daemon(1, 0); 

    WebServer server(
        1316, 3, 60000, false,                  // 端口，触发模式，超时时间，优雅关闭
        3306, "root", "root",                   // sql端口，sql用户名，sql密码
        "webserver", 12, 6,                     // 数据库名，连接池数量，线程数量
        true, 1, 1024                           // 开启日志，日志等级，日志队列大小
    );

    server.Start();                             // 开始服务
}