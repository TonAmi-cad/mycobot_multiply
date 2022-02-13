#include "mycobot/process_received.hpp"

#include <fp/all.hpp>
#include <range/v3/all.hpp>
#include <string>
#include <utility>

#include "mycobot/bytearray.hpp"
#include "mycobot/protocol_code.hpp"
#include "mycobot/serialize.hpp"

namespace views = ::ranges::views;

namespace mycobot {

bytearray process_ssid_pwd_response(bytearray const& data) {
  // TODO: implement this
  return data;
}

fp::Result<std::pair<size_t, size_t>> process_header(bytearray const& data,
                                                     ProtocolCode genre) {
  // Get valid header: 0xfe0xfe<data_len><id == genre>
  constexpr size_t kHeaderSize = 4;
  constexpr size_t kHeaderPos = 0;
  constexpr size_t kDataLenPos = 2;
  constexpr size_t kCmdId = 3;

  auto header_view =
      data | views::enumerate | views::sliding(kHeaderSize) |
      views::filter([&](auto const& view) {
        auto const header = view | ranges::to<std::vector>();
        return header.at(kHeaderPos).second == ProtocolCode::HEADER &&
               header.at(kHeaderPos + 1).second == ProtocolCode::HEADER &&
               header.at(kCmdId).second == genre;
      }) |
      views::take(1);

  if (ranges::empty(header_view)) {
    return tl::make_unexpected(fp::NotFound("Header not found"));
  }

  auto header = ranges::front(header_view) | ranges::to<std::vector>();
  size_t start_index = header.at(kHeaderPos).first + kHeaderSize;
  size_t data_len = header.at(kDataLenPos).second;
  ProtocolCode id = static_cast<ProtocolCode>(header.at(kCmdId).second);

  if (id == ProtocolCode::GET_BASIC_INPUT ||
      id == ProtocolCode::GET_DIGITAL_INPUT) {
    return std::make_pair(start_index + 1, data_len - 1);
  }

  return std::make_pair(start_index, data_len);
}

std::vector<int16_t> process_command(bytearray const& data,
                                     ProtocolCode genre) {
  if (data.size() == 12 || data.size() == 8) {
    return data | views::chunk(2) | views::transform([](auto const& view) {
             return decode_int16(view | ranges::to<bytearray>());
           }) |
           ranges::to<std::vector>();
  } else if (data.size() == 2) {
    if (genre == ProtocolCode::IS_SERVO_ENABLE) {
      return std::vector<int16_t>(
          1, static_cast<int16_t>(decode_int8(bytearray{1, data.at(1)})));
    }
    return std::vector<int16_t>(1, decode_int16(data));
  }

  return std::vector<int16_t>(
      1, static_cast<int16_t>(decode_int8(bytearray{1, data.at(0)})));
}

fp::Result<std::vector<int16_t>> process_received(bytearray const& data,
                                                  ProtocolCode genre) {
  if (genre == ProtocolCode::GET_SSID_PWD) {
    auto const pwd = process_ssid_pwd_response(data);
    return std::vector<int16_t>(pwd.begin(), pwd.end());
  }

  if (data.size() == 0) {
    return {};
  }

  // Process the header
  auto const header_result = process_header(data, genre);
  if (!header_result) {
    return tl::make_unexpected(header_result.error());
  }
  auto const& [start_index, data_len] = header_result.value();

  // Process the command_data
  return process_command(data |
                             views::slice(start_index, start_index + data_len) |
                             ranges::to<bytearray>(),
                         genre);
}

}  // namespace mycobot