#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <vector>

extern "C" {
int APS5_VABI sceVoiceQoSInit(void*, std::uint32_t, std::int32_t);
int APS5_VABI sceVoiceQoSEnd(void);
int APS5_VABI sceVoiceQoSCreateLocalEndpoint(std::int32_t*, std::int32_t, std::int32_t, std::int32_t);
int APS5_VABI sceVoiceQoSDeleteLocalEndpoint(std::int32_t);
int APS5_VABI sceVoiceQoSCreateRemoteEndpoint(std::int32_t*);
int APS5_VABI sceVoiceQoSDeleteRemoteEndpoint(std::int32_t);
int APS5_VABI sceVoiceQoSConnect(std::int32_t*, std::int32_t, std::int32_t);
int APS5_VABI sceVoiceQoSDisconnect(std::int32_t);
int APS5_VABI sceVoiceQoSSetLocalEndpointAttribute(std::int32_t, std::int32_t, const void*, std::int32_t);
int APS5_VABI sceVoiceQoSGetLocalEndpointAttribute(std::int32_t, std::int32_t, void*, std::int32_t);
int APS5_VABI sceVoiceQoSSetRemoteEndpointAttribute(std::int32_t, std::int32_t, const void*, std::int32_t);
int APS5_VABI sceVoiceQoSReadPacket(std::int32_t, void*, std::uint32_t*);
int APS5_VABI sceVoiceQoSWritePacket(std::int32_t, const void*, std::uint32_t*);
}

static void Require(bool value) { if (!value) std::abort(); }

template <typename TCall>
static void RequireThrows(TCall call) {
    try {
        call();
    } catch (const std::logic_error&) {
        return;
    }
    std::abort();
}

static void CheckEndpoints() {
    constexpr int argumentInvalid = static_cast<int>(0x804E0902);
    std::int32_t local = -1;
    std::int32_t secondLocal = -1;
    std::int32_t remote = -1;
    std::int32_t connection = -1;
    Require(sceVoiceQoSCreateLocalEndpoint(nullptr, 1, 0, 0) == argumentInvalid);
    Require(sceVoiceQoSCreateRemoteEndpoint(nullptr) == argumentInvalid);
    Require(sceVoiceQoSCreateLocalEndpoint(&local, 1, 0, 0) == 0);
    Require(sceVoiceQoSCreateLocalEndpoint(&secondLocal, 2, 0, 0) == 0);
    Require(local != secondLocal);
    Require(sceVoiceQoSCreateRemoteEndpoint(&remote) == 0);
    RequireThrows([&] { sceVoiceQoSConnect(&connection, local + secondLocal + 7, remote); });
    RequireThrows([&] { sceVoiceQoSConnect(&connection, local, remote + 7); });
    Require(sceVoiceQoSConnect(nullptr, local, remote) == argumentInvalid);
    Require(sceVoiceQoSConnect(&connection, local, remote) == 0);
    RequireThrows([&] { sceVoiceQoSConnect(&connection, local, remote); });

    const std::int32_t volume = 70;
    const std::int32_t bitRate = 16000;
    std::int32_t value = -1;
    Require(sceVoiceQoSSetLocalEndpointAttribute(local, 0, nullptr, 4) == argumentInvalid);
    Require(sceVoiceQoSSetLocalEndpointAttribute(local, 0, &volume, sizeof(volume)) == 0);
    Require(sceVoiceQoSSetLocalEndpointAttribute(local, 4, &bitRate, sizeof(bitRate)) == 0);
    Require(sceVoiceQoSGetLocalEndpointAttribute(local, 0, &value, sizeof(value)) == 0 && value == volume);
    Require(sceVoiceQoSGetLocalEndpointAttribute(local, 4, &value, sizeof(value)) == 0 && value == bitRate);
    RequireThrows([&] { sceVoiceQoSGetLocalEndpointAttribute(secondLocal, 0, &value, sizeof(value)); });
    RequireThrows([&] { sceVoiceQoSGetLocalEndpointAttribute(local, 0, &value, 2); });
    Require(sceVoiceQoSGetLocalEndpointAttribute(local, 5, &value, sizeof(value)) == 0 && value == 0);
    RequireThrows([&] { sceVoiceQoSSetLocalEndpointAttribute(local, 5, &volume, sizeof(volume)); });
    RequireThrows([&] { sceVoiceQoSSetLocalEndpointAttribute(local, 7, &volume, sizeof(volume)); });
    Require(sceVoiceQoSSetRemoteEndpointAttribute(remote, 7, &volume, sizeof(volume)) == 0);
    RequireThrows([&] { sceVoiceQoSSetRemoteEndpointAttribute(remote, 0, &volume, sizeof(volume)); });

    std::vector<std::uint8_t> packet(256, 0x5A);
    std::uint32_t size = static_cast<std::uint32_t>(packet.size());
    Require(sceVoiceQoSReadPacket(connection, nullptr, &size) == argumentInvalid);
    Require(sceVoiceQoSReadPacket(connection, packet.data(), &size) == 0 && size == 0);
    for (std::uint8_t byte : packet) Require(byte == 0x5A);
    size = static_cast<std::uint32_t>(packet.size());
    Require(sceVoiceQoSWritePacket(connection, packet.data(), nullptr) == argumentInvalid);
    Require(sceVoiceQoSWritePacket(connection, packet.data(), &size) == 0 && size == packet.size());
    RequireThrows([&] { sceVoiceQoSReadPacket(connection + 7, packet.data(), &size); });

    RequireThrows([&] { sceVoiceQoSDeleteLocalEndpoint(local); });
    RequireThrows([&] { sceVoiceQoSDeleteRemoteEndpoint(remote); });
    Require(sceVoiceQoSDisconnect(connection) == 0);
    RequireThrows([&] { sceVoiceQoSDisconnect(connection); });
    RequireThrows([&] { sceVoiceQoSWritePacket(connection, packet.data(), &size); });
    Require(sceVoiceQoSDeleteLocalEndpoint(local) == 0);
    Require(sceVoiceQoSDeleteRemoteEndpoint(remote) == 0);
    RequireThrows([&] { sceVoiceQoSDeleteLocalEndpoint(local); });
    RequireThrows([&] { sceVoiceQoSDeleteRemoteEndpoint(remote); });
}

