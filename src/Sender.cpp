#include "libera/protocol/Sender.hpp"

#include "libera/protocol/Codec.hpp"

#include <utility>

namespace libera::protocol {

Sender::Sender(std::uint8_t userChannelCount)
    : userChannelCountValue(userChannelCount) {
}

void Sender::setUserChannelCount(std::uint8_t count) {
    userChannelCountValue = count;
}

std::uint8_t Sender::getUserChannelCount() const {
    return userChannelCountValue;
}

std::vector<std::uint8_t> Sender::makeHello(const Hello& hello) {
    return makeRecord(RecordType::Hello, encodeHello(hello));
}

std::vector<std::uint8_t> Sender::makeAccept(const Accept& accept) {
    return makeRecord(RecordType::Accept, encodeAccept(accept));
}

std::vector<std::uint8_t> Sender::makeReady() {
    return makeRecord(RecordType::Ready);
}

std::vector<std::uint8_t> Sender::makeStreamConfig(const StreamConfig& config) {
    return makeRecord(RecordType::StreamConfig, encodeStreamConfig(config));
}

std::vector<std::uint8_t> Sender::makeFrameMarker(const FrameMarker& marker) {
    return makeRecord(RecordType::FrameMarker, encodeFrameMarker(marker));
}

std::vector<std::uint8_t> Sender::makePoints(const std::vector<PointSample>& points) {
    return makeRecord(RecordType::Points, encodePointSamples(points, userChannelCountValue));
}

std::vector<std::uint8_t> Sender::makeStatus(const Status& status) {
    return makeRecord(RecordType::Status, encodeStatus(status));
}

std::vector<std::uint8_t> Sender::makePing(std::uint64_t timestampNs) {
    std::vector<std::uint8_t> payload;
    appendUInt64(payload, timestampNs);
    return makeRecord(RecordType::Ping, std::move(payload));
}

std::vector<std::uint8_t> Sender::makePong(std::uint64_t timestampNs) {
    std::vector<std::uint8_t> payload;
    appendUInt64(payload, timestampNs);
    return makeRecord(RecordType::Pong, std::move(payload));
}

std::vector<std::uint8_t> Sender::makeClose() {
    return makeRecord(RecordType::Close);
}

std::vector<std::uint8_t> Sender::makeRecord(RecordType type,
                                             std::vector<std::uint8_t> payload,
                                             std::uint16_t flags) {
    Record record;
    record.type = type;
    record.flags = flags;
    record.sequence = nextSequence++;
    record.payload = std::move(payload);
    return encodeRecord(record);
}

} // namespace libera::protocol
