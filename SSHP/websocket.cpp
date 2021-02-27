#include "websocket.h"
#include <vector>
#include <base64.h>
#include <chrono>
using namespace std::chrono_literals;
/////////////////////////////////////////////////
struct ClientInfo{
  std::string     bidname;
  bool  start_update = false;
  std::map<std::string, std::string> subinfo; //destination: sub-id
};
std::map<std::string, ClientInfo> user_map_;//rip_port: info

// 当前拍卖信息
struct CurrentAuctionInfo
{
  std::atomic_bool  inited;
  size_t  low_limiter;
  size_t  user_count;     // 
  size_t  total_quota;    // 总拍卖额度
  std::string start_time; // 拍卖会起始时间
  std::string end_time;   // 拍卖会结束时间
  std::string update_time;// 第二轮开始时间
  std::string type{"Corporate"};//     // 拍卖会类型
};
CurrentAuctionInfo cai{ false };

asio::io_context update_time(1);
asio::executor_work_guard<asio::io_context::executor_type> work(update_time.get_executor());
/////////////////////////////////////////////////
void wss_t::handle_connect(request& req)
{
  spdlog::info("CONNECTED ");
  // JWT解密拿到标书号
  auto part_data = req.get_part_data();
  auto start = part_data.find_first_of('.');
  if (start == std::string_view::npos)
      spdlog::error("未找到JWT开始");

  auto end = part_data.find_first_of('.', start+1);
  if (end == std::string_view::npos)
      spdlog::error("未找到JWT结束");

  auto jwt_sv = part_data.substr(start + 1, end - start - 1);
  if (jwt_sv.empty())
      spdlog::error("没有JWT数据");

  using vzsdk::Base64;
  std::string ss =
    Base64::Decode(std::string(jwt_sv.data(), jwt_sv.size()), Base64::DO_LAX);
   
  auto obj = nlohmann::json::parse(ss);
  auto name = obj["sub"].get<std::string>();
  if (name.empty())
      spdlog::error("没有user_name");

  if (!cai.inited)
  {
    std::thread t([] {spdlog::info("update线程启动"); update_time.run(); });
    t.detach();
    cai.inited = true;
  }
  cai.low_limiter = 90000;
  cai.user_count = 8000;     // 
  cai.total_quota = 18888;    // 总拍卖额度
  cai.start_time = get_current_time(); // 拍卖会起始时间
  cai.end_time = get_end_time();   // 拍卖会结束时间
  cai.update_time= get_update_time();// 第二轮开始时间

  // 保存用户
  auto key = get_client_key(req);
  if (key.length() > 6)
    user_map_[get_client_key(req)] = { std::string(name.data(), name.size()) };
  std::string res = fmt::format("CONNECTED\nversion:1.2\n"
    "heart-beat:10000,10000\n"
    "user-name:{}\n\n", name.c_str());
  res.append("1");  // 在结尾追加一个'\0'
  res[res.length() - 1] = 0;
  req.get_conn<cinatra::NonSSL>()->send_ws_string(std::move(res));
}

void wss_t::response_error(request& req, std::string_view msg)
{
  nlohmann::json jcont;
  jcont["code"] = "InvalidRequest";
  jcont["message"] = std::string(msg.data(), msg.size());
  std::string content = jcont.dump();

  auto uri = "/user/queue/errors"sv;
  std::string title = "MESSAGE\n";
  title += "destination:" + std::string(uri.data(), uri.size());
  title += "\ncontent-type:application/json\n";
  title += "subscription:" + get_sub_id(req, uri);
  title += "\nmessage-id:dbbbf7f8-f207-8b31-cb80-f43aa2b2f966-3993\n";
  title += "content-length:";
  title += std::to_string(content.length());
  title += "\n\n";

  std::string str = std::string{ title.c_str(), title.length() };
  str += std::string{ content.c_str(), content.length() + 1};

  req.get_conn<cinatra::NonSSL>()->send_ws_string(std::move(str));
}

std::string_view wss_t::get_title_value(std::string_view name, std::string_view data)
{
  auto start = data.find(name.data());
  if (start == std::string_view::npos)
    return "";

  start += name.size();
  auto end = data.find_first_of('\n', start);
  if (end == std::string_view::npos || end <= start)
    return "";

  return data.substr(start, end - start);
}

std::string_view wss_t::get_content(size_t length, std::string_view data)
{
  auto name = "\n\n"sv;
  auto start = data.find(name.data());
  if (start == std::string_view::npos)
    return "";
  start += name.size();
  return data.substr(start, length);
}

