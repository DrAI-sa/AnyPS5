#include "SceTypes.hpp"
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

extern "C" {
int APS5_VABI sceContentExportInit2(const ContentExportInitParam2* init_param);
int APS5_VABI sceContentExportTerm(void);
std::int32_t APS5_VABI sceContentExportStart(void);
std::int32_t APS5_VABI sceContentExportFinish(std::int32_t exportId);
std::int32_t APS5_VABI sceContentExportFromFile(std::int32_t exportId, const ContentExportParam* param, const char* sourcePath, char* path, std::size_t pathBufLen);
std::int32_t APS5_VABI sceContentExportFromFileWithThumbnail(std::int32_t exportId, const ContentExportParam* param, const char* sourcePath, const char* thumbnailPath, char* path, std::size_t pathBufLen);
std::int32_t APS5_VABI sceContentExportFromData(std::int32_t exportId, const ContentExportParam* param, std::size_t contentLength, ContentExportDataProvideFunction func, void* userData, char* path, std::size_t pathBufLen);
std::int32_t APS5_VABI sceContentExportFromDataWithThumbnail(std::int32_t exportId, const ContentExportParam* param, const ContentExportCallbackParam* contentCallbackParam, const char* thumbnailType, const ContentExportCallbackParam* thumbnailCallbackParam, char* path, std::size_t pathBufLen);
std::int32_t APS5_VABI sceContentExportGetProgress(std::int32_t exportId);
std::int32_t APS5_VABI sceContentExportCancel(std::int32_t exportId);
}

