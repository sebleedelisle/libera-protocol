#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace libera::protocol {

inline constexpr std::uint32_t PROTOCOL_MAGIC = 0x4c494250u; // "LIBP"
inline constexpr std::uint8_t PROTOCOL_VERSION_MAJOR = 0;
inline constexpr std::uint8_t PROTOCOL_VERSION_MINOR = 1;
inline constexpr std::size_t RECORD_HEADER_SIZE = 12;
inline constexpr std::uint16_t DEFAULT_DISCOVERY_PORT = 45425;
inline constexpr std::uint16_t DEFAULT_SESSION_PORT = 45426;

enum class RecordType : std::uint16_t {
    Hello = 0x0001,
    Accept = 0x0002,
    Reject = 0x0003,
    Ready = 0x0004,
    Ack = 0x0005,
    Error = 0x0006,
    Ping = 0x0007,
    Pong = 0x0008,
    Close = 0x0009,

    StreamConfig = 0x0201,
    SetScannerSync = 0x0202,

    Points = 0x0101,
    FrameMarker = 0x0102,

    Status = 0x0301,

    ManufacturerPrivate = 0x8000,
};

enum class StreamMode : std::uint16_t {
    RawPointStream = 0,
    RawPointStreamWithOptionalMarkers = 1,
    MarkedPointStream = 2,
    FrameByCount = 3,
    FrameByNextMarker = 4,
};

enum SessionFeatureFlags : std::uint32_t {
    FeatureTargetBeginTime = 1u << 0,
    FeatureRepeatLastFrame = 1u << 1,
    FeatureScannerSync = 1u << 2,
    FeatureStatus = 1u << 3,
    FeatureManufacturerPrivate = 1u << 4,
};

enum class DecodeStatus {
    Complete,
    Incomplete,
    Invalid,
};

struct Record {
    RecordType type = RecordType::Error;
    std::uint16_t flags = 0;
    std::uint32_t sequence = 0;
    std::vector<std::uint8_t> payload;
};

struct RecordHeader {
    RecordType type = RecordType::Error;
    std::uint16_t flags = 0;
    std::uint32_t payloadSize = 0;
    std::uint32_t sequence = 0;
};

struct DecodeResult {
    DecodeStatus status = DecodeStatus::Incomplete;
    Record record;
    std::size_t bytesConsumed = 0;
    std::string error;
};

struct PointSample {
    std::int16_t x = 0;
    std::int16_t y = 0;
    std::uint16_t r = 0;
    std::uint16_t g = 0;
    std::uint16_t b = 0;
    std::uint16_t i = 0;
    std::vector<std::uint16_t> user;
};

struct FrameMarker {
    std::uint64_t frameId = 0;
    std::uint64_t targetBeginTimeNs = 0;
    std::uint32_t pointRate = 0;
    std::uint32_t framePointCount = 0;
    std::uint32_t flags = 0;
};

struct StreamConfig {
    std::uint32_t defaultPointRate = 0;
    StreamMode streamMode = StreamMode::RawPointStream;
    std::uint8_t userChannelCount = 0;
    std::uint16_t flags = 0;
};

struct Hello {
    std::string senderName;
    StreamMode requestedStreamMode = StreamMode::RawPointStream;
    std::uint8_t requestedUserChannelCount = 0;
    std::uint32_t defaultPointRate = 0;
};

struct Accept {
    StreamMode acceptedStreamMode = StreamMode::RawPointStream;
    std::uint8_t acceptedUserChannelCount = 0;
    std::uint32_t defaultPointRate = 0;
    std::uint32_t maxPointRate = 0;
    std::uint32_t maxFramePointCount = 0;
    std::uint32_t maxRecordPayloadSize = 0;
    std::uint64_t sessionId = 0;
    std::uint32_t featureFlags = 0;
};

struct Status {
    std::uint16_t code = 0;
    std::uint32_t queuedPoints = 0;
    std::uint32_t queuedFrames = 0;
    std::uint32_t featureFlags = 0;
    std::string message;
};

struct DiscoveryAdvertisement {
    std::string endpointId;
    std::string displayName;
    std::string endpointType;
    std::string address;
    std::uint16_t tcpPort = DEFAULT_SESSION_PORT;
    std::uint16_t supportedStreamModes = 0;
    std::uint8_t maxUserChannelCount = 0;
    std::uint32_t minPointRate = 0;
    std::uint32_t maxPointRate = 0;
    std::uint32_t maxFramePointCount = 0;
    std::uint32_t featureFlags = 0;
};

inline constexpr std::uint16_t streamModeMask(StreamMode mode) {
    return static_cast<std::uint16_t>(1u << static_cast<std::uint16_t>(mode));
}

} // namespace libera::protocol