std::string_view wss_t::to_subscribe(std::string_view cmd)
{
  if (cmd._Starts_with("/app/bidcaptcha"))
    return "/user/queue/bidcaptcha";

  spdlog::error("未匹配到定阅:{}", cmd);
  return "";
}

void wss_t::ret_captcha_img(request& req
  , std::string_view cmduri, std::string aid, size_t amount)
{
  std::string_view path= "./www/captcha/price/1.png";
  if (!std::filesystem::exists(path))
  {
    response_error(req, u8"没有验证码");
    return;
  }
  auto ifs_ptr = std::make_shared<std::ifstream>(path, std::ios_base::binary);
  ifs_ptr->seekg(0, std::ios_base::end);
  auto nFileLen = ifs_ptr->tellg();

  auto uri = to_subscribe(cmduri);
  std::string title = "MESSAGE\n";
  title += "auction-id:" + aid + "\n";
  title += "amount:" + std::to_string(amount) +"\n";
  title += u8"prompt:从左至右输入大于2的4位红色数字\n";
  title += "destination:" + std::string(uri.data(), uri.size()) +"\n";
  title += "content-type:application/octet-stream\n";
  title += "subscription:" + get_sub_id(req, uri) + "\n";
  title += "message-id:dbbbf7f8-f207-8b31-cb80-f43aa2b2f966-3993\n";
  title += "content-length:";
  title += std::to_string(nFileLen);
  title += "\n\n";
  std::stringstream file_buffer;
  file_buffer << title;
  ifs_ptr->seekg(0, std::ios_base::beg);
  file_buffer << ifs_ptr->rdbuf();
  file_buffer << '\0';
  req.get_conn<cinatra::NonSSL>()->send_ws_binary(std::move(file_buffer.str()));
}

std::string wss_t::now_date()
{
  time_t tick = time(0);
  tm temptm;
  localtime_s(&temptm, &tick);
  SYSTEMTIME wtm = {(WORD)(1900 + temptm.tm_year), (WORD)(temptm.tm_mon + 1),
    (WORD)temptm.tm_wday, (WORD)temptm.tm_mday, (WORD)temptm.tm_hour,
    (WORD)temptm.tm_min, (WORD)temptm.tm_sec, 0 };

  return fmt::format(u8"{}年{:0>2}月{:0>2}日", wtm.wYear, wtm.wMonth, wtm.wDay);
}

std::string wss_t::get_current_time()
{
  time_t tick = time(0);
  tick -= 8 * 60 * 60;
  tm temptm;
  localtime_s(&temptm, &tick);
  SYSTEMTIME wtm = {(WORD)(1900 + temptm.tm_year), (WORD)(temptm.tm_mon + 1),
    (WORD)temptm.tm_wday, (WORD)temptm.tm_mday, (WORD)temptm.tm_hour,
    (WORD)temptm.tm_min, (WORD)temptm.tm_sec, 0 };
  std::string sdate = fmt::format(u8"{}-{:0>2}-{:0>2}T", wtm.wYear, wtm.wMonth, wtm.wDay);
  std::string stime = fmt::format(u8"{:0>2}:{:0>2}:{:0>2}Z", wtm.wHour, wtm.wMinute, wtm.wSecond);
  return sdate + stime;
}

std::string wss_t::get_end_time()
{
  time_t tick = time(0);
  tick -= 7 * 60 * 60;
  tm temptm;
  localtime_s(&temptm, &tick);
  SYSTEMTIME wtm = {(WORD)(1900 + temptm.tm_year), (WORD)(temptm.tm_mon + 1),
    (WORD)temptm.tm_wday, (WORD)temptm.tm_mday, (WORD)temptm.tm_hour,
    (WORD)temptm.tm_min, (WORD)temptm.tm_sec, 0 };
  std::string sdate = fmt::format(u8"{}-{:0>2}-{:0>2}T", wtm.wYear, wtm.wMonth, wtm.wDay);
  std::string stime = fmt::format(u8"{:0>2}:{:0>2}:{:0>2}Z", wtm.wHour, wtm.wMinute, wtm.wSecond);
  return sdate + stime;
}

