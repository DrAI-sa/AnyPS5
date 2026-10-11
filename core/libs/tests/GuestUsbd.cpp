#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace {

struct UsbdTimeval {
    std::int64_t seconds;
    std::int64_t microseconds;
};

}

extern "C" {
std::int32_t APS5_VABI sceUsbdInit();
void APS5_VABI sceUsbdExit();
std::int64_t APS5_VABI sceUsbdGetDeviceList(void*** list);
void APS5_VABI sceUsbdFreeDeviceList(void** list, std::int32_t unrefDevices);
std::int32_t APS5_VABI sceUsbdHandleEventsTimeout(const UsbdTimeval* timeout);
int APS5_VABI sceUsbdOpen();
void* APS5_VABI sceUsbdAllocTransfer(std::int32_t isoPackets);
void APS5_VABI sceUsbdFreeTransfer(void* transfer);
std::int32_t APS5_VABI sceUsbdCancelTransfer(void* transfer);
void APS5_VABI sceUsbdFillInterruptTransfer(void* transfer, void* deviceHandle, std::uint8_t endpoint, std::uint8_t* buffer, std::int32_t length, void (APS5_VABI* callback)(void*), void* userData, std::uint32_t timeout);
std::int32_t APS5_VABI sceUsbdEventHandlingOk();
void APS5_VABI sceUsbdClose(void* deviceHandle);
void APS5_VABI sceUsbdUnrefDevice(void* device);
void APS5_VABI sceUsbdFreeConfigDescriptor(void* config);
void* APS5_VABI malloc_nid_postfix(std::size_t size);
}

namespace {

constexpr std::int32_t invalidArgument = static_cast<std::int32_t>(0x80240002);
constexpr std::int32_t notFound = static_cast<std::int32_t>(0x80240005);

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "USBD: %s\n", message);
        std::abort();
    }
}

struct GuestTransfer {
    void* deviceHandle;
    std::uint8_t flags;
    std::uint8_t endpoint;
    std::uint8_t type;
    std::uint32_t timeout;
    std::int32_t status;
    std::int32_t length;
    std::int32_t actualLength;
    void (APS5_VABI* callback)(void*);
    void* userData;
    std::uint8_t* buffer;
    std::int32_t numIsoPackets;
    std::uint32_t isoPackets[3 * 4];
};

void APS5_VABI TransferDone(void*) {}

template<class TFunction>
bool Throws(TFunction function) {
    try {
        function();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

void TestTransfers() {
    Require(sceUsbdAllocTransfer(-1) == nullptr, "negative iso packet count accepted");
    auto* transfer = static_cast<GuestTransfer*>(sceUsbdAllocTransfer(4));
    Require(transfer != nullptr, "transfer allocation failed");
    Require(transfer->numIsoPackets == 4, "iso packet count not stored");
    Require(transfer->deviceHandle == nullptr && transfer->flags == 0 && transfer->buffer == nullptr && transfer->callback == nullptr, "transfer not zeroed");
    for (const auto word : transfer->isoPackets) Require(word == 0, "iso packet descriptors not zeroed");
    transfer->isoPackets[11] = 7;

    std::uint8_t data[8] {};
    int context = 0;
    auto* handle = reinterpret_cast<void*>(std::uintptr_t{0x1000});
    sceUsbdFillInterruptTransfer(transfer, handle, 0x81, data, sizeof(data), TransferDone, &context, 250);
    Require(transfer->deviceHandle == handle && transfer->endpoint == 0x81 && transfer->type == 3, "interrupt transfer header not filled");
    Require(transfer->buffer == data && transfer->length == 8 && transfer->timeout == 250, "interrupt transfer buffer not filled");
    Require(transfer->callback == TransferDone && transfer->userData == &context, "interrupt transfer callback not filled");
    Require(transfer->numIsoPackets == 4 && transfer->isoPackets[11] == 7, "interrupt fill touched the iso packets");
    Require(sceUsbdCancelTransfer(transfer) == notFound, "cancelling a transfer that was never submitted did not report not found");
    transfer->buffer = nullptr;
    sceUsbdFreeTransfer(transfer);
    Require(Throws([&] { sceUsbdFreeTransfer(transfer); }), "freeing a transfer twice did not throw");
    Require(Throws([&] { sceUsbdCancelTransfer(transfer); }), "cancelling a freed transfer did not throw");
    Require(Throws([] { sceUsbdCancelTransfer(nullptr); }), "cancelling a null transfer did not throw");
    Require(Throws([] { sceUsbdFillInterruptTransfer(nullptr, nullptr, 1, nullptr, 0, nullptr, nullptr, 0); }), "filling a null transfer did not throw");
    sceUsbdFreeTransfer(nullptr);

    std::array<void*, 10> heap{};
    ApplicationHeapRegister_nid_no_patch(heap.data());
    auto* owning = static_cast<GuestTransfer*>(sceUsbdAllocTransfer(0));
    Require(owning != nullptr && owning->numIsoPackets == 0, "transfer without iso packets not allocated");
    owning->flags = 0x02;
    owning->buffer = static_cast<std::uint8_t*>(malloc_nid_postfix(64));
    Require(owning->buffer != nullptr, "guest buffer allocation failed");
    sceUsbdFreeTransfer(owning);
}

void TestNullDeviceObjects() {
    Require(sceUsbdEventHandlingOk() == 1, "event handling not allowed");
    sceUsbdClose(nullptr);
    sceUsbdUnrefDevice(nullptr);
    sceUsbdFreeConfigDescriptor(nullptr);
    auto* object = reinterpret_cast<void*>(std::uintptr_t{0x1000});
    Require(Throws([&] { sceUsbdClose(object); }), "closing an unknown handle did not throw");
    Require(Throws([&] { sceUsbdUnrefDevice(object); }), "unreferencing an unknown device did not throw");
    Require(Throws([&] { sceUsbdFreeConfigDescriptor(object); }), "freeing an unknown descriptor did not throw");
}

}

int main() {
    Require(sceUsbdInit() == 0, "initialization failed");
    void** list = nullptr;
    Require(sceUsbdGetDeviceList(&list) == 0, "a device was listed");
    Require(list != nullptr && list[0] == nullptr, "device list is not an empty null-terminated array");
    sceUsbdFreeDeviceList(list, 1);
    Require(sceUsbdGetDeviceList(nullptr) == invalidArgument, "null list accepted");
    Require(sceUsbdHandleEventsTimeout(nullptr) == invalidArgument, "null timeout accepted");
    const UsbdTimeval invalid{0, 1000000};
    Require(sceUsbdHandleEventsTimeout(&invalid) == invalidArgument, "out-of-range microseconds accepted");
    const UsbdTimeval timeout{0, 50000};
    const auto start = std::chrono::steady_clock::now();
    Require(sceUsbdHandleEventsTimeout(&timeout) == 0, "event handling failed");
    Require(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(45), "event handling returned before the timeout");
    bool threw = false;
    try {
        sceUsbdOpen();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw, "opening a device did not throw");
    TestTransfers();
    TestNullDeviceObjects();
    sceUsbdExit();
    std::puts("USBD tests passed");
    return 0;
}
