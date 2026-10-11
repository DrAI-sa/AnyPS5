#include <cstdint>
#include <cstddef>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace {

constexpr std::int32_t ErrorCanceled = static_cast<std::int32_t>(0x809D3001u);
constexpr std::int32_t ErrorFileNotFound = static_cast<std::int32_t>(0x809D3011u);
constexpr std::int32_t ErrorNotSupportedFormat = static_cast<std::int32_t>(0x809D3012u);
constexpr std::int32_t ErrorLargeTitle = static_cast<std::int32_t>(0x809D3013u);
constexpr std::int32_t ErrorLargeComment = static_cast<std::int32_t>(0x809D3015u);
constexpr std::int32_t ErrorDiskFull = static_cast<std::int32_t>(0x809D3017u);
constexpr std::int32_t ErrorNotSupportedThumbnail = static_cast<std::int32_t>(0x809D3019u);
constexpr std::int32_t ErrorDataProvideFunction = static_cast<std::int32_t>(0x809D301Bu);

constexpr std::string_view ExportDirectory = "/_ce";
constexpr std::size_t ChunkSize = 64 * 1024;

struct Session {
    std::atomic<bool> busy{false};
    std::atomic<bool> canceled{false};
    std::atomic<std::int32_t> progress{0};
};

std::atomic<bool> g_initialized{false};
std::mutex g_mutex;
std::map<std::int32_t, std::shared_ptr<Session>> g_sessions;
std::int32_t g_nextSession = 1;
std::uint64_t g_nextExport = 1;

std::string Message(const char* funcName, const char* text) {
    return std::string(funcName) + ": " + text;
}

std::shared_ptr<Session> FindSession(const char* funcName, std::int32_t exportId) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized.load()) throw std::logic_error(Message(funcName, "not initialized"));
    const auto entry = g_sessions.find(exportId);
    if (entry == g_sessions.end()) throw std::invalid_argument(Message(funcName, "unknown export id"));
    return entry->second;
}

class BusyScope {
public:
    BusyScope(const char* funcName, Session& session) : session(session) {
        bool expected = false;
        if (!session.busy.compare_exchange_strong(expected, true)) throw std::logic_error(Message(funcName, "an export is already running in this session"));
        session.canceled.store(false);
        session.progress.store(0);
    }
    ~BusyScope() { session.busy.store(false); }
    BusyScope(const BusyScope&) = delete;
    BusyScope& operator=(const BusyScope&) = delete;

private:
    Session& session;
};

template <std::size_t TSize>
bool IsTerminated(const char (&text)[TSize]) {
    return std::memchr(text, 0, TSize) != nullptr;
}

const char* ContentExtension(std::string_view contentType) {
    if (contentType == "image/jpeg") return ".jpg";
    if (contentType == "image/png") return ".png";
    if (contentType == "image/gif") return ".gif";
    if (contentType == "video/mp4") return ".mp4";
    if (contentType == "video/webm") return ".webm";
    return nullptr;
}

const char* ThumbnailExtension(std::string_view contentType) {
    if (contentType == "image/jpeg") return ".jpg";
    if (contentType == "image/png") return ".png";
    return nullptr;
}

const char* ThumbnailFileExtension(const std::filesystem::path& path) {
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char character) { return static_cast<char>(character >= 'A' && character <= 'Z' ? character - 'A' + 'a' : character); });
    if (extension == ".jpg" || extension == ".jpeg") return ".jpg";
    if (extension == ".png") return ".png";
    return nullptr;
}

std::int32_t CheckParam(const ContentExportParam& param, const char*& extension) {
    if (!IsTerminated(param.title)) return ErrorLargeTitle;
    if (!IsTerminated(param.comment)) return ErrorLargeComment;
    if (!IsTerminated(param.content_type)) return ErrorNotSupportedFormat;
    extension = ContentExtension(param.content_type);
    return extension == nullptr ? ErrorNotSupportedFormat : 0;
}

struct Destination {
    std::string guestPath;
    std::filesystem::path content;
    std::filesystem::path thumbnail;
};