int main(int argc, char** argv) {
    Require(argc == 2);
    constexpr int argumentInvalid = static_cast<int>(0x804E0902);
    constexpr int initialized = static_cast<int>(0x804E0905);
    const auto appType = static_cast<std::int32_t>(std::strtoul(argv[1], nullptr, 16));
    std::vector<std::uint8_t> memory(0x40000, 0xAA);
    std::int32_t id = -1;
    RequireThrows([] { sceVoiceQoSEnd(); });
    RequireThrows([&] { sceVoiceQoSCreateLocalEndpoint(&id, 1, 0, 0); });
    Require(sceVoiceQoSInit(nullptr, 0x40000, appType) == argumentInvalid);
    Require(sceVoiceQoSInit(nullptr, 0, appType) == argumentInvalid);
    for (std::uint32_t size : {0u, 1u, 0x100u, 0x3FFFFu, 0x40001u, 0x80000u, 0xFFFFFFFFu}) Require(sceVoiceQoSInit(memory.data(), size, appType) == argumentInvalid);
    for (std::int32_t type : {0, 1, 0x08000000, 0x20000001, 0x30000000, 0x40000000, static_cast<std::int32_t>(0x80000000), -1}) {
        Require(sceVoiceQoSInit(memory.data(), 0x40000, type) == argumentInvalid);
    }
    for (std::uint8_t byte : memory) Require(byte == 0xAA);
    Require(sceVoiceQoSInit(memory.data(), 0x40000, appType) == 0);
    Require(sceVoiceQoSInit(memory.data(), 0x40000, appType) == initialized);
    Require(sceVoiceQoSInit(nullptr, 0, 0) == initialized);
    CheckEndpoints();

    std::int32_t local = -1;
    std::int32_t remote = -1;
    std::int32_t connection = -1;
    Require(sceVoiceQoSCreateLocalEndpoint(&local, 1, 0, 0) == 0);
    Require(sceVoiceQoSCreateRemoteEndpoint(&remote) == 0);
    Require(sceVoiceQoSConnect(&connection, local, remote) == 0);
    Require(sceVoiceQoSEnd() == 0);
    RequireThrows([] { sceVoiceQoSEnd(); });
    RequireThrows([&] { sceVoiceQoSDisconnect(connection); });
    Require(sceVoiceQoSInit(memory.data(), 0x40000, appType) == 0);
    RequireThrows([&] { sceVoiceQoSDeleteLocalEndpoint(local); });
    CheckEndpoints();
    Require(sceVoiceQoSEnd() == 0);
}