std::string wss_t::get_update_time()
{
  time_t tick = time(0);
  tick -= 8 * 60 * 60;
  tm temptm;
  localtime_s(&temptm, &tick);
  SYSTEMTIME wtm = {(WORD)(1900 + temptm.tm_year), (WORD)(temptm.tm_mon + 1),
    (WORD)temptm.tm_wday, (WORD)temptm.tm_mday, (WORD)temptm.tm_hour,
    (WORD)temptm.tm_min, (WORD)temptm.tm_sec, 0 };
  std::string sdate = fmt::format(u8"{}-{:0>2}-{:0>2}T", wtm.wYear, wtm.wMonth, wtm.wDay);
  int min = (wtm.wMinute + 30) % 60;
  int hour = (wtm.wMinute + 30) / 60;
  std::string stime = fmt::format(u8"{:0>2}:{:0>2}:{:0>2}Z", wtm.wHour+hour, min, wtm.wSecond);
  return sdate + stime;
}

std::string wss_t::get_sub_id(request& req, std::string_view uri)
{
  auto& item = user_map_[get_client_key(req)];
  std::string s(uri.data(), uri.size());
  return item.subinfo[s];
}

std::string wss_t::get_bid_num(request& req)
{
  auto& item = user_map_[get_client_key(req)];
  return item.bidname;
}

//////////////////////////////////////////////////////////////////////////
// 定阅处理
void wss_t::handle_app_auction(request& req, std::string_view uri)
{
  spdlog::info("enter {}", uri);
  auto sname = now_date();
  sname += u8"上海市单位非营业性客车额度拍卖会";

  nlohmann::json jcont;
  jcont[0]["id"] = "139";
  jcont[0]["version"] = 0;
  jcont[0]["type"] = cai.type;
  jcont[0]["name"] = sname;
  jcont[0]["startTime"] = cai.start_time;
  jcont[0]["endTime"] = cai.end_time;
  jcont[0]["updateTime"] = cai.update_time;
  jcont[0]["quota"] = cai.total_quota;
  jcont[0]["lowerLimit"] = cai.low_limiter;
  jcont[0]["priceUp"] = 300;
  jcont[0]["priceDown"] = 300;
  std::string content = jcont.dump();

  std::string title = "MESSAGE\n";
  title += "destination:" + std::string(uri.data(), uri.size());
  title += "\ncontent-type:application/json\n";
  title += "subscription:" + get_sub_id(req, uri);
  title += "\nmessage-id:dbbbf7f8-f207-8b31-cb80-f43aa2b2f966-3995\n";
  title += "content-length:";
  title += std::to_string(content.length());
  title += "\n\n";

  std::string str = std::string{ title.c_str(), title.length() };
  str += std::string{ content.c_str(), content.length() + 1};
  
  req.get_conn<cinatra::NonSSL>()->send_ws_string(std::move(str));
}

void wss_t::handle_mybid_app_auction(request& req, std::string_view uri, size_t amount)
{
  spdlog::info("enter {}", uri);
  nlohmann::json jcont;
  jcont[0]["auctionId"] = "139";
  jcont[0]["bidAmount"] = amount;
  jcont[0]["bidnumber"] = get_bid_num(req);
  jcont[0]["bidTime"] = get_current_time();
  jcont[0]["bidCount"] = 1;
  jcont[0]["dealTime"] = get_current_time();
  jcont[0]["type"] = "web";
  jcont[0]["msg"] = u8"出价有效";
  std::string content = jcont.dump();

  std::string title = "MESSAGE\n";
  title += "destination:"+std::string(uri.data(), uri.size());
  title += "\ncontent-type:application/json\n";
  title += "subscription:" + get_sub_id(req, uri);
  title += "\nmessage-id:dbbbf7f8-f207-8b31-cb80-f43aa2b2f966-3996\n";
  title += "content-length:";
  title += std::to_string(content.length());
  title += "\n\n";

  std::string str = std::string{ title.c_str(), title.length() };
  str += std::string{ content.c_str(), content.length() + 1};

  req.get_conn<cinatra::NonSSL>()->send_ws_string(std::move(str));
}

asio::awaitable<void> wss_t::co_topic_auction(std::shared_ptr<connection<NonSSL>> connection,
  std::string sub_id, std::string uri)
{
  auto key = get_client_key(connection);
  spdlog::info("开始更新[{}]", key);
  auto executor = co_await asio::this_coro::executor;
  auto& item = user_map_[key];
  asio::steady_timer timer(executor);
  for (;item.start_update;)
  {
    std::string str_current_time = get_current_time();
    nlohmann::json jcont;
    jcont["auctionId"] = "139";
    jcont["auctionVersion"] = 0;
    jcont["status"] = "Bid";
    jcont["systemTime"] = str_current_time;
    jcont["numberOfBidUsers"] = cai.user_count;
    jcont["basePrice"] = cai.low_limiter;
    jcont["basePriceTime"] = str_current_time;
    std::string content = jcont.dump();

    // 更新用户数与最低成交价
    if (str_current_time < cai.update_time)
    {
      if (cai.user_count < 150000)
        cai.user_count += 1;
      if (cai.low_limiter < 200000)
        cai.low_limiter += 100;
    }

    std::string title = "MESSAGE\n";
    title += "destination:" + uri;
    title += "\ncontent-type:application/json\n";
    title += "subscription:" + sub_id;
    title += "\nmessage-id:szX9zLXE\n";
    title += "content-length:";
    title += std::to_string(content.length());
    title += "\n\n";

    std::string str = std::string{ title.c_str(), title.length() };
    str += std::string{ content.c_str(), content.length() + 1 };

    connection->send_ws_string(std::move(str));
    asio::error_code ec;
    timer.expires_from_now(1s);
    co_await timer.async_wait(asio::redirect_error(asio::use_awaitable, ec));
  }
  spdlog::info("停止更新[{}]", key);
  co_return;
}