Destination ReserveDestination(const char* funcName, const char* extension, const char* thumbnailExtension, std::size_t pathBufLen) {
    const std::filesystem::path directory = ResolvePath_nid_no_patch(std::string(ExportDirectory).c_str());
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if (error) throw std::system_error(error, Message(funcName, "creating the export directory"));
    std::lock_guard lock(g_mutex);
    for (;;) {
        const std::string name = std::to_string(g_nextExport++);
        Destination destination{std::string(ExportDirectory) + "/" + name + extension, directory / (name + extension), {}};
        if (thumbnailExtension != nullptr) destination.thumbnail = directory / (name + "_thumbnail" + thumbnailExtension);
        if (destination.guestPath.size() + 1 > pathBufLen) throw std::invalid_argument(Message(funcName, "path buffer is too small"));
        const bool taken = std::filesystem::exists(destination.content, error);
        if (error) throw std::system_error(error, Message(funcName, "checking the export directory"));
        const bool thumbnailTaken = thumbnailExtension != nullptr && std::filesystem::exists(destination.thumbnail, error);
        if (error) throw std::system_error(error, Message(funcName, "checking the export directory"));
        if (taken || thumbnailTaken) continue;
        std::ofstream reserve(destination.content, std::ios::binary);
        if (!reserve) throw std::runtime_error(Message(funcName, "cannot create the exported file"));
        return destination;
    }
}

void RemoveOutputs(const Destination& destination) {
    std::error_code error;
    std::filesystem::remove(destination.content, error);
    if (!destination.thumbnail.empty()) std::filesystem::remove(destination.thumbnail, error);
}

bool IsDiskFull(const std::filesystem::path& file, std::size_t pending) {
    std::error_code error;
    const auto space = std::filesystem::space(file.parent_path(), error);
    return !error && space.available < pending;
}

template <typename TReader>
std::int32_t Transfer(const char* funcName, Session& session, const std::filesystem::path& file, std::uint64_t total, std::uint64_t progressBase, std::uint64_t progressTotal, TReader read) {
    std::ofstream output(file, std::ios::binary | std::ios::trunc);
    if (!output) throw std::runtime_error(Message(funcName, "cannot open the exported file"));
    std::uint64_t written = 0;
    while (written < total) {
        if (session.canceled.load()) return ErrorCanceled;
        const void* data = nullptr;
        std::size_t size = 0;
        const std::int32_t result = read(written, data, size);
        if (result != 0) return result;
        output.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
        if (!output) {
            if (IsDiskFull(file, size)) return ErrorDiskFull;
            throw std::runtime_error(Message(funcName, "writing the exported file failed"));
        }
        written += size;
        session.progress.store(static_cast<std::int32_t>((progressBase + written) * 100 / progressTotal));
    }
    output.flush();
    if (!output) {
        if (IsDiskFull(file, 0)) return ErrorDiskFull;
        throw std::runtime_error(Message(funcName, "writing the exported file failed"));
    }
    if (session.canceled.load()) return ErrorCanceled;
    RecordWrittenPath_nid_no_patch(file);
    return 0;
}

std::int32_t CopyFileContents(const char* funcName, Session& session, const std::filesystem::path& source, const std::filesystem::path& file, std::uint64_t size, std::uint64_t progressBase, std::uint64_t progressTotal) {
    std::ifstream input(source, std::ios::binary);
    if (!input) return ErrorFileNotFound;
    std::array<char, ChunkSize> buffer{};
    return Transfer(funcName, session, file, size, progressBase, progressTotal, [&](std::uint64_t written, const void*& data, std::size_t& chunk) {
        chunk = static_cast<std::size_t>(std::min<std::uint64_t>(buffer.size(), size - written));
        input.read(buffer.data(), static_cast<std::streamsize>(chunk));
        if (input.gcount() != static_cast<std::streamsize>(chunk)) throw std::runtime_error(Message(funcName, "the source file changed while it was exported"));
        data = buffer.data();
        return 0;
    });
}

std::int32_t ProvideContents(const char* funcName, Session& session, const ContentExportCallbackParam& callback, const std::filesystem::path& file, std::uint64_t progressBase, std::uint64_t progressTotal) {
    return Transfer(funcName, session, file, callback.content_length, progressBase, progressTotal, [&](std::uint64_t written, const void*& data, std::size_t& chunk) {
        void* provided = nullptr;
        std::size_t providedSize = 0;
        if (callback.func(&provided, &providedSize, callback.user_data) < 0) return ErrorDataProvideFunction;
        if (provided == nullptr || providedSize == 0 || providedSize > callback.content_length - written) return ErrorDataProvideFunction;
        data = provided;
        chunk = providedSize;
        return 0;
    });
}

