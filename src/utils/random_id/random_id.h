#pragma once

#include <random>
#include <string>

std::string get_random_id(std::size_t length = 16)
{
  const char charset[] = "0123456789"
                         "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                         "abcdefghijklmnopqrstuvwxyz";
  const std::size_t max_index = (sizeof(charset) - 1);
  std::string random_id;
  random_id.reserve(length);

  std::mt19937_64 rng{static_cast<unsigned long long>(time(nullptr))};
  std::uniform_int_distribution<std::size_t> dist(0, max_index - 1);

  for (std::size_t i = 0; i < length; ++i)
  {
    random_id += charset[dist(rng)];
  }

  return random_id;
}