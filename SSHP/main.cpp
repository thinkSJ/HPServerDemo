#include <cstdio>
#include "aops.h"
#include "index.h"
#include "login.h"
#include "get.h"
#include "check.h"
#include "websocket.h"

////////////////////////////////////////////////////////
// 服务器监听端口
constexpr auto server_port = "8080";

////////////////////////////////////////////////////////
int main()
{
  spdlog::info("HTTP服务器启动,端口:{}", server_port);
  http_server server(std::thread::hardware_concurrency());
  //server.set_ssl_conf({ "server.crt", "server.key" });
  server.listen("0.0.0.0", server_port);
  server.set_http_handler<GET, POST>("/", index_t{}, log_t{});
  server.set_http_handler<GET, POST>("/login", login_t{}, log_t{});
  server.set_http_handler<GET, POST>("/system-info", sysinfo_t{}, log_t{});
  server.set_http_handler<GET, POST>("/api/get", get_t{server}, log_t{});
  server.set_http_handler<GET, POST>("/api/check", check_t{}, log_t{});
  server.set_http_handler<GET, POST>("/messages", wss_t{});

  server.run();
  return 0;
}