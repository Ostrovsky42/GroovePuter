#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include "src/eye_pair_sync/eye_output_mode.h"

namespace {

int openReceiver() {
    const int sock = socket(AF_INET, SOCK_DGRAM, 0);
    assert(sock >= 0);

    int reuse = 1;
    assert(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
                      &reuse, sizeof(reuse)) == 0);
    timeval timeout{};
    timeout.tv_sec = 1;
    assert(setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
                      &timeout, sizeof(timeout)) == 0);

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(9876);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    assert(bind(sock, reinterpret_cast<const sockaddr*>(&address),
                sizeof(address)) == 0);
    return sock;
}

template <typename Packet>
Packet receivePacket(int sock) {
    Packet packet{};
    const ssize_t received = recvfrom(sock, &packet, sizeof(packet), 0,
                                      nullptr, nullptr);
    if (received < 0) std::perror("recvfrom");
    assert(received == static_cast<ssize_t>(sizeof(packet)));
    return packet;
}

}  // namespace

int main() {
    static_assert(sizeof(eye_output_mode_packet_t) == sizeof(eye_gvep_packet_t),
                  "UDP smoke decoder expects both current v2 packets to be 23 bytes");
    const int receiver = openReceiver();
    eye_output_mode_init();
    const eye_transport_diagnostics_t before =
        eye_output_mode_transport_diagnostics();

    eye_gvep_notify_transport(true);
    const eye_gvep_packet_t play = receivePacket<eye_gvep_packet_t>(receiver);
    assert(play.magic == EYE_SYNC_MAGIC_GVEP);
    assert(play.version == EYE_SYNC_VERSION_GVEP);
    assert(play.event_type == EYE_GVEP_TRANSPORT);
    assert(play.value0 == 1u);
    assert(play.crc == eye_gvep_calc_crc8(&play));

    eye_gvep_notify_transport(false);
    const eye_gvep_packet_t stop = receivePacket<eye_gvep_packet_t>(receiver);
    assert(stop.session_id == play.session_id);
    assert(stop.seq > play.seq);
    assert(stop.event_type == EYE_GVEP_TRANSPORT);
    assert(stop.value0 == 0u);
    assert(stop.crc == eye_gvep_calc_crc8(&stop));

    const eye_transport_diagnostics_t after =
        eye_output_mode_transport_diagnostics();
    assert(after.send_attempts == before.send_attempts + 2u);
    assert(after.send_accepted == before.send_accepted + 2u);
    assert(after.send_rejected == before.send_rejected);

    close(receiver);
    std::printf("Dual-Eye UDP transport PLAY/STOP loopback: PASS\n");
    return 0;
}
