//////////////////////////////////////////////////////////
//
// 获取登录验证码: "/api/get"
//   返回验证给前端
//
//////////////////////////////////////////////////////////
#pragma once
#include "aops.h"
#include <atomic>
#include <random>

// 登录验证码
struct get_t
{
  http_server& server;
  // 登录验证码图片数量
  static std::atomic_int file_count;
  // 包含X,Y
  int rand_range(int x, int y)
  {
    unsigned int seed = (unsigned int)std::chrono::high_resolution_clock::now().time_since_epoch().count();
    std::default_random_engine engine(seed);
    std::uniform_int_distribution<int> range(x, y);
    return range(engine);
  }

  void operator()(request& req, response& res)
  {
    if (0 == file_count)
    {
      // 第一次来统计一下登录验证码数量
      std::filesystem::path path("./www/captcha/login/");
      for (auto& fe : std::filesystem::directory_iterator(path))
      {
        if (fe.is_directory())
          continue;
        file_count++;
      }
      // 底图与滑块数量必须一致
      if (file_count % 2)
      {
        spdlog::info("登录验证码图片数量不正确,必须配对");
        return;
      }
      file_count.store(file_count.load() >> 1);
      spdlog::info("总验证码数量:{}", file_count);
    }

    // 随机一对验证码图片返回
    int index = rand_range(1, file_count);
    auto path = fmt::format("./www/captcha/login/{}.jpg", index); // 底图
    auto path1= fmt::format("./www/captcha/login/{}.png", index); // 滑块
    if (!std::filesystem::exists(path) || !std::filesystem::exists(path1))
    {
      res.set_status_and_content(status_type::not_found, "file not found");
      return;
    }
    uintmax_t jpgsize = std::filesystem::file_size(path);
    uintmax_t pngsize = std::filesystem::file_size(path1);
    std::string strsize = fmt::format("{0:0>6}{1:0>6}", jpgsize, pngsize);

    // 响应头
    res.add_header("Access-Control-Allow-Credentials", "true");
    res.add_header("Access-Control-Allow-Headers", "Content-Type");
    res.add_header("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
    res.add_header("Access-Control-Expose-Headers", "gpcaptchaid,gpcaptchal");
    res.add_header("gpcaptchaid", "142980a3-6422-456f-8f49-3899e0ca473f");
    res.add_header("gpcaptchal", std::move(strsize));
    res.add_header("Set-Cookie", "gpcaptchaid=142980a3-6422-456f-8f49-3899e0ca473f; Path=/; HTTPOnly");

    // 读取验证码图片
    auto ifs_ptr = std::make_shared<std::ifstream>(path, std::ios_base::binary);
    std::stringstream file_buffer;
    file_buffer << ifs_ptr->rdbuf();
    auto ifs_ptr1 = std::make_shared<std::ifstream>(path1, std::ios_base::binary);
    file_buffer << ifs_ptr1->rdbuf();
    server.send_small_file_content(res, file_buffer, "text/plain");
  }
};

std::atomic_int get_t::file_count = 0;