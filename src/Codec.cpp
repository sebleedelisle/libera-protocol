#include "libera/protocol/Codec.hpp"

#include <algorithm>
#include <utility>

namespace libera::protocol {
namespace {

constexpr std::size_t frameMarkerPayloadSize = 28;
constexpr std::size_t streamConfigPayloadSize = 10;
constexpr std::size_t helloFixedPayloadSize = 16;

} // namespace

void appendUInt16(std::vector<std::uint8_t>& output, std::uint16_t value) {
    output.push_back(static_cast<std::uint8_t>((value >> 8) & 0xffu));
    output.push_back(static_cast<std::uint8_t>(value & 0xffu));
}

void appendUInt32(std::vector<std::uint8_t>& output, std::uint32_t value) {
    output.push_back(static_cast<std::uint8_t>((value >> 24) & 0xffu));
    output.push_back(static_cast<std::uint8_t>((value >> 16) & 0xffu));
    output.push_back(static_cast<std::uint8_t>((value >> 8) & 0xffu));
    output.push_back(static_cast<std::uint8_t>(value & 0xffu));
}

void appendUInt64(std::vector<std::uint8_t>& output, std::uint64_t value) {
    appendUInt32(output, static_cast<std::uint32_t>((value >> 32) & 0xffffffffull));
    appendUInt32(output, static_cast<std::uint32_t>(value & 0xffffffffull));
}

std::uint16_t readUInt16(const std::uint8_t* data) {
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(data[0]) << 8) |
                                      static_cast<std::uint16_t>(data[1]));
}

std::uint32_t readUInt32(const std::uint8_t* data) {
    return (static_cast<std::uint32_t>(data[0]) << 24) |
           (static_cast<std::uint32_t>(data[1]) << 16) |
           (static_cast<std::uint32_t>(data[2]) << 8) |
           static_cast<std::uint32_t>(data[3]);
}

std::uint64_t readUInt64(const std::uint8_t* data) {
    return (static_cast<std::uint64_t>(readUInt32(data)) << 32) |
           static_cast<std::uint64_t>(readUInt32(data + 4));
}

std::vector<std::uint8_t> encodeRecord(const Record& record) {
    std::vector<std::uint8_t> output;
    output.reserve(RECORD_HEADER_SIZE + record.payload.size());
    appendUInt16(output, static_cast<std::uint16_t>(record.type));
    appendUInt16(output, record.flags);
    appendUInt32(output, static_cast<std::uint32_t>(record.payload.size()));
    appendUInt32(output, record.sequence);
    output.insert(output.end(), record.payload.begin(), record.payload.end());
    return output;
}

DecodeResult decodeRecord(const std::uint8_t* data, std::size_t size) {
    DecodeResult result;
    if (data == nullptr) {
        result.status = DecodeStatus::Invalid;
        result.error = "null record buffer";
        return result;
    }
    if (size < RECORD_HEADER_SIZE) {
        result.status = DecodeStatus::Incomplete;
        return result;
    }

    const auto payloadSize = readUInt32(data + 4);
    const std::size_t totalSize = RECORD_HEADER_SIZE + static_cast<std::size_t>(payloadSize);
    if (totalSize < RECORD_HEADER_SIZE) {
        result.status = DecodeStatus::Invalid;
        result.error = "record size overflow";
        return result;
    }
    if (size < totalSize) {
        result.status = DecodeStatus::Incomplete;
        return result;
    }

    result.status = DecodeStatus::Complete;
    result.bytesConsumed = totalSize;
    result.record.type = static_cast<RecordType>(readUInt16(data));
    result.record.flags = readUInt16(data + 2);
    result.record.sequence = readUInt32(data + 8);
    result.record.payload.assign(data + RECORD_HEADER_SIZE, data + totalSize);
    return result;
}

std::size_t pointSampleSize(std::uint8_t userChannelCount) {
    return 12u + (static_cast<std::size_t>(userChannelCount) * 2u);
}

std::vector<std::uint8_t> encodePointSamples(const std::vector<PointSample>& points,
                                             std::uint8_t userChannelCount) {
    std::vector<std::uint8_t> output;
    output.reserve(points.size() * pointSampleSize(userChannelCount));
    for (const auto& point : points) {
        appendUInt16(output, static_cast<std::uint16_t>(point.x));
        appendUInt16(output, static_cast<std::uint16_t>(point.y));
        appendUInt16(output, point.r);
        appendUInt16(output, point.g);
        appendUInt16(output, point.b);
        appendUInt16(output, point.i);
        for (std::uint8_t i = 0; i < userChannelCount; ++i) {
            const std::uint16_t value =
                i < point.user.size() ? point.user[i] : static_cast<std::uint16_t>(0);
            appendUInt16(output, value);
        }
    }
    return output;
}

