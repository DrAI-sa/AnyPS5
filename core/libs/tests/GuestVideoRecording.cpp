#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>

extern "C" {
int APS5_VABI sceVideoRecordingClose(int discard);
int APS5_VABI sceVideoRecordingGetInfo(std::int32_t info, void* data, std::size_t size);
int APS5_VABI sceVideoRecordingGetStatus(void);
int APS5_VABI sceVideoRecordingOpen(const char* path, const void* param, void* heap, int heapSize);
int APS5_VABI sceVideoRecordingQueryMemSize(const void* param);
int APS5_VABI sceVideoRecordingSetInfo(std::int32_t info, const void* data, std::size_t size);
int APS5_VABI sceVideoRecordingStart(void);
int APS5_VABI sceVideoRecordingStop(void);
}

namespace {

void Require(bool value) { if (!value) std::abort(); }

}

int main() {
    constexpr int invalidState = static_cast<int>(0x80A80006);
    constexpr int unsupported = static_cast<int>(0x80A80008);
    std::uint8_t param[32]{};
    std::uint8_t heap[64]{};

    Require(sceVideoRecordingGetStatus() == 0);
    Require(sceVideoRecordingQueryMemSize(param) == unsupported);
    Require(sceVideoRecordingQueryMemSize(nullptr) == unsupported);
    Require(sceVideoRecordingOpen("/app0/clip.mp4", param, heap, static_cast<int>(sizeof(heap))) == unsupported);
    Require(sceVideoRecordingOpen(nullptr, nullptr, nullptr, 0) == unsupported);
    Require(sceVideoRecordingGetStatus() == 0);
    Require(sceVideoRecordingStart() == invalidState);
    Require(sceVideoRecordingGetStatus() == 0);
    Require(sceVideoRecordingStop() == invalidState);
    Require(sceVideoRecordingClose(0) == invalidState);
    Require(sceVideoRecordingClose(1) == invalidState);
    Require(sceVideoRecordingGetStatus() == 0);

    const char subtitle[] = "subtitle";
    Require(sceVideoRecordingSetInfo(2, subtitle, sizeof(subtitle)) == 0);
    char read[sizeof(subtitle)]{};
    Require(sceVideoRecordingGetInfo(2, read, sizeof(read)) == 0);
    Require(read[0] == 's' && read[sizeof(read) - 1] == '\0');
    return 0;
}
