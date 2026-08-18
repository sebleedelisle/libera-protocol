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

    RecordHeader header;
    std::string error;
    assert(decodeRecordHeader(bytes.data(), RECORD_HEADER_SIZE, header, error));
    assert(header.type == record.type);
    assert(header.flags == record.flags);
    assert(header.sequence == record.sequence);
    assert(header.payloadSize == record.payload.size());
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

void testScannerSyncRoundTrip() {
    ScannerSync scannerSync;
    scannerSync.offsetNs = 237500;
    scannerSync.enabled = true;

    const auto payload = encodeScannerSync(scannerSync);
    ScannerSync decoded;
    std::string error;
    assert(decodeScannerSync(payload.data(), payload.size(), decoded, error));
    assert(decoded.offsetNs == scannerSync.offsetNs);
    assert(decoded.enabled == scannerSync.enabled);

    scannerSync.offsetNs = -50000;
    scannerSync.enabled = false;
    const auto negativePayload = encodeScannerSync(scannerSync);
    assert(decodeScannerSync(negativePayload.data(), negativePayload.size(), decoded, error));
    assert(decoded.offsetNs == scannerSync.offsetNs);
    assert(!decoded.enabled);
}

void testSenderReceiver() {
    Sender sender(2);
    Receiver receiver(2);

    bool sawHello = false;
    bool sawAccept = false;
    bool sawReject = false;
    bool sawConfig = false;
    bool sawScannerSync = false;
    bool sawMarker = false;
    bool sawPoints = false;
    bool sawStatus = false;
    std::string error;

    ReceiverCallbacks callbacks;
    callbacks.onError = [&](const std::string& value) { error = value; };
    callbacks.onHello = [&](const Hello& hello) {
        sawHello = true;
        assert(hello.senderName == "test-sender");
        assert(hello.requestedStreamMode == StreamMode::FrameByCount);
    };
    callbacks.onAccept = [&](const Accept& accept) {
        sawAccept = true;
        assert(accept.acceptedStreamMode == StreamMode::FrameByCount);
        assert(accept.acceptedUserChannelCount == 2);
        assert(accept.sessionId == 123);
    };
    callbacks.onReject = [&](const Reject& reject) {
        sawReject = true;
        assert(reject.code == RejectCode::Busy);
        assert(reject.message == "busy");
    };
    callbacks.onStreamConfig = [&](const StreamConfig& config) {
        sawConfig = true;
        assert(config.defaultPointRate == 30000);
        assert(config.userChannelCount == 2);
    };
    callbacks.onScannerSync = [&](const ScannerSync& scannerSync) {
        sawScannerSync = true;
        assert(scannerSync.offsetNs == 175000);
        assert(scannerSync.enabled);
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
    callbacks.onStatus = [&](const Status& status) {
        sawStatus = true;
        assert(status.code == 2);
        assert(status.queuedFrames == 4);
        assert(status.message == "warming");
    };
    receiver.setCallbacks(callbacks);

    Hello hello;
    hello.senderName = "test-sender";
    hello.requestedStreamMode = StreamMode::FrameByCount;
    hello.requestedUserChannelCount = 2;
    hello.defaultPointRate = 30000;

    Accept accept;
    accept.acceptedStreamMode = StreamMode::FrameByCount;
    accept.acceptedUserChannelCount = 2;
    accept.defaultPointRate = 30000;
    accept.maxPointRate = 60000;
    accept.maxFramePointCount = 4096;
    accept.maxRecordPayloadSize = 65536;
    accept.sessionId = 123;
    accept.featureFlags = FeatureTargetBeginTime | FeatureStatus;

    Reject reject;
    reject.code = RejectCode::Busy;
    reject.message = "busy";

    StreamConfig config;
    config.defaultPointRate = 30000;
    config.streamMode = StreamMode::FrameByCount;
    config.userChannelCount = 2;

    ScannerSync scannerSync;
    scannerSync.offsetNs = 175000;
    scannerSync.enabled = true;

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

    Status status;
    status.code = 2;
    status.queuedFrames = 4;
    status.message = "warming";

    std::vector<std::uint8_t> bytes;
    const auto helloBytes = sender.makeHello(hello);
    const auto acceptBytes = sender.makeAccept(accept);
    const auto rejectBytes = sender.makeReject(reject);
    const auto configBytes = sender.makeStreamConfig(config);
    const auto scannerSyncBytes = sender.makeScannerSync(scannerSync);
    const auto markerBytes = sender.makeFrameMarker(marker);
    const auto pointBytes = sender.makePoints({a, b});
    const auto statusBytes = sender.makeStatus(status);
    bytes.insert(bytes.end(), helloBytes.begin(), helloBytes.end());
    bytes.insert(bytes.end(), acceptBytes.begin(), acceptBytes.end());
    bytes.insert(bytes.end(), rejectBytes.begin(), rejectBytes.end());
    bytes.insert(bytes.end(), configBytes.begin(), configBytes.end());
    bytes.insert(bytes.end(), scannerSyncBytes.begin(), scannerSyncBytes.end());
    bytes.insert(bytes.end(), markerBytes.begin(), markerBytes.end());
    bytes.insert(bytes.end(), pointBytes.begin(), pointBytes.end());
    bytes.insert(bytes.end(), statusBytes.begin(), statusBytes.end());

    receiver.feed(bytes.data(), 5);
    receiver.feed(bytes.data() + 5, bytes.size() - 5);

    assert(error.empty());
    assert(sawHello);
    assert(sawAccept);
    assert(sawReject);
    assert(sawConfig);
    assert(sawScannerSync);
    assert(sawMarker);
    assert(sawPoints);
    assert(sawStatus);
}

void testDiscoveryAdvertisement() {
    DiscoveryAdvertisement advertisement;
    advertisement.endpointId = "endpoint-1";
    advertisement.displayName = "Libera Link Target";
    advertisement.endpointType = "virtual";
    advertisement.address = "192.168.1.20";
    advertisement.tcpPort = 45430;
    advertisement.supportedStreamModes = streamModeMask(StreamMode::RawPointStream) |
                                         streamModeMask(StreamMode::FrameByCount);
    advertisement.availability = EndpointAvailability::Busy;
    advertisement.maxUserChannelCount = 2;
    advertisement.minPointRate = 1000;
    advertisement.maxPointRate = 60000;
    advertisement.maxFramePointCount = 8192;
    advertisement.featureFlags = FeatureTargetBeginTime | FeatureScannerSync;

    const auto payload = encodeDiscoveryAdvertisement(advertisement);
    DiscoveryAdvertisement decoded;
    std::string error;
    assert(decodeDiscoveryAdvertisement(payload.data(), payload.size(), decoded, error));
    assert(decoded.endpointId == advertisement.endpointId);
    assert(decoded.displayName == advertisement.displayName);
    assert(decoded.endpointType == advertisement.endpointType);
    assert(decoded.address == advertisement.address);
    assert(decoded.tcpPort == advertisement.tcpPort);
    assert(decoded.supportedStreamModes == advertisement.supportedStreamModes);
    assert(decoded.availability == advertisement.availability);
    assert(decoded.maxUserChannelCount == advertisement.maxUserChannelCount);
    assert(decoded.minPointRate == advertisement.minPointRate);
    assert(decoded.maxPointRate == advertisement.maxPointRate);
    assert(decoded.maxFramePointCount == advertisement.maxFramePointCount);
    assert(decoded.featureFlags == advertisement.featureFlags);
}

int main() {
    testRecordRoundTrip();
    testPointRoundTrip();
    testScannerSyncRoundTrip();
    testSenderReceiver();
    testDiscoveryAdvertisement();
    return 0;
}
