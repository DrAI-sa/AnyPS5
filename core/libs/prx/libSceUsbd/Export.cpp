#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_set>

namespace {

constexpr std::int32_t SCE_USBD_ERROR_INVALID_ARG = static_cast<std::int32_t>(0x80240002);
constexpr std::int32_t SCE_USBD_ERROR_NOT_FOUND = static_cast<std::int32_t>(0x80240005);
constexpr std::uint8_t TransferFreeBuffer = 0x02;
constexpr std::uint8_t TransferTypeInterrupt = 3;

struct UsbdIsoPacketDescriptor {
    std::uint32_t length;
    std::uint32_t actualLength;
    std::int32_t status;
};

struct UsbdTransfer;
using UsbdTransferCallback = void (APS5_VABI*)(UsbdTransfer*);

struct UsbdTransfer {
    void* deviceHandle;
    std::uint8_t flags;
    std::uint8_t endpoint;
    std::uint8_t type;
    std::uint32_t timeout;
    std::int32_t status;
    std::int32_t length;
    std::int32_t actualLength;
    UsbdTransferCallback callback;
    void* userData;
    std::uint8_t* buffer;
    std::int32_t numIsoPackets;
};

static_assert(offsetof(UsbdTransfer, timeout) == 12);
static_assert(offsetof(UsbdTransfer, callback) == 32);
static_assert(offsetof(UsbdTransfer, buffer) == 48);
static_assert(offsetof(UsbdTransfer, numIsoPackets) == 56);

constexpr std::size_t IsoPacketsOffset = offsetof(UsbdTransfer, numIsoPackets) + sizeof(std::int32_t);

std::size_t TransferBytes(std::int32_t isoPackets) {
    return std::max(sizeof(UsbdTransfer), IsoPacketsOffset + static_cast<std::size_t>(isoPackets) * sizeof(UsbdIsoPacketDescriptor));
}

std::mutex g_transferMutex;
std::unordered_set<UsbdTransfer*> g_transfers;

void RequireTransfer(UsbdTransfer* transfer, const char* function) {
    if (transfer == nullptr) throw std::invalid_argument(std::string(function) + ": null transfer");
    std::lock_guard lock(g_transferMutex);
    if (!g_transfers.contains(transfer)) throw std::runtime_error(std::string(function) + ": transfer was not returned by sceUsbdAllocTransfer");
}

struct UsbdTimeval {
    std::int64_t seconds;
    std::int64_t microseconds;
};

void* g_emptyDeviceList[1] = {nullptr};

}

extern "C" {

void APS5_VABI free_nid_postfix(void* pointer);

std::int32_t APS5_VABI sceUsbdInit() {
    return 0;
}

void APS5_VABI sceUsbdExit() {
}

std::int64_t APS5_VABI sceUsbdGetDeviceList(void*** list) {
    if (list == nullptr) return SCE_USBD_ERROR_INVALID_ARG;
    *list = g_emptyDeviceList;
    return 0;
}

void APS5_VABI sceUsbdFreeDeviceList(void** list, std::int32_t unrefDevices) {
    (void)unrefDevices;
    if (list != nullptr && list != g_emptyDeviceList) throw std::runtime_error(std::string(__func__) + ": list was not returned by sceUsbdGetDeviceList");
}

std::int32_t APS5_VABI sceUsbdHandleEventsTimeout(const UsbdTimeval* timeout) {
    if (timeout == nullptr || timeout->seconds < 0 || timeout->microseconds < 0 || timeout->microseconds >= 1000000) return SCE_USBD_ERROR_INVALID_ARG;
    std::this_thread::sleep_for(std::chrono::seconds(timeout->seconds) + std::chrono::microseconds(timeout->microseconds));
    return 0;
}

UsbdTransfer* APS5_VABI sceUsbdAllocTransfer(std::int32_t isoPackets) {
    if (isoPackets < 0) return nullptr;
    const auto bytes = TransferBytes(isoPackets);
    auto* transfer = static_cast<UsbdTransfer*>(std::calloc(1, bytes));
    if (transfer == nullptr) return nullptr;
    transfer->numIsoPackets = isoPackets;
    std::lock_guard lock(g_transferMutex);
    g_transfers.insert(transfer);
    return transfer;
}

int APS5_VABI sceUsbdAttachKernelDriver() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::int32_t APS5_VABI sceUsbdCancelTransfer(UsbdTransfer* transfer) {
    RequireTransfer(transfer, __func__);
    return SCE_USBD_ERROR_NOT_FOUND;
}

int APS5_VABI sceUsbdCheckConnected() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUsbdClaimInterface() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI sceUsbdClose(void* deviceHandle) {
    if (deviceHandle != nullptr) throw std::runtime_error(std::string(__func__) + ": no device can be opened, so the handle was not returned by sceUsbdOpen");
}

int APS5_VABI sceUsbdControlTransfer() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::int32_t APS5_VABI sceUsbdEventHandlingOk() {
    return 1;
}

void APS5_VABI sceUsbdFillInterruptTransfer(UsbdTransfer* transfer, void* deviceHandle, std::uint8_t endpoint, std::uint8_t* buffer, std::int32_t length, UsbdTransferCallback callback, void* userData, std::uint32_t timeout) {
    if (transfer == nullptr) throw std::invalid_argument(std::string(__func__) + ": null transfer");
    transfer->deviceHandle = deviceHandle;
    transfer->endpoint = endpoint;
    transfer->type = TransferTypeInterrupt;
    transfer->timeout = timeout;
    transfer->buffer = buffer;
    transfer->length = length;
    transfer->userData = userData;
    transfer->callback = callback;
}

void APS5_VABI sceUsbdFreeConfigDescriptor(void* config) {
    if (config != nullptr) throw std::runtime_error(std::string(__func__) + ": no device has a configuration descriptor, so the descriptor was not returned by this library");
}

void APS5_VABI sceUsbdFreeTransfer(UsbdTransfer* transfer) {
    if (transfer == nullptr) return;
    {
        std::lock_guard lock(g_transferMutex);
        if (g_transfers.erase(transfer) == 0) throw std::runtime_error(std::string(__func__) + ": transfer was not returned by sceUsbdAllocTransfer");
    }
    if ((transfer->flags & TransferFreeBuffer) != 0 && transfer->buffer != nullptr) free_nid_postfix(transfer->buffer);
    std::free(transfer);
}

int APS5_VABI sceUsbdGetActiveConfigDescriptor() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUsbdGetBusNumber() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUsbdGetConfigDescriptor() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUsbdGetDeviceAddress() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUsbdGetDeviceDescriptor() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUsbdGetStringDescriptor() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUsbdKernelDriverActive() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUsbdOpen() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUsbdRefDevice() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUsbdReleaseInterface() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUsbdResetDevice() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUsbdSetConfiguration() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUsbdSubmitTransfer() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI sceUsbdUnrefDevice(void* device) {
    if (device != nullptr) throw std::runtime_error(std::string(__func__) + ": the device list is empty, so the device was not returned by this library");
}

}
