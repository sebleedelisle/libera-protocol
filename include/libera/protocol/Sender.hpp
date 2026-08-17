#pragma once

#include "libera/protocol/Protocol.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace libera::protocol {

class Sender {
public:
    explicit Sender(std::uint8_t userChannelCount = 0);

    void setUserChannelCount(std::uint8_t count);
    std::uint8_t getUserChannelCount() const;

    std::vector<std::uint8_t> makeHello(const Hello& hello);
    std::vector<std::uint8_t> makeAccept(const Accept& accept);
    std::vector<std::uint8_t> makeReady();
    std::vector<std::uint8_t> makeStreamConfig(const StreamConfig& config);
    std::vector<std::uint8_t> makeFrameMarker(const FrameMarker& marker);
    std::vector<std::uint8_t> makePoints(const std::vector<PointSample>& points);
    std::vector<std::uint8_t> makeStatus(const Status& status);
    std::vector<std::uint8_t> makePing(std::uint64_t timestampNs);
    std::vector<std::uint8_t> makePong(std::uint64_t timestampNs);
    std::vector<std::uint8_t> makeClose();

private:
    std::vector<std::uint8_t> makeRecord(RecordType type,
                                         std::vector<std::uint8_t> payload = {},
                                         std::uint16_t flags = 0);

    std::uint8_t userChannelCountValue = 0;
    std::uint32_t nextSequence = 1;
};

} // namespace libera::protocol