std::optional<std::uint64_t> RegularFileSize(const std::filesystem::path& path) {
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error)) return std::nullopt;
    const auto size = std::filesystem::file_size(path, error);
    if (error) return std::nullopt;
    return size;
}

void WriteGuestPath(const Destination& destination, char* path) {
    std::memcpy(path, destination.guestPath.c_str(), destination.guestPath.size() + 1);
}

std::int32_t ExportFile(const char* funcName, std::int32_t exportId, const ContentExportParam* param, const char* sourcePath, const char* thumbnailPath, bool withThumbnail, char* path, std::size_t pathBufLen) {
    if (param == nullptr || sourcePath == nullptr || (withThumbnail && thumbnailPath == nullptr) || path == nullptr || pathBufLen == 0) throw std::invalid_argument(Message(funcName, "invalid argument"));
    const auto session = FindSession(funcName, exportId);
    BusyScope busy(funcName, *session);
    const char* extension = nullptr;
    if (const std::int32_t result = CheckParam(*param, extension); result != 0) return result;
    const std::filesystem::path source = ResolvePath_nid_no_patch(sourcePath);
    const auto sourceSize = RegularFileSize(source);
    if (!sourceSize) return ErrorFileNotFound;
    std::filesystem::path thumbnail;
    std::optional<std::uint64_t> thumbnailSize = 0;
    const char* thumbnailExtension = nullptr;
    if (withThumbnail) {
        thumbnail = ResolvePath_nid_no_patch(thumbnailPath);
        thumbnailSize = RegularFileSize(thumbnail);
        if (!thumbnailSize) return ErrorFileNotFound;
        thumbnailExtension = ThumbnailFileExtension(thumbnail);
        if (thumbnailExtension == nullptr) return ErrorNotSupportedThumbnail;
    }
    const Destination destination = ReserveDestination(funcName, extension, thumbnailExtension, pathBufLen);
    const std::uint64_t total = std::max<std::uint64_t>(*sourceSize + *thumbnailSize, 1);
    try {
        std::int32_t result = CopyFileContents(funcName, *session, source, destination.content, *sourceSize, 0, total);
        if (result == 0 && withThumbnail) result = CopyFileContents(funcName, *session, thumbnail, destination.thumbnail, *thumbnailSize, *sourceSize, total);
        if (result != 0) {
            RemoveOutputs(destination);
            return result;
        }
    } catch (...) {
        RemoveOutputs(destination);
        throw;
    }
    session->progress.store(100);
    WriteGuestPath(destination, path);
    return 0;
}

std::int32_t ExportData(const char* funcName, std::int32_t exportId, const ContentExportParam* param, const ContentExportCallbackParam& content, const char* thumbnailType, const ContentExportCallbackParam* thumbnail, char* path, std::size_t pathBufLen) {
    if (param == nullptr || content.func == nullptr || path == nullptr || pathBufLen == 0) throw std::invalid_argument(Message(funcName, "invalid argument"));
    const auto session = FindSession(funcName, exportId);
    BusyScope busy(funcName, *session);
    const char* extension = nullptr;
    if (const std::int32_t result = CheckParam(*param, extension); result != 0) return result;
    const char* thumbnailExtension = nullptr;
    if (thumbnail != nullptr) {
        thumbnailExtension = ThumbnailExtension(thumbnailType);
        if (thumbnailExtension == nullptr) return ErrorNotSupportedThumbnail;
    }
    const Destination destination = ReserveDestination(funcName, extension, thumbnailExtension, pathBufLen);
    const std::uint64_t thumbnailLength = thumbnail != nullptr ? thumbnail->content_length : 0;
    const std::uint64_t total = std::max<std::uint64_t>(content.content_length + thumbnailLength, 1);
    try {
        std::int32_t result = ProvideContents(funcName, *session, content, destination.content, 0, total);
        if (result == 0 && thumbnail != nullptr) result = ProvideContents(funcName, *session, *thumbnail, destination.thumbnail, content.content_length, total);
        if (result != 0) {
            RemoveOutputs(destination);
            return result;
        }
    } catch (...) {
        RemoveOutputs(destination);
        throw;
    }
    session->progress.store(100);
    WriteGuestPath(destination, path);
    return 0;
}

}