bool decodePointSamples(const std::uint8_t* data,
                        std::size_t size,
                        std::uint8_t userChannelCount,
                        std::vector<PointSample>& output,
                        std::string& error) {
    output.clear();
    if (data == nullptr && size != 0) {
        error = "null point payload";
        return false;
    }

    const std::size_t sampleSize = pointSampleSize(userChannelCount);
    if (sampleSize == 0 || (size % sampleSize) != 0) {
        error = "point payload is not aligned to negotiated sample size";
        return false;
    }

    const std::size_t pointCount = size / sampleSize;
    output.reserve(pointCount);
    const std::uint8_t* cursor = data;
    for (std::size_t i = 0; i < pointCount; ++i) {
        PointSample point;
        point.x = static_cast<std::int16_t>(readUInt16(cursor));
        cursor += 2;
        point.y = static_cast<std::int16_t>(readUInt16(cursor));
        cursor += 2;
        point.r = readUInt16(cursor);
        cursor += 2;
        point.g = readUInt16(cursor);
        cursor += 2;
        point.b = readUInt16(cursor);
        cursor += 2;
        point.i = readUInt16(cursor);
        cursor += 2;
        point.user.resize(userChannelCount);
        for (std::uint8_t channel = 0; channel < userChannelCount; ++channel) {
            point.user[channel] = readUInt16(cursor);
            cursor += 2;
        }
        output.push_back(std::move(point));
    }
    return true;
}

std::vector<std::uint8_t> encodeFrameMarker(const FrameMarker& marker) {
    std::vector<std::uint8_t> output;
    output.reserve(frameMarkerPayloadSize);
    appendUInt64(output, marker.frameId);
    appendUInt64(output, marker.targetBeginTimeNs);
    appendUInt32(output, marker.pointRate);
    appendUInt32(output, marker.framePointCount);
    appendUInt32(output, marker.flags);
    return output;
}

bool decodeFrameMarker(const std::uint8_t* data,
                       std::size_t size,
                       FrameMarker& marker,
                       std::string& error) {
    if (data == nullptr || size != frameMarkerPayloadSize) {
        error = "invalid frame marker payload size";
        return false;
    }
    marker.frameId = readUInt64(data);
    marker.targetBeginTimeNs = readUInt64(data + 8);
    marker.pointRate = readUInt32(data + 16);
    marker.framePointCount = readUInt32(data + 20);
    marker.flags = readUInt32(data + 24);
    return true;
}

std::vector<std::uint8_t> encodeStreamConfig(const StreamConfig& config) {
    std::vector<std::uint8_t> output;
    output.reserve(streamConfigPayloadSize);
    appendUInt32(output, config.defaultPointRate);
    appendUInt16(output, static_cast<std::uint16_t>(config.streamMode));
    output.push_back(config.userChannelCount);
    output.push_back(0);
    appendUInt16(output, config.flags);
    return output;
}

bool decodeStreamConfig(const std::uint8_t* data,
                        std::size_t size,
                        StreamConfig& config,
                        std::string& error) {
    if (data == nullptr || size != streamConfigPayloadSize) {
        error = "invalid stream config payload size";
        return false;
    }
    config.defaultPointRate = readUInt32(data);
    config.streamMode = static_cast<StreamMode>(readUInt16(data + 4));
    config.userChannelCount = data[6];
    config.flags = readUInt16(data + 8);
    return true;
}

std::vector<std::uint8_t> encodeHello(const Hello& hello) {
    const auto nameSize =
        static_cast<std::uint16_t>(std::min<std::size_t>(hello.senderName.size(), 65535u));
    std::vector<std::uint8_t> output;
    output.reserve(helloFixedPayloadSize + nameSize);
    appendUInt32(output, PROTOCOL_MAGIC);
    output.push_back(PROTOCOL_VERSION_MAJOR);
    output.push_back(PROTOCOL_VERSION_MINOR);
    appendUInt16(output, static_cast<std::uint16_t>(hello.requestedStreamMode));
    output.push_back(hello.requestedUserChannelCount);
    output.push_back(0);
    appendUInt32(output, hello.defaultPointRate);
    appendUInt16(output, nameSize);
    output.insert(output.end(), hello.senderName.begin(), hello.senderName.begin() + nameSize);
    return output;
}

bool decodeHello(const std::uint8_t* data,
                 std::size_t size,
                 Hello& hello,
                 std::string& error) {
    if (data == nullptr || size < helloFixedPayloadSize) {
        error = "invalid hello payload size";
        return false;
    }
    if (readUInt32(data) != PROTOCOL_MAGIC) {
        error = "invalid protocol magic";
        return false;
    }
    if (data[4] != PROTOCOL_VERSION_MAJOR) {
        error = "unsupported protocol major version";
        return false;
    }

    const auto nameSize = readUInt16(data + 14);
    if (size != helloFixedPayloadSize + static_cast<std::size_t>(nameSize)) {
        error = "hello name length does not match payload size";
        return false;
    }

    hello.requestedStreamMode = static_cast<StreamMode>(readUInt16(data + 6));
    hello.requestedUserChannelCount = data[8];
    hello.defaultPointRate = readUInt32(data + 10);
    hello.senderName.assign(reinterpret_cast<const char*>(data + helloFixedPayloadSize),
                            nameSize);
    return true;
}

} // namespace libera::protocol
