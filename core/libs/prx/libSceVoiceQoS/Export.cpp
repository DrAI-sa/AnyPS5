#include <cstdint>
#include <cstddef>
#include <cstring>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr std::int32_t VOICE_QOS_APP_TYPE_GAME = 0x20000000;
constexpr std::int32_t VOICE_QOS_APP_TYPE_10000000 = 0x10000000;
constexpr std::uint32_t VOICE_QOS_MEMORY_SIZE = 0x40000;
constexpr int SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID = static_cast<int>(0x804E0902);
constexpr int SCE_VOICE_ERROR_LIBVOICEQOS_INITIALIZED = static_cast<int>(0x804E0905);

constexpr std::int32_t AttrMicVolume = 0;
constexpr std::int32_t AttrDesiredOutBitRate = 4;
constexpr std::int32_t AttrMicUsable = 5;
constexpr std::int32_t AttrRemoteMute = 7;
constexpr std::int32_t AttrSpeakerDestination = 8;

using Attributes = std::map<std::int32_t, std::vector<std::uint8_t>>;

struct Connection {
    std::int32_t localId;
    std::int32_t remoteId;
};

struct VoiceQoS {
    std::mutex mutex;
    bool initialized = false;
    std::int32_t nextLocalId = 0;
    std::int32_t nextRemoteId = 0;
    std::int32_t nextConnectionId = 0;
    std::map<std::int32_t, Attributes> locals;
    std::map<std::int32_t, Attributes> remotes;
    std::map<std::int32_t, Connection> connections;
};

VoiceQoS& State() {
    static VoiceQoS qos;
    return qos;
}

[[noreturn]] void Fail(const char* function, const std::string& why) {
    throw std::logic_error(std::string(function) + ": " + why);
}

void RequireInitialized(const VoiceQoS& qos, const char* function) {
    if (!qos.initialized) Fail(function, "library not initialized");
}

Attributes& RequireEndpoint(std::map<std::int32_t, Attributes>& endpoints, std::int32_t id, const char* kind, const char* function) {
    const auto found = endpoints.find(id);
    if (found == endpoints.end()) Fail(function, std::string("unknown ") + kind + " endpoint " + std::to_string(id));
    return found->second;
}

const Connection& RequireConnection(const VoiceQoS& qos, std::int32_t id, const char* function) {
    const auto found = qos.connections.find(id);
    if (found == qos.connections.end()) Fail(function, "unknown connection " + std::to_string(id));
    return found->second;
}

bool Connected(const VoiceQoS& qos, bool local, std::int32_t id) {
    for (const auto& [connectionId, connection] : qos.connections) {
        if ((local ? connection.localId : connection.remoteId) == id) return true;
    }
    return false;
}

void Store(Attributes& attributes, std::int32_t attributeId, const void* value, std::int32_t size) {
    const auto* bytes = static_cast<const std::uint8_t*>(value);
    attributes[attributeId].assign(bytes, bytes + size);
}

}

