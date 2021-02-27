//////////////////////////////////////////////////////////
//
// 首页处理: 需要返回./www/login.html
// :"/"
// :"/login?type=corporate"
//////////////////////////////////////////////////////////
#pragma once
#include <cinatra/render.h>
#include "aops.h"

// 首页
struct index_t
{
  void operator()(request& req, response& res)
  {
    using namespace std::string_literals;
    std::filesystem::path p{ "./www/login.html" };
    if (!std::filesystem::exists(p))
    {
      res.set_status_and_content(status_type::not_found, "file not found!!"s);
      return;
    }
    std::string str = render::render_file(p.string());
    res.set_status_and_content(status_type::ok, std::move(str));
  }
};