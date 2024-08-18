#include "cutpayloadqueue.h"
#include "../coresim/packet.h"
#include <deque>
#include <cassert>

constexpr uint32_t CutPayloadQueue::LOW_RATIO;
constexpr uint32_t CutPayloadQueue::HI_RATIO;

void CutPayloadQueue::enque(Packet *packet) {
    p_arrivals += 1;
    b_arrivals += packet->size;
    PrioLevels lvl = (packet->type == STORM_PACKET && !packet->is_header) ? LOW : HI;

    if (lvl == LOW && bytes_in_queues[LOW] + packet->size > limit_bytes) {
        lvl = HI;
        packet->strip_payload();
    }

    if (bytes_in_queues[lvl] + packet->size > limit_bytes) {
        pkt_drop++;
        drop(packet);
        return;
    }
    prio_queues[lvl].push_back(packet);
    bytes_in_queues[lvl] += packet->size;
    bytes_in_queue += packet->size;
}

Packet* CutPayloadQueue::deque() {
    if (prio_queues[LOW].empty() && prio_queues[HI].empty()) {
        return NULL;
    }
    /*If both queues are not empty, then we must select
    a servicing level
    */
    if (!prio_queues[LOW].empty() && !prio_queues[HI].empty()) {
        ++counter;

        if (counter >= LOW_RATIO + HI_RATIO) {
            counter = 0;
        }

        if (counter < HI_RATIO) {
            serv = HI;
        }
        else {
            serv = LOW;
        }
    }
    PrioLevels lvl = serv;
    switch(serv) {//Select the queue based on serv and non-emptyness of queues
        case HI:
            if (prio_queues[HI].empty()) {
                lvl = LOW;
            }
        break;
        case LOW:
            if (prio_queues[LOW].empty()) {
                lvl = HI;
            }
        break;
        default: 
            assert(false);
    }
    auto pkt = prio_queues[lvl].front();
    prio_queues[lvl].pop_front();
    bytes_in_queues[lvl] -= pkt->size;
    return pkt;
}
