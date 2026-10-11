#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace {

struct InitParam {
    std::size_t memorySize;
};

}

extern "C" {
int APS5_VABI sceContentSearchInit(const InitParam* initParam);
int APS5_VABI sceContentSearchTerm();
int APS5_VABI sceContentSearchGetContentLastUpdateId(std::int64_t* lastUpdateId);
int APS5_VABI sceContentSearchGetNumOfContent(const void* columnSet, std::uint32_t columnSetLength, std::int64_t* numOfContent, std::int64_t* lastUpdateId);
int APS5_VABI sceContentSearchGetTotalContentSize(const void* columnSet, std::uint32_t columnSetLength, std::int64_t* size, std::int64_t* lastUpdateId);
int APS5_VABI sceContentSearchSearchContent(const void* columnSet, std::uint32_t columnSetLength, const void* orderByConditions, std::uint32_t orderByConditionsLength,
                                            std::uint32_t offset, std::uint32_t limit, std::int64_t* numOfContent, void* infos, std::int64_t* lastUpdateId);
int APS5_VABI sceContentSearchOpenMetadata(const char* filePath, std::int32_t* metadataId);
int APS5_VABI sceContentSearchOpenMetadataByContentId(std::int64_t contentId, std::int32_t* metadataId);
int APS5_VABI sceContentSearchCloseMetadata(std::int32_t metadataId);
int APS5_VABI sceContentSearchGetMetadataFieldInfo(std::int32_t metadataId, const char* field, std::int32_t* metadataType, std::int32_t* size);
int APS5_VABI sceContentSearchGetMetadataValue(std::int32_t metadataId, const char* field, void* value);
}

namespace {

constexpr int notInitialized = static_cast<int>(0x809D1001u);
constexpr int limitTooBig = static_cast<int>(0x809D100Au);
constexpr std::size_t contentInfoSize = 0x960;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "ContentSearch: %s\n", message);
        std::abort();
    }
}

template <typename TException, typename TCall>
void RequireThrows(TCall call, const char* message) {
    try {
        call();
    } catch (const TException&) {
        return;
    }
    Require(false, message);
}

}

int main() {
    std::int64_t count = -1;
    std::int64_t updateId = -1;
    std::int32_t metadataId = -1;
    std::int32_t metadataType = -1;
    std::int32_t fieldSize = -1;
    unsigned char value[16]{};
    static unsigned char infos[contentInfoSize * 4];

    Require(sceContentSearchGetContentLastUpdateId(&updateId) == notInitialized, "update id before init");
    Require(sceContentSearchGetNumOfContent(nullptr, 0, &count, &updateId) == notInitialized, "count before init");
    Require(sceContentSearchSearchContent(nullptr, 0, nullptr, 0, 0, 4, &count, infos, &updateId) == notInitialized, "search before init");
    Require(sceContentSearchOpenMetadata("/data/a.jpg", &metadataId) == notInitialized, "open before init");
    Require(sceContentSearchCloseMetadata(1) == notInitialized, "close before init");
    Require(count == -1 && updateId == -1 && metadataId == -1, "outputs written before init");

    InitParam param{0x10000};
    Require(sceContentSearchInit(&param) == 0, "init");

    Require(sceContentSearchGetContentLastUpdateId(&updateId) == 0 && updateId == 0, "update id");
    RequireThrows<std::invalid_argument>([] { sceContentSearchGetContentLastUpdateId(nullptr); }, "null update id");

    updateId = -1;
    Require(sceContentSearchGetNumOfContent(nullptr, 0, &count, &updateId) == 0 && count == 0 && updateId == 0, "empty count");
    count = -1;
    unsigned char columnSet[16]{};
    Require(sceContentSearchGetNumOfContent(columnSet, 1, &count, &updateId) == 0 && count == 0, "filtered count");
    RequireThrows<std::invalid_argument>([&] { sceContentSearchGetNumOfContent(nullptr, 1, &count, &updateId); }, "null column set");
    RequireThrows<std::invalid_argument>([&] { sceContentSearchGetNumOfContent(nullptr, 0, nullptr, &updateId); }, "null count");

    std::int64_t totalSize = -1;
    Require(sceContentSearchGetTotalContentSize(columnSet, 1, &totalSize, &updateId) == 0 && totalSize == 0, "total size");
    RequireThrows<std::invalid_argument>([&] { sceContentSearchGetTotalContentSize(nullptr, 0, &totalSize, nullptr); }, "null size update id");

    count = -1;
    updateId = -1;
    infos[0] = 0xAB;
    Require(sceContentSearchSearchContent(columnSet, 1, nullptr, 0, 0, 4, &count, infos, &updateId) == 0, "search");
    Require(count == 0 && updateId == 0 && infos[0] == 0xAB, "search results");
    Require(sceContentSearchSearchContent(nullptr, 0, nullptr, 0, 0, 92, &count, infos, &updateId) == 0, "maximum limit");
    Require(sceContentSearchSearchContent(nullptr, 0, nullptr, 0, 0, 93, &count, infos, &updateId) == limitTooBig, "limit too big");
    RequireThrows<std::invalid_argument>([&] { sceContentSearchSearchContent(nullptr, 0, nullptr, 0, 0, 4, &count, nullptr, &updateId); }, "null infos");
    RequireThrows<std::invalid_argument>([&] { sceContentSearchSearchContent(nullptr, 0, nullptr, 1, 0, 4, &count, infos, &updateId); }, "null order by");

    RequireThrows<std::runtime_error>([&] { sceContentSearchOpenMetadata("/data/a.jpg", &metadataId); }, "open metadata");
    RequireThrows<std::runtime_error>([&] { sceContentSearchOpenMetadataByContentId(1, &metadataId); }, "open metadata by id");
    RequireThrows<std::invalid_argument>([&] { sceContentSearchOpenMetadata(nullptr, &metadataId); }, "null path");
    Require(metadataId == -1, "metadata id written");
    RequireThrows<std::invalid_argument>([] { sceContentSearchCloseMetadata(1); }, "close unknown");
    RequireThrows<std::invalid_argument>([&] { sceContentSearchGetMetadataFieldInfo(1, "title", &metadataType, &fieldSize); }, "field info unknown");
    RequireThrows<std::invalid_argument>([&] { sceContentSearchGetMetadataValue(1, "title", value); }, "value unknown");
    Require(metadataType == -1 && fieldSize == -1, "metadata outputs written");

    Require(sceContentSearchTerm() == 0, "term");
    Require(sceContentSearchGetContentLastUpdateId(&updateId) == notInitialized, "update id after term");
    return 0;
}
