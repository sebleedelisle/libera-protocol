#pragma once

#include "libera/protocol/Protocol.hpp"

#include <string>
#include <vector>

namespace libera::protocol {

void appendUInt16(std::vector<std::uint8_t>& output, std::uint16_t value);
void appendUInt32(std::vector<std::uint8_t>& output, std::uint32_t value);
void appendUInt64(std::vector<std::uint8_t>& output, std::uint64_t value);

std::uint16_t readUInt16(const std::uint8_t* data);
std::uint32_t readUInt32(const std::uint8_t* data);
std::uint64_t readUInt64(const std::uint8_t* data);

std::vector<std::uint8_t> encodeRecord(const Record& record);
DecodeResult decodeRecord(const std::uint8_t* data, std::size_t size);

std::size_t pointSampleSize(std::uint8_t userChannelCount);
std::vector<std::uint8_t> encodePointSamples(const std::vector<PointSample>& points,
                                             std::uint8_t userChannelCount);
bool decodePointSamples(const std::uint8_t* data,
                        std::size_t size,
                        std::uint8_t userChannelCount,
                        std::vector<PointSample>& output,
                        std::string& error);

std::vector<std::uint8_t> encodeFrameMarker(const FrameMarker& marker);
bool decodeFrameMarker(const std::uint8_t* data,
                       std::size_t size,
                       FrameMarker& marker,
                       std::string& error);

std::vector<std::uint8_t> encodeStreamConfig(const StreamConfig& config);
bool decodeStreamConfig(const std::uint8_t* data,
                        std::size_t size,
                        StreamConfig& config,
                        std::string& error);

std::vector<std::uint8_t> encodeHello(const Hello& hello);
bool decodeHello(const std::uint8_t* data,
                 std::size_t size,
                 Hello& hello,
                 std::string& error);

} // namespace libera::protocol

