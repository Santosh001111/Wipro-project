#include "policer.hpp"

void Policer::process(Packet& pkt) {
    const uint32_t len = pkt.length();
    if (bucket_.consume(len)) {
        pkt.accept();
        stats_.record_accept(len);
    } else {
        pkt.drop();
        stats_.record_drop(len);
    }
}