void wss_t::handle_user_queue_bid_updates(request& req, size_t amount, std::string_view msg, std::string_view id, bool isok)
{
  //MESSAGE
  //destination:/user/queue/bid-updates
  //content-type:application/json
  //subscription:sub-3
  //message-id:de9c112c-892d-b9f3-f336-d4f29a31d92f-3949
  //content-length:146
  nlohmann::json jcont;
  jcont["auctionId"] = "139";
  jcont["requestId"] = std::string(id.data(), id.size());
  if (isok)
  {
    jcont["type"] = "enqueue";
    jcont["code"] = 0;
  }
  else {
    jcont["type"] = "deal";
    jcont["code"] = 0;
    jcont["bidCount"] = 1;
  }
  jcont["message"] = std::string(msg.data(), msg.size());
  jcont["bidAmount"] = amount;
  jcont["time"] = get_current_time();
  jcont["bidType"] = "web";
  std::string content = jcont.dump();

  auto uri = "/user/queue/bid-updates"sv;
  std::string title = "MESSAGE\n";
  title += "destination:"+std::string(uri.data(), uri.size());
  title += "\ncontent-type:application/json\n";
  title += "subscription:" + get_sub_id(req, uri);
  title += "\nmessage-id:dbbbf7f8-f207-8b31-cb80-f43aa2b2f965-3994\n";
  title += "content-length:";
  title += std::to_string(content.length());
  title += "\n\n";

  std::string str = std::string{ title.c_str(), title.length() };
  str += std::string{ content.c_str(), content.length() + 1};

  req.get_conn<cinatra::NonSSL>()->send_ws_string(std::move(str));
}

void wss_t::handle_topic_auction(request& req, std::string_view uri)
{
  spdlog::info("enter {}", uri);
  auto& item = user_map_[get_client_key(req)];
  item.start_update = true;
  asio::error_code ec;
  asio::co_spawn(update_time.get_executor(), [=, conn = req.get_conn<cinatra::NonSSL>(), 
    url = std::string(uri.data(), uri.size()), sub_id = get_sub_id(req, uri)]() mutable {
    return co_topic_auction(conn, std::move(sub_id), std::move(url));}, 
    asio::redirect_error(asio::detached, ec));
}

void wss_t::handle_app_cardinfo(request& req, std::string_view uri)
{
  spdlog::info("enter {}", uri);

  nlohmann::json jcont;
  jcont["name"] = u8"模拟用户";
  jcont["bidnumber"] = get_bid_num(req);
  jcont["bidcount"] = 2;
  jcont["validdate"] = "2021-05-31";
  std::string content = jcont.dump();

  std::string title = "MESSAGE\n";
  title += "destination:"+std::string(uri.data(), uri.size());
  title += "\ncontent-type:application/json\n";
  title += "subscription:" + get_sub_id(req, uri);
  title += "\nmessage-id:dbbbf7f8-f207-8b31-cb80-f43aa2b2f966-3984\n";
  title += "content-length:";
  title += std::to_string(content.length());
  title += "\n\n";

  std::string str = std::string{ title.c_str(), title.length() };
  str += std::string{ content.c_str(), content.length() + 1};
  req.get_conn<cinatra::NonSSL>()->send_ws_string(std::move(str));
}

