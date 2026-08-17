#pragma once

#include "libera/protocol/Protocol.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace libera::protocol {

struct ReceiverCallbacks {
    std::function<void(const Record&)> onRecord;
    std::function<void(const Hello&)> onHello;
    std::function<void(const Accept&)> onAccept;
    std::function<void(const StreamConfig&)> onStreamConfig;
    std::function<void(const ScannerSync&)> onScannerSync;
    std::function<void(const FrameMarker&)> onFrameMarker;
    std::function<void(const std::vector<PointSample>&)> onPoints;
    std::function<void(const Status&)> onStatus;
    std::function<void(const std::string&)> onError;
};

class Receiver {
public:
    explicit Receiver(std::uint8_t userChannelCount = 0);

    void setUserChannelCount(std::uint8_t count);
    std::uint8_t getUserChannelCount() const;

    void setCallbacks(ReceiverCallbacks callbacks);
    void feed(const std::uint8_t* data, std::size_t size);
    void feed(const std::vector<std::uint8_t>& data);
    void reset();

private:
    void dispatchRecord(const Record& record);
    void reportError(const std::string& error);

    std::uint8_t userChannelCountValue = 0;
    ReceiverCallbacks callbacksValue;
    std::vector<std::uint8_t> buffer;
};

} // namespace libera::protocol