extern "C" {

int APS5_VABI sceContentExportInit2(const ContentExportInitParam2* init_param) {
    if (init_param == nullptr) APS5_INVALID_ARG_EX;
    if (init_param->malloc_func == nullptr || init_param->free_func == nullptr) throw std::invalid_argument(std::string(__func__) + ": missing allocator functions");
    if (init_param->reserved0 != 0 || init_param->reserved1 != 0) throw std::invalid_argument(std::string(__func__) + ": reserved fields are not zero");
    std::lock_guard lock(g_mutex);
    bool expected = false;
    if (!g_initialized.compare_exchange_strong(expected, true)) throw std::logic_error(std::string(__func__) + ": already initialized");
    return 0;
}

std::int32_t APS5_VABI sceContentExportStart(void) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized.load()) throw std::logic_error(std::string(__func__) + ": not initialized");
    const std::int32_t exportId = g_nextSession;
    g_nextSession = g_nextSession == INT32_MAX ? 1 : g_nextSession + 1;
    if (!g_sessions.emplace(exportId, std::make_shared<Session>()).second) throw std::logic_error(std::string(__func__) + ": export ids exhausted");
    return exportId;
}

std::int32_t APS5_VABI sceContentExportFinish(std::int32_t exportId) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized.load()) throw std::logic_error(std::string(__func__) + ": not initialized");
    const auto entry = g_sessions.find(exportId);
    if (entry == g_sessions.end()) throw std::invalid_argument(std::string(__func__) + ": unknown export id");
    if (entry->second->busy.load()) throw std::logic_error(std::string(__func__) + ": an export is still running in this session");
    g_sessions.erase(entry);
    return 0;
}

std::int32_t APS5_VABI sceContentExportFromFile(std::int32_t exportId, const ContentExportParam* param, const char* sourcePath, char* path, std::size_t pathBufLen) {
    return ExportFile(__func__, exportId, param, sourcePath, nullptr, false, path, pathBufLen);
}

std::int32_t APS5_VABI sceContentExportFromFileWithThumbnail(std::int32_t exportId, const ContentExportParam* param, const char* sourcePath, const char* thumbnailPath, char* path, std::size_t pathBufLen) {
    return ExportFile(__func__, exportId, param, sourcePath, thumbnailPath, true, path, pathBufLen);
}

std::int32_t APS5_VABI sceContentExportFromData(std::int32_t exportId, const ContentExportParam* param, std::size_t contentLength, ContentExportDataProvideFunction func, void* userData, char* path, std::size_t pathBufLen) {
    const ContentExportCallbackParam content{contentLength, func, userData};
    return ExportData(__func__, exportId, param, content, nullptr, nullptr, path, pathBufLen);
}

std::int32_t APS5_VABI sceContentExportFromDataWithThumbnail(std::int32_t exportId, const ContentExportParam* param, const ContentExportCallbackParam* contentCallbackParam, const char* thumbnailType, const ContentExportCallbackParam* thumbnailCallbackParam, char* path, std::size_t pathBufLen) {
    if (contentCallbackParam == nullptr || thumbnailType == nullptr || thumbnailCallbackParam == nullptr || thumbnailCallbackParam->func == nullptr) APS5_INVALID_ARG_EX;
    return ExportData(__func__, exportId, param, *contentCallbackParam, thumbnailType, thumbnailCallbackParam, path, pathBufLen);
}

std::int32_t APS5_VABI sceContentExportGetProgress(std::int32_t exportId) {
    return FindSession(__func__, exportId)->progress.load();
}

std::int32_t APS5_VABI sceContentExportCancel(std::int32_t exportId) {
    FindSession(__func__, exportId)->canceled.store(true);
    return 0;
}

int APS5_VABI sceContentExportTerm(void) {
    std::lock_guard lock(g_mutex);
    for (const auto& entry : g_sessions)
        if (entry.second->busy.load()) throw std::logic_error(std::string(__func__) + ": an export is still running");
    bool expected = true;
    if (!g_initialized.compare_exchange_strong(expected, false)) throw std::logic_error(std::string(__func__) + ": not initialized");
    g_sessions.clear();
    return 0;
}

}
