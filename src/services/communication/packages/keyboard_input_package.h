#pragma once

#include "keyboard/input_event.h"
#include "services/communication/typed_package.h"

namespace services
{
  struct keyboard_input_package
  {
    static constexpr const char* __typename = "keyboard_input";

    keyboard::InputEvent event;

    inline std::string encode() const
    {
      return keyboard::InputEventJSONConverter::encode(event);
    }

    static inline keyboard_input_package decode(const std::string& s)
    {
      keyboard_input_package p{};
      p.event = keyboard::InputEventJSONConverter::decode(s);
      return p;
    }

    static bool is(services::typed_package pkg)
    {
      return pkg.__typename == __typename;
    }

    static inline typed_package build(const keyboard::InputEvent& e)
    {
      typed_package p{};
      p.__typename = __typename;
      keyboard_input_package kip;
      kip.event = e;
      p.payload = kip.encode();
      return p;
    }
  };
} // namespace services