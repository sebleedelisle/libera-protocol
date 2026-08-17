#include "libera/protocol/Receiver.hpp"

#include "libera/protocol/Codec.hpp"

#include <cstddef>
#include <utility>

namespace libera::protocol {

Receiver::Receiver(std::uint8_t userChannelCount)
    : userChannelCountValue(userChannelCount) {
}

void Receiver::setUserChannelCount(std::uint8_t count) {
    userChannelCountValue = count;
}

std::uint8_t Receiver::getUserChannelCount() const {
    return userChannelCountValue;
}

void Receiver::setCallbacks(ReceiverCallbacks callbacks) {
    callbacksValue = std::move(callbacks);
}

void Receiver::feed(const std::uint8_t* data, std::size_t size) {
    if (data == nullptr && size != 0) {
        reportError("null input buffer");
        return;
    }

    if (size == 0) {
        return;
    }

    buffer.insert(buffer.end(), data, data + size);
    while (!buffer.empty()) {
        DecodeResult result = decodeRecord(buffer.data(), buffer.size());
        if (result.status == DecodeStatus::Incomplete) {
            return;
        }
        if (result.status == DecodeStatus::Invalid) {
            reportError(result.error);
            buffer.clear();
            return;
        }

        dispatchRecord(result.record);
        buffer.erase(buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(result.bytesConsumed));
    }
}

void Receiver::feed(const std::vector<std::uint8_t>& data) {
    feed(data.data(), data.size());
}

void Receiver::reset() {
    buffer.clear();
}

void Receiver::dispatchRecord(const Record& record) {
    if (callbacksValue.onRecord) {
        callbacksValue.onRecord(record);
    }

    std::string error;
    switch (record.type) {
    case RecordType::Hello: {
        Hello hello;
        if (!decodeHello(record.payload.data(), record.payload.size(), hello, error)) {
            reportError(error);
            return;
        }
        if (callbacksValue.onHello) {
            callbacksValue.onHello(hello);
        }
        break;
    }
    case RecordType::Accept: {
        Accept accept;
        if (!decodeAccept(record.payload.data(), record.payload.size(), accept, error)) {
            reportError(error);
            return;
        }
        userChannelCountValue = accept.acceptedUserChannelCount;
        if (callbacksValue.onAccept) {
            callbacksValue.onAccept(accept);
        }
        break;
    }
    case RecordType::StreamConfig: {
        StreamConfig config;
        if (!decodeStreamConfig(record.payload.data(), record.payload.size(), config, error)) {
            reportError(error);
            return;
        }
        userChannelCountValue = config.userChannelCount;
        if (callbacksValue.onStreamConfig) {
            callbacksValue.onStreamConfig(config);
        }
        break;
    }
    case RecordType::SetScannerSync: {
        ScannerSync scannerSync;
        if (!decodeScannerSync(record.payload.data(),
                               record.payload.size(),
                               scannerSync,
                               error)) {
            reportError(error);
            return;
        }
        if (callbacksValue.onScannerSync) {
            callbacksValue.onScannerSync(scannerSync);
        }
        break;
    }
    case RecordType::FrameMarker: {
        FrameMarker marker;
        if (!decodeFrameMarker(record.payload.data(), record.payload.size(), marker, error)) {
            reportError(error);
            return;
        }
        if (callbacksValue.onFrameMarker) {
            callbacksValue.onFrameMarker(marker);
        }
        break;
    }
    case RecordType::Status: {
        Status status;
        if (!decodeStatus(record.payload.data(), record.payload.size(), status, error)) {
            reportError(error);
            return;
        }
        if (callbacksValue.onStatus) {
            callbacksValue.onStatus(status);
        }
        break;
    }
    case RecordType::Points: {
        std::vector<PointSample> points;
        if (!decodePointSamples(record.payload.data(),
                               record.payload.size(),
                               userChannelCountValue,
                               points,
                               error)) {
            reportError(error);
            return;
        }
        if (callbacksValue.onPoints) {
            callbacksValue.onPoints(points);
        }
        break;
    }
    default:
        break;
    }
}

void Receiver::reportError(const std::string& error) {
    if (callbacksValue.onError) {
        callbacksValue.onError(error);
    }
}

} // namespace libera::protocol
