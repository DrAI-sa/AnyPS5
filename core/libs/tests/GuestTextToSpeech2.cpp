#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

extern "C" {
std::int32_t APS5_VABI sceTextToSpeech2Initialize(const void* param);
std::int32_t APS5_VABI sceTextToSpeech2GetSystemStatus();
std::int32_t APS5_VABI sceTextToSpeech2Open();
std::int32_t APS5_VABI sceTextToSpeech2Speak();
std::int32_t APS5_VABI sceTextToSpeech2GetSpeechStatus();
std::int32_t APS5_VABI sceTextToSpeech2Cancel();
std::int32_t APS5_VABI sceTextToSpeech2Close();
std::int32_t APS5_VABI sceTextToSpeech2Terminate();
}

namespace {

constexpr std::int32_t EOPNOTSUPP_ERROR = static_cast<std::int32_t>(0x8002002D);

void ExpectUnsupported(std::int32_t result, const char* name) {
    if (result != EOPNOTSUPP_ERROR) {
        std::printf("TextToSpeech2: %s returned 0x%08X instead of the unsupported operation\n", name,
                    static_cast<std::uint32_t>(result));
        std::abort();
    }
}

}

int main() {
    const std::uint32_t param[12]{0x2000000, 0x26c};
    ExpectUnsupported(sceTextToSpeech2GetSystemStatus(), "sceTextToSpeech2GetSystemStatus");
    ExpectUnsupported(sceTextToSpeech2Initialize(param), "sceTextToSpeech2Initialize");
    ExpectUnsupported(sceTextToSpeech2Open(), "sceTextToSpeech2Open");
    ExpectUnsupported(sceTextToSpeech2Speak(), "sceTextToSpeech2Speak");
    ExpectUnsupported(sceTextToSpeech2GetSpeechStatus(), "sceTextToSpeech2GetSpeechStatus");
    ExpectUnsupported(sceTextToSpeech2Cancel(), "sceTextToSpeech2Cancel");
    ExpectUnsupported(sceTextToSpeech2Close(), "sceTextToSpeech2Close");
    ExpectUnsupported(sceTextToSpeech2Terminate(), "sceTextToSpeech2Terminate");
    std::puts("TextToSpeech2 tests passed");
    return 0;
}
