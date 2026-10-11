#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>

extern "C" {
int APS5_VABI sceNetCtlApDialogInitialize(void);
int APS5_VABI sceNetCtlApDialogOpen(const void* param);
int APS5_VABI sceNetCtlApDialogUpdateStatus(void);
int APS5_VABI sceNetCtlApDialogGetResult(void* result);
int APS5_VABI sceNetCtlApDialogClose(void);
int APS5_VABI sceNetCtlApDialogTerminate(void);
}

namespace {

void Require(bool value) { if (!value) std::abort(); }

constexpr int kStatusFinished = 3;
constexpr int kErrNotInitialized = static_cast<int>(0x80B80003);
constexpr int kErrAlreadyInitialized = static_cast<int>(0x80B80004);
constexpr int kErrNotFinished = static_cast<int>(0x80B80005);
constexpr int kErrArgNull = static_cast<int>(0x80B8000D);
constexpr int kResultUserCanceled = 1;

}

int main() {
    std::int64_t param = 0;
    std::int32_t result[2] = {-1, -1};

    Require(sceNetCtlApDialogTerminate() == kErrNotInitialized);
    Require(sceNetCtlApDialogOpen(&param) == kErrNotInitialized);
    Require(sceNetCtlApDialogGetResult(result) == kErrNotInitialized);

    Require(sceNetCtlApDialogInitialize() == 0);
    Require(sceNetCtlApDialogInitialize() == kErrAlreadyInitialized);

    Require(sceNetCtlApDialogOpen(nullptr) == kErrArgNull);
    Require(sceNetCtlApDialogGetResult(result) == kErrNotFinished);

    Require(sceNetCtlApDialogOpen(&param) == 0);
    Require(sceNetCtlApDialogUpdateStatus() == kStatusFinished);

    Require(sceNetCtlApDialogGetResult(nullptr) == kErrArgNull);
    Require(sceNetCtlApDialogGetResult(result) == 0);
    Require(result[0] == kResultUserCanceled);
    Require(result[1] == -1);

    Require(sceNetCtlApDialogClose() == 0);
    Require(sceNetCtlApDialogTerminate() == 0);
    Require(sceNetCtlApDialogTerminate() == kErrNotInitialized);
}
