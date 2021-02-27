//////////////////////////////////////////////////////////
//
// 登录码验证: "/api/check"
//   返回登录验证结果
//
//////////////////////////////////////////////////////////
#pragma once
#include "aops.h"

//////////////////////////////////////////////////////////
//{"requestid":"121745570","response":{"responsecode":2100,"responsemsg":"验证码过期","data":null}}
//{"requestid":"122632651","response":{"responsecode":0,"responsemsg":"Success","data":null}}

// 登录验证码结果验证
struct check_t
{
  std::string build__msg(std::string_view id, int code, std::string_view content)
  {
    nlohmann::json response;
    response["responsecode"] = code;
    response["responsemsg"] = std::string(content.data(), content.size());
    response["data"] = nullptr;
    
    auto ret_json = nlohmann::json::object();
    ret_json["requestid"] = std::string(id.data(), id.size());
    ret_json["response"] = response;
    return ret_json.dump();
  }

  void operator()(request& req, response& res)
  {
    std::string_view body = req.body();
    if (body.size() == 0)
    {
      res.set_status_and_content(status_type::bad_request, "bad request format");
      return;
    }

    auto obj = nlohmann::json::parse(body.data(), body.data() + body.size());
    if (!obj.is_object())
    {
      res.set_status_and_content(status_type::bad_request, "bad request format");
      return;
    }
    std::string req_id;
    obj.at("requestid").get_to(req_id);

    res.add_header("Access-Control-Allow-Credentials", "true");
    res.add_header("Access-Control-Allow-Headers", "Content-Type");
    res.add_header("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
    res.add_header("Content-Type", "application/json");
    std::string str = build__msg(req_id, 0, "Success"sv);
    res.set_status_and_content(status_type::ok, std::move(str));
  }
};