//////////////////////////////////////////////////////////
//
// webmessage消息处理
//
//////////////////////////////////////////////////////////
#pragma once
#include <fmt/core.h>
#include <cinatra.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/bin_to_hex.h>
#include <jwt-cpp/base.h>
#include <jwt-cpp/jwt.h>
using namespace cinatra;

// WSS
struct wss_t
{
  // 各个流程处理
  void handle_connect(request& req);
  void handle_disconnect(request& req);
  void handle_subscribe(request& req);
  void handle_unsubscribe(request& req);
  void handle_client_send(request& req);
  void handle_client_error(request& req);
  void handle_app_cardinfo(request& req, std::string_view uri);
  void handle_app_auction(request& req, std::string_view uri);
  void handle_mybid_app_auction(request& req, std::string_view uri, size_t amount);

  // 返回出价成功或失败
  void handle_user_queue_bid_updates(request& req, size_t amount, std::string_view msg, std::string_view id, bool isok);
  // 每秒回一次
  void handle_topic_auction(request& req, std::string_view uri);

  //////////////////////////////////////////////////
  void ret_captcha_img(request& req, std::string_view uri, std::string aid, size_t amount);
  std::string now_date();

  std::string get_current_time();
  std::string get_end_time();
  std::string get_update_time();

  std::string get_sub_id(request& req, std::string_view uri);
  std::string get_bid_num(request& req);

  // 相等:true, 不等:false
  bool stringnicmp(std::string_view sv1, std::string_view sv2)
  {
    if (!strncmp(sv1.data(), sv2.data(), sv2.size()))
      return true;
    return false;
  }
  // 用远程地址+端口作为Key
  std::string get_client_key(request& req)
  {
    return get_client_key(req.get_conn<cinatra::NonSSL>());
  }
  std::string get_client_key(std::shared_ptr<connection<NonSSL>> connection)
  {
    auto [ip, port] = connection->remote_ip_port();
    return ip + "_" + port;
  }

  void response_error(request& req, std::string_view msg);

  // 解析自定义协议头
  std::string_view get_title_value(std::string_view name, std::string_view data);
  // 获取内容
  std::string_view get_content(size_t length, std::string_view data);
  // send命令转成subscribe
  std::string_view to_subscribe(std::string_view cmd);

  asio::awaitable<void> co_topic_auction(std::shared_ptr<connection<NonSSL>> connection,
    std::string sub_id, std::string uri);

  void operator()(request& req, response& res)
  {
    assert(req.get_content_type() == content_type::websocket);
    req.on(ws_open, [](request& req) {
      spdlog::info("websocket start");
      });

    req.on(ws_message, [this](request& req) {
      auto part_data = req.get_part_data();
      if (stringnicmp(part_data.data(), "CONNECT"))
      {
        handle_connect(req);
        return;
      }
      else if (stringnicmp(part_data.data(), "DISCONNECT"))
      {
        handle_disconnect(req);
        return;
      }
      else if (stringnicmp(part_data.data(), "SUBSCRIBE"))
      {
        handle_subscribe(req);
        return;
      }
      else if (stringnicmp(part_data.data(), "UNSUBSCRIBE"))
      {
        handle_unsubscribe(req);
        return;
      }
      else if (stringnicmp(part_data.data(), "SEND"))
      {
        handle_client_send(req);
        return;
      }
      else if (part_data.length() == 1)
      {
        spdlog::info("ping/pong");
        return;
      }

      std::string str = std::string(part_data.data(), part_data.length());
      req.get_conn<cinatra::NonSSL>()->send_ws_string(std::move(str));

      spdlog::info("OH~ NO~~ 未解析头: {}", part_data);
      });

    req.on(ws_error, [=](request& req) { 
      handle_client_error(req);
      });
  }
};