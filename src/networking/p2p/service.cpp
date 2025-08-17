#include "service.h"

#include <iostream>

namespace net
{
  namespace p2p
  {

    Service& Service::instance()
    {
      static Service s;
      return s;
    }

    void Service::set_client_session(Session::Ptr s)
    {
      std::lock_guard<std::mutex> lk(mtx_);
      client_ = std::move(s);
    }

    void Service::set_server_session(Session::Ptr s)
    {
      std::lock_guard<std::mutex> lk(mtx_);
      server_ = std::move(s);
      if (server_)
      {
        auto cb = on_server_message_;
        server_->set_on_message(
            [cb](const std::string& line)
            {
              if (cb)
              {
                cb(line);
              }
              else
              {
                std::cerr << "[p2p][server] got: " << line << std::endl;
              }
            });
      }
    }

    Session::Ptr Service::client_session()
    {
      std::lock_guard<std::mutex> lk(mtx_);
      return client_;
    }

    Session::Ptr Service::server_session()
    {
      std::lock_guard<std::mutex> lk(mtx_);
      return server_;
    }

    void Service::set_on_server_message(std::function<void(const std::string&)> cb)
    {
      std::lock_guard<std::mutex> lk(mtx_);
      on_server_message_ = std::move(cb);
      if (server_)
      {
        server_->set_on_message(on_server_message_);
      }
    }

    void Service::send_to_peer_tcp(const std::string& line)
    {
      Session::Ptr c;
      Session::Ptr s;
      {
        std::lock_guard<std::mutex> lk(mtx_);
        c = client_;
        s = server_;
      }
      if (c)
      {
        c->send_line(line);
        return;
      }
      if (s)
      {
        s->send_line(line);
      }
    }

    void Service::send_to_peer_udp(const std::string& line)
    {
      Session::Ptr c;
      Session::Ptr s;
      {
        std::lock_guard<std::mutex> lk(mtx_);
        c = client_;
        s = server_;
      }
      // UDP not available; fall back to TCP
      if (c)
      {
        c->send_line(line);
        return;
      }
      if (s)
      {
        s->send_line(line);
      }
    }

    void Service::stop()
    {
      std::lock_guard<std::mutex> lk(mtx_);
  on_server_message_ = nullptr;
      client_.reset();
      server_.reset();
    }

  } // namespace p2p
} // namespace net
