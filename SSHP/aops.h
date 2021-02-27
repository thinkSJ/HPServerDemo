//////////////////////////////////////////////////////////
//
// 通用AOP对象
//
//////////////////////////////////////////////////////////
#pragma once
#include <fmt/core.h>
#include <cinatra.hpp>
#include <spdlog/spdlog.h>
using namespace cinatra;

// 日志记录
struct log_t
{
  bool before(request& req, response& res) {
    spdlog::info("value: {} -->url:{}", req.get_query_value("id"), req.get_url());
    return true;
  }
  bool after(request& req, response& res) {
    auto[ip, port] = req.get_conn<cinatra::SSL>()->remote_ip_port();
    spdlog::info("good boy[{}:{}]", ip, port);
    return true;
  }
};