namespace {

constexpr std::int32_t ErrorCanceled = static_cast<std::int32_t>(0x809D3001u);
constexpr std::int32_t ErrorFileNotFound = static_cast<std::int32_t>(0x809D3011u);
constexpr std::int32_t ErrorNotSupportedFormat = static_cast<std::int32_t>(0x809D3012u);
constexpr std::int32_t ErrorLargeTitle = static_cast<std::int32_t>(0x809D3013u);
constexpr std::int32_t ErrorLargeComment = static_cast<std::int32_t>(0x809D3015u);
constexpr std::int32_t ErrorNotSupportedThumbnail = static_cast<std::int32_t>(0x809D3019u);
constexpr std::int32_t ErrorDataProvideFunction = static_cast<std::int32_t>(0x809D301Bu);

void Require(bool value) { if (!value) std::abort(); }

template <typename TAction>
bool Throws(TAction action) {
    try {
        action();
    } catch (const std::logic_error&) {
        return true;
    }
    return false;
}

int allocator = 0;

std::string ReadHost(const char* guestPath) {
    std::ifstream file(std::string(".") + guestPath, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

void WriteHost(const char* path, const std::string& contents) {
    std::ofstream file(path, std::ios::binary);
    file << contents;
    Require(static_cast<bool>(file));
}

ContentExportParam Param(const char* contentType) {
    ContentExportParam param{};
    std::strcpy(param.title, "Title");
    std::strcpy(param.comment, "Comment");
    std::strcpy(param.content_type, contentType);
    return param;
}

struct Chunks {
    const char* parts[3];
    std::size_t count;
    std::size_t next;
    std::int32_t exportId;
    std::int32_t progress;
    bool cancel;
};

std::int32_t APS5_VABI Provide(void** data, std::size_t* size, void* userData) {
    auto& chunks = *static_cast<Chunks*>(userData);
    chunks.progress = sceContentExportGetProgress(chunks.exportId);
    if (chunks.cancel && chunks.next == 1) sceContentExportCancel(chunks.exportId);
    if (chunks.next == chunks.count) {
        *data = nullptr;
        *size = 0;
        return 1;
    }
    const char* part = chunks.parts[chunks.next++];
    *data = const_cast<char*>(part);
    *size = std::strlen(part);
    return chunks.next == chunks.count ? 1 : 0;
}

std::int32_t APS5_VABI Fail(void**, std::size_t*, void*) {
    return -1;
}

void Lifecycle() {
    ContentExportInitParam2 param{&allocator, &allocator, nullptr, 0x10000, 0, 0};
    Require(Throws([] { sceContentExportInit2(nullptr); }));
    Require(Throws([] { sceContentExportTerm(); }));
    Require(Throws([] { sceContentExportStart(); }));
    ContentExportInitParam2 missing = param;
    missing.free_func = nullptr;
    Require(Throws([&] { sceContentExportInit2(&missing); }));
    ContentExportInitParam2 reserved = param;
    reserved.reserved1 = 1;
    Require(Throws([&] { sceContentExportInit2(&reserved); }));
    Require(sceContentExportInit2(&param) == 0);
    Require(Throws([&] { sceContentExportInit2(&param); }));
    Require(sceContentExportTerm() == 0);
    Require(Throws([] { sceContentExportTerm(); }));
    Require(sceContentExportInit2(&param) == 0);
    const std::int32_t first = sceContentExportStart();
    const std::int32_t second = sceContentExportStart();
    Require(first > 0 && second > 0 && first != second);
    Require(sceContentExportGetProgress(first) == 0);
    Require(sceContentExportCancel(first) == 0);
    Require(sceContentExportFinish(first) == 0);
    Require(Throws([&] { sceContentExportFinish(first); }));
    Require(Throws([&] { sceContentExportGetProgress(first); }));
    Require(Throws([&] { sceContentExportCancel(first); }));
    Require(sceContentExportTerm() == 0);
    Require(sceContentExportInit2(&param) == 0);
    Require(Throws([&] { sceContentExportFinish(second); }));
}

void Files(std::int32_t exportId) {
    WriteHost("photo.png", "png-bytes");
    WriteHost("thumb.JPG", "jpeg-thumb");
    WriteHost("thumb.bmp", "bmp-thumb");
    char path[64] = {};
    const ContentExportParam png = Param("image/png");
    Require(sceContentExportFromFile(exportId, &png, "/photo.png", path, sizeof(path)) == 0);
    Require(std::string(path).rfind("/_ce/", 0) == 0 && std::string(path).ends_with(".png"));
    Require(ReadHost(path) == "png-bytes");
    Require(sceContentExportGetProgress(exportId) == 100);

    char second[64] = {};
    Require(sceContentExportFromFile(exportId, &png, "/photo.png", second, sizeof(second)) == 0);
    Require(std::string(second) != path && ReadHost(second) == "png-bytes");

    char withThumbnail[64] = {};
    Require(sceContentExportFromFileWithThumbnail(exportId, &png, "/photo.png", "/thumb.JPG", withThumbnail, sizeof(withThumbnail)) == 0);
    const std::string exported(withThumbnail);
    Require(ReadHost(withThumbnail) == "png-bytes");
    Require(ReadHost((exported.substr(0, exported.size() - 4) + "_thumbnail.jpg").c_str()) == "jpeg-thumb");

    char untouched[64] = "unchanged";
    Require(sceContentExportFromFile(exportId, &png, "/missing.png", untouched, sizeof(untouched)) == ErrorFileNotFound);
    Require(sceContentExportFromFileWithThumbnail(exportId, &png, "/photo.png", "/missing.jpg", untouched, sizeof(untouched)) == ErrorFileNotFound);
    Require(sceContentExportFromFileWithThumbnail(exportId, &png, "/photo.png", "/thumb.bmp", untouched, sizeof(untouched)) == ErrorNotSupportedThumbnail);
    const ContentExportParam text = Param("text/plain");
    Require(sceContentExportFromFile(exportId, &text, "/photo.png", untouched, sizeof(untouched)) == ErrorNotSupportedFormat);
    ContentExportParam longTitle = png;
    std::memset(longTitle.title, 'a', sizeof(longTitle.title));
    Require(sceContentExportFromFile(exportId, &longTitle, "/photo.png", untouched, sizeof(untouched)) == ErrorLargeTitle);
    ContentExportParam longComment = png;
    std::memset(longComment.comment, 'a', sizeof(longComment.comment));
    Require(sceContentExportFromFile(exportId, &longComment, "/photo.png", untouched, sizeof(untouched)) == ErrorLargeComment);
    Require(std::string(untouched) == "unchanged");
    Require(Throws([&] { sceContentExportFromFile(exportId, &png, "/photo.png", untouched, 4); }));
    Require(Throws([&] { sceContentExportFromFile(exportId, nullptr, "/photo.png", untouched, sizeof(untouched)); }));
    Require(Throws([&] { sceContentExportFromFileWithThumbnail(exportId, &png, "/photo.png", nullptr, untouched, sizeof(untouched)); }));
    Require(Throws([&] { sceContentExportFromFile(exportId + 1000, &png, "/photo.png", untouched, sizeof(untouched)); }));
}

void Data(std::int32_t exportId) {
    const ContentExportParam jpeg = Param("image/jpeg");
    char path[64] = {};
    Chunks chunks{{"abc", "defg", ""}, 2, 0, exportId, -1, false};
    Require(sceContentExportFromData(exportId, &jpeg, 7, Provide, &chunks, path, sizeof(path)) == 0);
    Require(std::string(path).ends_with(".jpg") && ReadHost(path) == "abcdefg");
    Require(chunks.next == 2 && chunks.progress == 42);
    Require(sceContentExportGetProgress(exportId) == 100);

    char untouched[64] = "unchanged";
    Chunks shortData{{"abc", "", ""}, 1, 0, exportId, -1, false};
    Require(sceContentExportFromData(exportId, &jpeg, 7, Provide, &shortData, untouched, sizeof(untouched)) == ErrorDataProvideFunction);
    Chunks longData{{"abc", "defg", ""}, 2, 0, exportId, -1, false};
    Require(sceContentExportFromData(exportId, &jpeg, 5, Provide, &longData, untouched, sizeof(untouched)) == ErrorDataProvideFunction);
    Require(sceContentExportFromData(exportId, &jpeg, 5, Fail, nullptr, untouched, sizeof(untouched)) == ErrorDataProvideFunction);
    Chunks canceled{{"abc", "defg", "hi"}, 3, 0, exportId, -1, true};
    Require(sceContentExportFromData(exportId, &jpeg, 9, Provide, &canceled, untouched, sizeof(untouched)) == ErrorCanceled);
    Require(canceled.next == 2);
    Require(std::string(untouched) == "unchanged");
    Require(Throws([&] { sceContentExportFromData(exportId, &jpeg, 5, nullptr, nullptr, untouched, sizeof(untouched)); }));

    Chunks content{{"video", "", ""}, 1, 0, exportId, -1, false};
    Chunks thumbnail{{"png", "", ""}, 1, 0, exportId, -1, false};
    const ContentExportCallbackParam contentParam{5, Provide, &content};
    const ContentExportCallbackParam thumbnailParam{3, Provide, &thumbnail};
    const ContentExportParam mp4 = Param("video/mp4");
    char withThumbnail[64] = {};
    Require(sceContentExportFromDataWithThumbnail(exportId, &mp4, &contentParam, "image/png", &thumbnailParam, withThumbnail, sizeof(withThumbnail)) == 0);
    const std::string exported(withThumbnail);
    Require(exported.ends_with(".mp4") && ReadHost(withThumbnail) == "video");
    Require(ReadHost((exported.substr(0, exported.size() - 4) + "_thumbnail.png").c_str()) == "png");
    Require(thumbnail.progress == 62);
    Require(sceContentExportFromDataWithThumbnail(exportId, &mp4, &contentParam, "image/gif", &thumbnailParam, untouched, sizeof(untouched)) == ErrorNotSupportedThumbnail);
    Require(Throws([&] { sceContentExportFromDataWithThumbnail(exportId, &mp4, nullptr, "image/png", &thumbnailParam, untouched, sizeof(untouched)); }));
}

}

int main() {
    const auto original = std::filesystem::current_path();
    const auto directory = original / ("contentexport-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(directory));
    std::filesystem::current_path(directory);
    Lifecycle();
    const std::int32_t exportId = sceContentExportStart();
    Files(exportId);
    Data(exportId);
    std::size_t exported = 0;
    for (const auto& entry : std::filesystem::directory_iterator("_ce")) exported += entry.file_size() > 0 ? 1 : 0;
    Require(exported == 7);
    Require(sceContentExportFinish(exportId) == 0);
    Require(sceContentExportTerm() == 0);
    std::filesystem::current_path(original);
    std::filesystem::remove_all(directory);
}