extern "C" {

int APS5_VABI sceVoiceQoSInit(void* mem_block, uint32_t mem_size, int32_t app_type) {
    auto& qos = State();
    std::lock_guard lock(qos.mutex);
    if (qos.initialized) return SCE_VOICE_ERROR_LIBVOICEQOS_INITIALIZED;
    if (!mem_block || mem_size != VOICE_QOS_MEMORY_SIZE || (app_type != VOICE_QOS_APP_TYPE_GAME && app_type != VOICE_QOS_APP_TYPE_10000000)) return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
    qos.initialized = true;
    return 0;
}

int APS5_VABI sceVoiceQoSEnd(void) {
    auto& qos = State();
    std::lock_guard lock(qos.mutex);
    RequireInitialized(qos, __func__);
    qos.connections.clear();
    qos.locals.clear();
    qos.remotes.clear();
    qos.initialized = false;
    return 0;
}

int APS5_VABI sceVoiceQoSCreateLocalEndpoint(int32_t* local_id, int32_t user_id, int32_t device_in_id, int32_t device_out_id) {
    (void)user_id;
    (void)device_in_id;
    (void)device_out_id;
    if (local_id == nullptr) return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
    auto& qos = State();
    std::lock_guard lock(qos.mutex);
    RequireInitialized(qos, __func__);
    const auto id = qos.nextLocalId++;
    qos.locals.emplace(id, Attributes{});
    *local_id = id;
    return 0;
}

int APS5_VABI sceVoiceQoSDeleteLocalEndpoint(int32_t local_id) {
    auto& qos = State();
    std::lock_guard lock(qos.mutex);
    RequireInitialized(qos, __func__);
    RequireEndpoint(qos.locals, local_id, "local", __func__);
    if (Connected(qos, true, local_id)) Fail(__func__, "local endpoint " + std::to_string(local_id) + " is still connected");
    qos.locals.erase(local_id);
    return 0;
}

int APS5_VABI sceVoiceQoSCreateRemoteEndpoint(int32_t* remote_id) {
    if (remote_id == nullptr) return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
    auto& qos = State();
    std::lock_guard lock(qos.mutex);
    RequireInitialized(qos, __func__);
    const auto id = qos.nextRemoteId++;
    qos.remotes.emplace(id, Attributes{});
    *remote_id = id;
    return 0;
}

int APS5_VABI sceVoiceQoSDeleteRemoteEndpoint(int32_t remote_id) {
    auto& qos = State();
    std::lock_guard lock(qos.mutex);
    RequireInitialized(qos, __func__);
    RequireEndpoint(qos.remotes, remote_id, "remote", __func__);
    if (Connected(qos, false, remote_id)) Fail(__func__, "remote endpoint " + std::to_string(remote_id) + " is still connected");
    qos.remotes.erase(remote_id);
    return 0;
}

int APS5_VABI sceVoiceQoSConnect(int32_t* connection_id, int32_t local_id, int32_t remote_id) {
    if (connection_id == nullptr) return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
    auto& qos = State();
    std::lock_guard lock(qos.mutex);
    RequireInitialized(qos, __func__);
    RequireEndpoint(qos.locals, local_id, "local", __func__);
    RequireEndpoint(qos.remotes, remote_id, "remote", __func__);
    for (const auto& [id, connection] : qos.connections) {
        if (connection.localId == local_id && connection.remoteId == remote_id) Fail(__func__, "endpoints are already connected");
    }
    const auto id = qos.nextConnectionId++;
    qos.connections.emplace(id, Connection{local_id, remote_id});
    *connection_id = id;
    return 0;
}

int APS5_VABI sceVoiceQoSDisconnect(int32_t connection_id) {
    auto& qos = State();
    std::lock_guard lock(qos.mutex);
    RequireInitialized(qos, __func__);
    RequireConnection(qos, connection_id, __func__);
    qos.connections.erase(connection_id);
    return 0;
}

int APS5_VABI sceVoiceQoSSetLocalEndpointAttribute(int32_t local_id, int32_t attribute_id, const void* value, int32_t size) {
    if (value == nullptr || size <= 0) return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
    auto& qos = State();
    std::lock_guard lock(qos.mutex);
    RequireInitialized(qos, __func__);
    auto& attributes = RequireEndpoint(qos.locals, local_id, "local", __func__);
    if (attribute_id < AttrMicVolume || attribute_id > AttrDesiredOutBitRate) Fail(__func__, "attribute " + std::to_string(attribute_id) + " cannot be set on a local endpoint");
    Store(attributes, attribute_id, value, size);
    return 0;
}

int APS5_VABI sceVoiceQoSGetLocalEndpointAttribute(int32_t local_id, int32_t attribute_id, void* value, int32_t size) {
    if (value == nullptr || size <= 0) return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
    auto& qos = State();
    std::lock_guard lock(qos.mutex);
    RequireInitialized(qos, __func__);
    const auto& attributes = RequireEndpoint(qos.locals, local_id, "local", __func__);
    if (attribute_id == AttrMicUsable) {
        if (size != static_cast<std::int32_t>(sizeof(std::int32_t))) Fail(__func__, "unexpected size " + std::to_string(size) + " for the microphone usable attribute");
        const std::int32_t usable = 0;
        std::memcpy(value, &usable, sizeof(usable));
        return 0;
    }
    if (attribute_id < AttrMicVolume || attribute_id > AttrDesiredOutBitRate) Fail(__func__, "attribute " + std::to_string(attribute_id) + " not implemented");
    const auto found = attributes.find(attribute_id);
    if (found == attributes.end()) Fail(__func__, "default value of attribute " + std::to_string(attribute_id) + " is unknown");
    if (found->second.size() != static_cast<std::size_t>(size)) Fail(__func__, "attribute " + std::to_string(attribute_id) + " was set with a different size");
    std::memcpy(value, found->second.data(), found->second.size());
    return 0;
}

int APS5_VABI sceVoiceQoSSetRemoteEndpointAttribute(int32_t remote_id, int32_t attribute_id, const void* value, int32_t size) {
    if (value == nullptr || size <= 0) return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
    auto& qos = State();
    std::lock_guard lock(qos.mutex);
    RequireInitialized(qos, __func__);
    auto& attributes = RequireEndpoint(qos.remotes, remote_id, "remote", __func__);
    if (attribute_id != AttrRemoteMute && attribute_id != AttrSpeakerDestination) Fail(__func__, "attribute " + std::to_string(attribute_id) + " cannot be set on a remote endpoint");
    Store(attributes, attribute_id, value, size);
    return 0;
}

int APS5_VABI sceVoiceQoSReadPacket(int32_t connection_id, void* data, uint32_t* size) {
    if (data == nullptr || size == nullptr) return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
    auto& qos = State();
    std::lock_guard lock(qos.mutex);
    RequireInitialized(qos, __func__);
    RequireConnection(qos, connection_id, __func__);
    *size = 0;
    return 0;
}

int APS5_VABI sceVoiceQoSWritePacket(int32_t connection_id, const void* data, uint32_t* size) {
    if (data == nullptr || size == nullptr) return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
    auto& qos = State();
    std::lock_guard lock(qos.mutex);
    RequireInitialized(qos, __func__);
    RequireConnection(qos, connection_id, __func__);
    return 0;
}

}
