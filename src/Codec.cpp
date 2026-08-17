#include "libera/protocol/Codec.hpp"

#include <algorithm>
#include <utility>

namespace libera::protocol {
namespace {

constexpr std::size_t frameMarkerPayloadSize = 28;
constexpr std::size_t streamConfigPayloadSize = 10;
constexpr std::size_t helloFixedPayloadSize = 16;
constexpr std::size_t acceptPayloadSize = 32;
constexpr std::size_t statusFixedPayloadSize = 18;
constexpr std::size_t discoveryFixedPayloadSize = 36;

std::uint16_t clampedStringSize(const std::string& value) {
    return static_cast<std::uint16_t>(std::min<std::size_t>(value.size(), 65535u));
}

void appendStringBytes(std::vector<std::uint8_t>& output,
                       const std::string& value,
                       std::uint16_t size) {
    output.insert(output.end(), value.begin(), value.begin() + size);
}

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

bool decodeRecordHeader(const std::uint8_t* data,
                        std::size_t size,
                        RecordHeader& header,
                        std::string& error) {
    if (data == nullptr) {
        error = "null record buffer";
        return false;
    }
    if (size < RECORD_HEADER_SIZE) {
        error = "incomplete record header";
        return false;
    }

    header.type = static_cast<RecordType>(readUInt16(data));
    header.flags = readUInt16(data + 2);
    header.payloadSize = readUInt32(data + 4);
    header.sequence = readUInt32(data + 8);
    return true;
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

    RecordHeader header;
    std::string error;
    if (!decodeRecordHeader(data, size, header, error)) {
        result.status = DecodeStatus::Invalid;
        result.error = error;
        return result;
    }

    const std::size_t totalSize = RECORD_HEADER_SIZE + static_cast<std::size_t>(header.payloadSize);
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
    result.record.type = header.type;
    result.record.flags = header.flags;
    result.record.sequence = header.sequence;
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
    const auto nameSize = clampedStringSize(hello.senderName);
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
    appendStringBytes(output, hello.senderName, nameSize);
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

std::vector<std::uint8_t> encodeAccept(const Accept& accept) {
    std::vector<std::uint8_t> output;
    output.reserve(acceptPayloadSize);
    appendUInt16(output, static_cast<std::uint16_t>(accept.acceptedStreamMode));
    output.push_back(accept.acceptedUserChannelCount);
    output.push_back(0);
    appendUInt32(output, accept.defaultPointRate);
    appendUInt32(output, accept.maxPointRate);
    appendUInt32(output, accept.maxFramePointCount);
    appendUInt32(output, accept.maxRecordPayloadSize);
    appendUInt64(output, accept.sessionId);
    appendUInt32(output, accept.featureFlags);
    return output;
}

bool decodeAccept(const std::uint8_t* data,
                  std::size_t size,
                  Accept& accept,
                  std::string& error) {
    if (data == nullptr || size != acceptPayloadSize) {
        error = "invalid accept payload size";
        return false;
    }
    accept.acceptedStreamMode = static_cast<StreamMode>(readUInt16(data));
    accept.acceptedUserChannelCount = data[2];
    accept.defaultPointRate = readUInt32(data + 4);
    accept.maxPointRate = readUInt32(data + 8);
    accept.maxFramePointCount = readUInt32(data + 12);
    accept.maxRecordPayloadSize = readUInt32(data + 16);
    accept.sessionId = readUInt64(data + 20);
    accept.featureFlags = readUInt32(data + 28);
    return true;
}

std::vector<std::uint8_t> encodeStatus(const Status& status) {
    const auto messageSize = clampedStringSize(status.message);
    std::vector<std::uint8_t> output;
    output.reserve(statusFixedPayloadSize + messageSize);
    appendUInt16(output, status.code);
    appendUInt16(output, 0);
    appendUInt32(output, status.queuedPoints);
    appendUInt32(output, status.queuedFrames);
    appendUInt32(output, status.featureFlags);
    appendUInt16(output, messageSize);
    appendStringBytes(output, status.message, messageSize);
    return output;
}

bool decodeStatus(const std::uint8_t* data,
                  std::size_t size,
                  Status& status,
                  std::string& error) {
    if (data == nullptr || size < statusFixedPayloadSize) {
        error = "invalid status payload size";
        return false;
    }
    const auto messageSize = readUInt16(data + 16);
    if (size != statusFixedPayloadSize + static_cast<std::size_t>(messageSize)) {
        error = "status message length does not match payload size";
        return false;
    }
    status.code = readUInt16(data);
    status.queuedPoints = readUInt32(data + 4);
    status.queuedFrames = readUInt32(data + 8);
    status.featureFlags = readUInt32(data + 12);
    status.message.assign(reinterpret_cast<const char*>(data + statusFixedPayloadSize),
                          messageSize);
    return true;
}

std::vector<std::uint8_t> encodeDiscoveryAdvertisement(
    const DiscoveryAdvertisement& advertisement) {
    const auto endpointIdSize = clampedStringSize(advertisement.endpointId);
    const auto displayNameSize = clampedStringSize(advertisement.displayName);
    const auto endpointTypeSize = clampedStringSize(advertisement.endpointType);
    const auto addressSize = clampedStringSize(advertisement.address);

    std::vector<std::uint8_t> output;
    output.reserve(discoveryFixedPayloadSize + endpointIdSize + displayNameSize +
                   endpointTypeSize + addressSize);
    appendUInt32(output, PROTOCOL_MAGIC);
    output.push_back(PROTOCOL_VERSION_MAJOR);
    output.push_back(PROTOCOL_VERSION_MINOR);
    appendUInt16(output, advertisement.tcpPort);
    appendUInt16(output, advertisement.supportedStreamModes);
    output.push_back(advertisement.maxUserChannelCount);
    output.push_back(0);
    appendUInt32(output, advertisement.minPointRate);
    appendUInt32(output, advertisement.maxPointRate);
    appendUInt32(output, advertisement.maxFramePointCount);
    appendUInt32(output, advertisement.featureFlags);
    appendUInt16(output, endpointIdSize);
    appendUInt16(output, displayNameSize);
    appendUInt16(output, endpointTypeSize);
    appendUInt16(output, addressSize);
    appendStringBytes(output, advertisement.endpointId, endpointIdSize);
    appendStringBytes(output, advertisement.displayName, displayNameSize);
    appendStringBytes(output, advertisement.endpointType, endpointTypeSize);
    appendStringBytes(output, advertisement.address, addressSize);
    return output;
}

bool decodeDiscoveryAdvertisement(const std::uint8_t* data,
                                  std::size_t size,
                                  DiscoveryAdvertisement& advertisement,
                                  std::string& error) {
    if (data == nullptr || size < discoveryFixedPayloadSize) {
        error = "invalid discovery advertisement payload size";
        return false;
    }
    if (readUInt32(data) != PROTOCOL_MAGIC) {
        error = "invalid discovery magic";
        return false;
    }
    if (data[4] != PROTOCOL_VERSION_MAJOR) {
        error = "unsupported discovery protocol major version";
        return false;
    }

    const auto endpointIdSize = readUInt16(data + 28);
    const auto displayNameSize = readUInt16(data + 30);
    const auto endpointTypeSize = readUInt16(data + 32);
    const auto addressSize = readUInt16(data + 34);
    const auto expectedSize = discoveryFixedPayloadSize +
                              static_cast<std::size_t>(endpointIdSize) +
                              static_cast<std::size_t>(displayNameSize) +
                              static_cast<std::size_t>(endpointTypeSize) +
                              static_cast<std::size_t>(addressSize);
    if (size != expectedSize) {
        error = "discovery string lengths do not match payload size";
        return false;
    }

    advertisement.tcpPort = readUInt16(data + 6);
    advertisement.supportedStreamModes = readUInt16(data + 8);
    advertisement.maxUserChannelCount = data[10];
    advertisement.minPointRate = readUInt32(data + 12);
    advertisement.maxPointRate = readUInt32(data + 16);
    advertisement.maxFramePointCount = readUInt32(data + 20);
    advertisement.featureFlags = readUInt32(data + 24);

    const std::uint8_t* cursor = data + discoveryFixedPayloadSize;
    advertisement.endpointId.assign(reinterpret_cast<const char*>(cursor), endpointIdSize);
    cursor += endpointIdSize;
    advertisement.displayName.assign(reinterpret_cast<const char*>(cursor), displayNameSize);
    cursor += displayNameSize;
    advertisement.endpointType.assign(reinterpret_cast<const char*>(cursor), endpointTypeSize);
    cursor += endpointTypeSize;
    advertisement.address.assign(reinterpret_cast<const char*>(cursor), addressSize);
    return true;
}

} // namespace libera::protocol