/*
SUBSCRIBE
id:sub-5
destination:/app/cardinfo
destination:/app/auction
destination:/user/queue/bidcaptcha
destination:/user/queue/bid-updates
destination:/user/queue/errors
destination:/user/queue/messages
destination:/topic/auctions/139
destination:/app/mybid/auctions/139
*/
void wss_t::handle_subscribe(request& req)
{
  auto part_data = req.get_part_data();
  if (part_data.empty())
  {
    spdlog::error("SUBSCRIBE格式错误,无数据");
    return;
  }
  auto uri = get_title_value("destination:", part_data);
  if (uri.empty())
  {
    spdlog::error("SUBSCRIBE格式错误,无destination");
    return;
  }
  auto subid = get_title_value("id:", part_data);
  if (subid.empty())
  {
    spdlog::error("SUBSCRIBE格式错误,无sub-id");
    return;
  }
  auto& item = user_map_[get_client_key(req)];
  item.subinfo[std::string(uri.data(), uri.size())] = std::string(subid.data(), subid.size());
  if (stringnicmp(uri.data(), "/app/cardinfo"))
  {
    // 返回登录的标书信息
    handle_app_cardinfo(req, uri);
  }else if (stringnicmp(uri.data(), "/app/auction"))
  {
    // 返回本场拍卖会信息
    handle_app_auction(req, uri);
  }else if (stringnicmp(uri.data(), "/user/queue/bidcaptcha"))
  {
    // 出价验证
    // 当收到SEND /app/bidcaptcha时触发该定阅并返回验证码图片.
  }else if (stringnicmp(uri.data(), "/user/queue/bid-updates"))
  {
    // 正在等待出价入列
  }else if (stringnicmp(uri.data(), "/user/queue/errors"))
  {
    // 出错时触发该定阅返回
  }else if (stringnicmp(uri.data(), "/user/queue/messages"))
  {
    // 拍卖后表明自己是否中标
  }else if (stringnicmp(uri.data(), "/topic/auctions/139"))
  {
    // 1: 拍卖期间返回拍卖信息
    // 2: 结束后返回状态信息 成交信息
    handle_topic_auction(req, uri);
  }else if (stringnicmp(uri.data(), "/app/mybid/auctions/139"))
  {
    // 这里不用处理,等到实际出价时再处理.
    //handle_mybid_app_auction(req, uri);
  }
}

void wss_t::handle_unsubscribe(request& req)
{
  auto part_data = req.get_part_data();
  spdlog::info("{}", std::string(part_data.data(), part_data.size()));
}

void wss_t::handle_disconnect(request& req)
{
  if (auto key = get_client_key(req);key.length() > 6)
  {
    user_map_[key].start_update = false;
  }

  //DISCONNECT
  //  receipt:close-8
  auto part_data = req.get_part_data();
  auto close_id = get_title_value("receipt:", part_data);
  if (close_id.empty())
  {
    spdlog::error("DISCONNECT格式错误,无receipt");
    return;
  }

  //RECEIPT
  //  receipt-id:close-8
  std::string str = "RECEIPT\n";
  str += "receipt-id:";
  str += std::string{ close_id.data(), close_id.size() + 1};
  req.get_conn<cinatra::NonSSL>()->send_ws_string(std::move(str));
}

void wss_t::handle_client_send(request& req)
{
//SEND
//destination:/app/bidcaptcha
//content-type:application/json
//content-length:32
//{"auctionId":"139","amount":100}
  auto part_data = req.get_part_data();
  auto length = get_title_value("content-length:", part_data);
  std::string len(length.data(), length.size());
  auto content = get_content(atoi(len.c_str()), part_data);
  if (content.empty())
  {
    spdlog::error("bidcaptcha格式错误,无出价");
    return;
  }

  auto js = nlohmann::json::parse(content);
  size_t amount = js["amount"].get<size_t>();
  std::string auction_id = js["auctionId"].get<std::string>();
  std::string answer;
  if (js.contains("captchaAnswer"))
  {
    answer = js["captchaAnswer"].get<std::string>();
  }

  auto client_uri = get_title_value("destination:", part_data);
  if (client_uri.empty())
  {
    spdlog::error("bidcaptcha格式错误,无destination");
    return;
  }
  if (answer.empty())
  {
    ret_captcha_img(req, client_uri, std::move(auction_id), amount);
  }
  else if (!stringnicmp(answer, "8934"))
  {
    response_error(req, u8"图像校验码错误！");
  }
  else
  {
    // 这里需要判断出价范围
    //......
    auto req_id  = get_title_value("request-id:", part_data);
    handle_user_queue_bid_updates(req, amount, u8"成功", req_id, true);
    handle_user_queue_bid_updates(req, amount, u8"出价有效", req_id, false);
  }
  return;
}

void wss_t::handle_client_error(request& req)
{
  if (auto key = get_client_key(req);key.length() > 6)
  {
    user_map_[key].start_update = false;
  }
  spdlog::info("websocket pack error or network error");
}