#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <atomic>
#include <stdexcept>
#include <string>

namespace {
std::atomic<bool> g_initialized{false};

constexpr int ErrorNotInitialized = static_cast<int>(0x809D1001u);
constexpr int ErrorLimitTooBig = static_cast<int>(0x809D100Au);
constexpr std::uint32_t MaxSearchLimit = 92;
constexpr std::int64_t LibraryUpdateId = 0;

void RequireList(const char* funcName, const void* list, std::uint32_t length) {
    if (list == nullptr && length != 0) throw std::invalid_argument(std::string(funcName) + ": invalid argument");
}

[[noreturn]] void ThrowNoContent(const char* funcName, const std::string& what) {
    throw std::runtime_error(std::string(funcName) + ": " + what + " is not in the content library, which is empty; the console's error for it is unknown");
}

[[noreturn]] void ThrowUnknownMetadata(const char* funcName, std::int32_t metadataId) {
    throw std::invalid_argument(std::string(funcName) + ": unknown metadata id " + std::to_string(metadataId));
}
}

extern "C" {

int APS5_VABI sceContentSearchInit(const ContentSearchInitParam* init_param) {
    if (init_param == nullptr) APS5_INVALID_ARG_EX;
    bool expected = false;
    if (!g_initialized.compare_exchange_strong(expected, true)) throw std::logic_error(std::string(__func__) + ": already initialized");
    return 0;
}

int APS5_VABI sceContentSearchCloseMetadata(std::int32_t metadataId) {
    if (!g_initialized.load()) return ErrorNotInitialized;
    ThrowUnknownMetadata(__func__, metadataId);
}

int APS5_VABI sceContentSearchGetMetadataFieldInfo(std::int32_t metadataId, const char* field, std::int32_t* metadataType, std::int32_t* size) {
    if (field == nullptr || metadataType == nullptr || size == nullptr) APS5_INVALID_ARG_EX;
    if (!g_initialized.load()) return ErrorNotInitialized;
    ThrowUnknownMetadata(__func__, metadataId);
}

int APS5_VABI sceContentSearchGetMetadataValue(std::int32_t metadataId, const char* field, void* value) {
    if (field == nullptr || value == nullptr) APS5_INVALID_ARG_EX;
    if (!g_initialized.load()) return ErrorNotInitialized;
    ThrowUnknownMetadata(__func__, metadataId);
}

int APS5_VABI sceContentSearchOpenMetadata(const char* filePath, std::int32_t* metadataId) {
    if (filePath == nullptr || metadataId == nullptr) APS5_INVALID_ARG_EX;
    if (!g_initialized.load()) return ErrorNotInitialized;
    ThrowNoContent(__func__, std::string("file ") + filePath);
}

int APS5_VABI sceContentSearchSearchContent(const void* columnSet, std::uint32_t columnSetLength, const void* orderByConditions, std::uint32_t orderByConditionsLength,
                                            std::uint32_t offset, std::uint32_t limit, std::int64_t* numOfContent, void* infos, std::int64_t* lastUpdateId) {
    (void)offset;
    RequireList(__func__, columnSet, columnSetLength);
    RequireList(__func__, orderByConditions, orderByConditionsLength);
    RequireList(__func__, infos, limit);
    if (numOfContent == nullptr || lastUpdateId == nullptr) APS5_INVALID_ARG_EX;
    if (!g_initialized.load()) return ErrorNotInitialized;
    if (limit > MaxSearchLimit) return ErrorLimitTooBig;
    *numOfContent = 0;
    *lastUpdateId = LibraryUpdateId;
    return 0;
}

int APS5_VABI sceContentSearchGetContentLastUpdateId(std::int64_t* lastUpdateId) {
    if (lastUpdateId == nullptr) APS5_INVALID_ARG_EX;
    if (!g_initialized.load()) return ErrorNotInitialized;
    *lastUpdateId = LibraryUpdateId;
    return 0;
}

int APS5_VABI sceContentSearchOpenMetadataByContentId(std::int64_t contentId, std::int32_t* metadataId) {
    if (metadataId == nullptr) APS5_INVALID_ARG_EX;
    if (!g_initialized.load()) return ErrorNotInitialized;
    ThrowNoContent(__func__, "content id " + std::to_string(contentId));
}

int APS5_VABI sceContentSearchGetTotalContentSize(const void* columnSet, std::uint32_t columnSetLength, std::int64_t* size, std::int64_t* lastUpdateId) {
    RequireList(__func__, columnSet, columnSetLength);
    if (size == nullptr || lastUpdateId == nullptr) APS5_INVALID_ARG_EX;
    if (!g_initialized.load()) return ErrorNotInitialized;
    *size = 0;
    *lastUpdateId = LibraryUpdateId;
    return 0;
}

int APS5_VABI sceContentSearchGetNumOfContent(const void* columnSet, std::uint32_t columnSetLength, std::int64_t* numOfContent, std::int64_t* lastUpdateId) {
    RequireList(__func__, columnSet, columnSetLength);
    if (numOfContent == nullptr || lastUpdateId == nullptr) APS5_INVALID_ARG_EX;
    if (!g_initialized.load()) return ErrorNotInitialized;
    *numOfContent = 0;
    *lastUpdateId = LibraryUpdateId;
    return 0;
}

int APS5_VABI sceContentSearchTerm(void) {
    bool expected = true;
    if (!g_initialized.compare_exchange_strong(expected, false)) throw std::logic_error(std::string(__func__) + ": not initialized");
    return 0;
}

}
