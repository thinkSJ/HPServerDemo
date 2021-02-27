//////////////////////////////////////////////////////////
//
// 登录页处理:    "/login"
// 返回系统信息:  "/system-info"
//////////////////////////////////////////////////////////
#pragma once
#include "aops.h"
#include "index.h"
#include <jwt-cpp/base.h>
#include <jwt-cpp/jwt.h>
#include <string>
#include <string_view>

//////////////////////////////////////////////////////////
//{"error":"invalid_request","error_description":"****desc****"}
//{"access_token":"****JWT-TOKEN****","token_type":"bearer","expires_in":7200}

// 验证码错误
constexpr std::string_view captcha_ver = "Captcha verification failed"sv;
// 其它地方登录,需要身份验证
constexpr std::string_view need_id_card = "Id card required"sv;

// 登录页 或 首页
struct login_t
{
  // 构造错误JSON
  std::string build_error_msg(std::string_view content)
  {
    auto ret_json = nlohmann::json::object();
    ret_json["error"] = "invalid_request";
    ret_json["error_description"] = std::string(content.data(), content.size());
    return ret_json.dump();
  }
  
  void operator()(request& req, response& res)
  {
    std::string_view type = req.get_query_value("type");
    if (type == "corporate") // 首页
    {
      index_t{}(req, res);
      return;
    }
    // 从BODY中获取到username
    auto body = req.body();
    auto obj = nlohmann::json::parse(body.data(), body.data() + body.size());
    if (!obj.is_object() || !obj.contains("username"))
    {
      spdlog::info("error request format, no username");
      return;
    }
    std::string stridcard;
    obj["idCard"].get_to<std::string>(stridcard);
    if (stridcard.empty())
    {
      spdlog::info("error request format, no idCard");
      auto str = build_error_msg(need_id_card);
      res.add_header("Content-Type", "application/json");
      res.set_status_and_content(status_type::bad_request, str.c_str());
      return;
    }
    // JWT
    using namespace std::string_literals;
    std::string user_name;
    obj.at("username").get_to(user_name);
    auto token = jwt::create()
      .set_type("JWT")
      .set_algorithm("HS512")
      .set_payload_claim("sub", jwt::claim(user_name))
      .set_payload_claim("device_type", jwt::claim("web"s))
      .set_payload_claim("jti", jwt::claim(""s))
      .set_payload_claim("exp", jwt::claim("1606412852"s))
      .sign(jwt::algorithm::hs512{"secret"});

    // 构造返回JSON
    auto ret_json = nlohmann::json::object();
    ret_json["access_token"] = token.c_str();
    ret_json["token_type"] = "bearer";
    ret_json["expires_in"] = 7200;
    res.add_header("Content-Type", "application/json");
    res.set_status_and_content(status_type::ok, ret_json.dump());
  }
};

//////////////////////////////////////////////////////////
constexpr auto ws_server = "ws://localhost:8080";
constexpr auto wss_server = "wss://localhost:8080";
constexpr auto capt_server = "http://localhost:8080";
constexpr auto capt_servers = "https://localhost:8080";
// 系统信息
struct sysinfo_t
{
  void operator()(request& req, response& res)
  {
    auto ret_json = nlohmann::json::object();
    ret_json["auctionType"] = "Corporate";
    ret_json["currentTime"] = "2020-11-23T04:29:33.025Z";
    ret_json["captchaServiceUrl"] = "http://localhost:8080/"; // 验证码服务器
    ret_json["endpoints"].push_back(ws_server);  // WSS服务器
    ret_json["endpoints"].push_back(ws_server);  // WSS服务器
    res.add_header("Content-Type", "application/json");
    res.set_status_and_content(status_type::ok, ret_json.dump());
  }
};