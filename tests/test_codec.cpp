#include "libera/protocol/Codec.hpp"
#include "libera/protocol/Receiver.hpp"
#include "libera/protocol/Sender.hpp"

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

using namespace libera::protocol;

void testRecordRoundTrip() {
    Record record;
    record.type = RecordType::Points;
    record.flags = 7;
    record.sequence = 42;
    record.payload = {1, 2, 3, 4};

    const auto bytes = encodeRecord(record);
    const auto decoded = decodeRecord(bytes.data(), bytes.size());

    assert(decoded.status == DecodeStatus::Complete);
    assert(decoded.bytesConsumed == bytes.size());
    assert(decoded.record.type == record.type);
    assert(decoded.record.flags == record.flags);
    assert(decoded.record.sequence == record.sequence);
    assert(decoded.record.payload == record.payload);

    const auto partial = decodeRecord(bytes.data(), bytes.size() - 1);
    assert(partial.status == DecodeStatus::Incomplete);
}

void testPointRoundTrip() {
    PointSample point;
    point.x = -32768;
    point.y = 32767;
    point.r = 100;
    point.g = 200;
    point.b = 300;
    point.i = 400;
    point.user = {500, 600};

    const std::vector<PointSample> source{point};
    const auto payload = encodePointSamples(source, 2);

    std::vector<PointSample> decoded;
    std::string error;
    assert(decodePointSamples(payload.data(), payload.size(), 2, decoded, error));
    assert(decoded.size() == 1);
    assert(decoded[0].x == point.x);
    assert(decoded[0].y == point.y);
    assert(decoded[0].r == point.r);
    assert(decoded[0].g == point.g);
    assert(decoded[0].b == point.b);
    assert(decoded[0].i == point.i);
    assert(decoded[0].user == point.user);
}

void testSenderReceiver() {
    Sender sender(2);
    Receiver receiver(2);

    bool sawHello = false;
    bool sawConfig = false;
    bool sawMarker = false;
    bool sawPoints = false;
    std::string error;

    ReceiverCallbacks callbacks;
    callbacks.onError = [&](const std::string& value) { error = value; };
    callbacks.onHello = [&](const Hello& hello) {
        sawHello = true;
        assert(hello.senderName == "test-sender");
        assert(hello.requestedStreamMode == StreamMode::FrameByCount);
    };
    callbacks.onStreamConfig = [&](const StreamConfig& config) {
        sawConfig = true;
        assert(config.defaultPointRate == 30000);
        assert(config.userChannelCount == 2);
    };
    callbacks.onFrameMarker = [&](const FrameMarker& marker) {
        sawMarker = true;
        assert(marker.frameId == 9);
        assert(marker.framePointCount == 2);
    };
    callbacks.onPoints = [&](const std::vector<PointSample>& points) {
        sawPoints = true;
        assert(points.size() == 2);
        assert(points[1].x == 1234);
        assert(points[1].user.size() == 2);
        assert(points[1].user[1] == 456);
    };
    receiver.setCallbacks(callbacks);

    Hello hello;
    hello.senderName = "test-sender";
    hello.requestedStreamMode = StreamMode::FrameByCount;
    hello.requestedUserChannelCount = 2;
    hello.defaultPointRate = 30000;

    StreamConfig config;
    config.defaultPointRate = 30000;
    config.streamMode = StreamMode::FrameByCount;
    config.userChannelCount = 2;

    FrameMarker marker;
    marker.frameId = 9;
    marker.framePointCount = 2;
    marker.pointRate = 30000;

    PointSample a;
    a.x = -1234;
    a.user = {1, 2};
    PointSample b;
    b.x = 1234;
    b.user = {123, 456};

    std::vector<std::uint8_t> bytes;
    const auto helloBytes = sender.makeHello(hello);
    const auto configBytes = sender.makeStreamConfig(config);
    const auto markerBytes = sender.makeFrameMarker(marker);
    const auto pointBytes = sender.makePoints({a, b});
    bytes.insert(bytes.end(), helloBytes.begin(), helloBytes.end());
    bytes.insert(bytes.end(), configBytes.begin(), configBytes.end());
    bytes.insert(bytes.end(), markerBytes.begin(), markerBytes.end());
    bytes.insert(bytes.end(), pointBytes.begin(), pointBytes.end());

    receiver.feed(bytes.data(), 5);
    receiver.feed(bytes.data() + 5, bytes.size() - 5);

    assert(error.empty());
    assert(sawHello);
    assert(sawConfig);
    assert(sawMarker);
    assert(sawPoints);
}

int main() {
    testRecordRoundTrip();
    testPointRoundTrip();
    testSenderReceiver();
    return 0;
}

