#include "packet.hpp"

#include <cstdio>
#include <cstring>
#include <cerrno>

Packet::Packet(Packet&& other) noexcept
    : data_(other.data_),
      length_(other.length_),
      qh_(other.qh_),
      id_(other.id_),
      arrival_(other.arrival_) {
    other.qh_ = nullptr;
}

Packet& Packet::operator=(Packet&& other) noexcept {
    if (this != &other) {
        data_ = other.data_;
        length_ = other.length_;
        qh_ = other.qh_;
        id_ = other.id_;
        arrival_ = other.arrival_;
        other.qh_ = nullptr;
    }
    return *this;
}

bool Packet::verdict(uint32_t v, const char* name) {
    if (qh_ == nullptr) return false;  // moved-from or already finalised
    const int ret = nfq_set_verdict(qh_, id_, v, 0, nullptr);
    qh_ = nullptr;  // a packet receives exactly one verdict
    if (ret < 0) {
        std::fprintf(stderr, "[error] nfq_set_verdict(%s) failed for packet %u: %s\n", name, id_,
                     std::strerror(errno));
        return false;
    }
    return true;
}

bool Packet::accept() { return verdict(NF_ACCEPT, "ACCEPT"); }
bool Packet::drop() { return verdict(NF_DROP, "DROP"); }
