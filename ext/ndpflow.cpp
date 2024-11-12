#include <cassert>
#include "ndpflow.h"
#include "../coresim/event.h"
#include "../coresim/packet.h"
#include "ndphost.h"

extern double get_current_time();
extern void add_to_event_queue(Event *ev);
constexpr double NDPFlow::RTO;

extern uint32_t num_outstanding_packets;

Packet::bytes_t NDPFlow::send(Packet::seqno_t seqno, bool pulled) {
    Packet::bytes_t bytes_sent = std::min(size - seqno, Packet::mss);
    bool last_packet = (seqno + bytes_sent >= size) ? true : false;
    auto p = new StormPacket(this, src, dst, seqno, bytes_sent + 40, last_packet);
    //std::cout << "Packet has seqno " << p->seqno() << '\n';
    total_pkt_sent++;
    auto h = static_cast<NDPHost*>(src);
    //(pulled) ? h->enqueue_front_nic_buffer(p) : h->enqueue_back_nic_buffer(p);
    h->enqueue_back_nic_buffer(p);
    return bytes_sent;
}

void NDPFlow::send_pull(PullPkt::pullctr_t ctr) {
    auto pull = new PullPkt(this, dst, src, ctr);
    add_to_event_queue(new PacketQueuingEvent(get_current_time(), pull, dst->queue));
}

void NDPFlow::send_ack(Packet::seqno_t ackno, Packet::seqno_t cum_ack) {
    auto pkt = new StormAckPkt(this, dst, src, ackno, cum_ack);
    add_to_event_queue(new PacketQueuingEvent(get_current_time(), pkt, dst->queue));
}

void NDPFlow::send_nack(Packet::seqno_t nackno, bool pull) {
    auto pkt = new StormNackPkt(this, dst, src, nackno, pull);
    add_to_event_queue(new PacketQueuingEvent(get_current_time(), pkt, dst->queue));
}

void NDPFlow::receive(Packet *pkt) {
    if (finished) {
        if (pkt->type == STORM_PACKET) {
            ++num_bad_retransmits_received;
        }
        delete pkt;
        return;
    }
    auto sender = static_cast<NDPHost*>(src);
    auto recv = static_cast<NDPHost*>(dst);

    switch(pkt->type) {

        case STORM_ACK: {
            //Remove from sent times. This packet need not be rtx on timeout
            auto ack = static_cast<StormAckPkt*>(pkt);
            _sent_times.erase(ack->_ackno);
            
            if (ack->_ackno > _highest_recvd_ackno) {
                _highest_recvd_ackno = ack->_ackno;
            }
            if (ack->_cum_ackno > _sender_cum_ackno) {
                _sender_cum_ackno = ack->_cum_ackno;
            }

            //Also erase any cum_acked packets..
            for (auto it = _sent_times.begin(); it != _sent_times.end();) {
                if (it->first < _sender_cum_ackno) {
                    it = _sent_times.erase(it);
                }
                break;
            }

            if (_sender_cum_ackno >= size) {
                finished = true;
                add_to_event_queue(new FlowFinishedEvent(get_current_time(), this));
                if (_timer != NULL) {
                    _timer->cancelled = true;
                    _timer = NULL;
                }
            }
            else {
                update_rtx_timer();
            }

            break;
        }

        case STORM_NACK: {
            //Put into retransmit queue:
            auto nack = static_cast<StormNackPkt*>(pkt);
            if (nack->_pull) {
                send(nack->_nackno, true);
            }
            else {
                _rtx_list.insert(nack->_nackno);
            }
            update_rtx_timer();
            break;
        }

        case PULL: {
            auto pullpkt = static_cast<PullPkt*>(pkt);
            while (_sender_pullno < pullpkt->_ctr) {
                ++_sender_pullno;
                //Check rtx queue
                if (!_rtx_list.empty()) {
                    ++num_retransmits;
                    send(*(_rtx_list.begin()), true);
                    _rtx_list.erase(_rtx_list.begin());
                }
                else {
                    send_pending_data();
                }
            }
            break;
        }
        case STORM_PACKET: {
            if (!recv->has_active_flow(this)) {
                recv->add_active_flows(this);
            }
            auto data_pkt = static_cast<StormPacket*>(pkt);
            if (first_byte_receive_time == -1) {
                first_byte_receive_time = get_current_time();
            }
            if (data_pkt->is_header) {
                ++num_trimmed_packets_received;
                bool pull = data_pkt->_last_packet;
                send_nack(data_pkt->_seqno, pull);
            }
            else {
                ++num_packets_received;
                ++received_count;
                if (data_pkt->_seqno == _cum_ackno) { //Expected packet, 
                    /*Should actually increment cum_ackno by size of packet, but since all
                    packets except possibly for that last one may not be of MSS size, we can continue
                    to increment cum_ackno by MSS
                    */
                    _cum_ackno = data_pkt->_seqno + Packet::mss;
                    //Any other packets which now can be cum_acked?
                    for (auto it = _received.begin(); it != _received.end() && *it == _cum_ackno;) {
                        _cum_ackno += Packet::mss;
                        it = _received.erase(it);
                    }

                    //Update some global variables used for logging utilization
                    if (num_outstanding_packets >= 1) { 
                        num_outstanding_packets -= 1;
                    }
                    log_utilization(data_pkt->size);
                }
                else if (data_pkt->_seqno < _cum_ackno) {
                    //Bad retransmit. Do nothing
                    ++num_bad_retransmits_received;
                }
                else {//Out of order
                    _received.insert(data_pkt->_seqno);
                    //Update some global variables used for logging utilization
                    if (num_outstanding_packets >= 1) { //Again continuing with the assumption that all packets are full sized.
                        num_outstanding_packets -= 1;
                    }
                    log_utilization(data_pkt->size);
                }
                send_ack(data_pkt->_seqno, _cum_ackno);
            }
            _is_finished_at_receiver = (_cum_ackno  >= size) ? true : false;
            if (_is_finished_at_receiver || data_pkt->_last_packet) {
                _pull_queue.clear();
            }
            else {
                _pull_queue.push_back(++_receiver_pullno);
                if (recv->pacer_free()) {
                    //To start pacer...
                    recv->schedule_pull_pacer();
                }
            }
            break;
        }
        default:
            assert(false);
    }
    delete pkt;
}

void NDPFlow::start_flow() {
    send_intial_window();
}

void NDPFlow::send_intial_window() {
    while ( (_seqno < size) && (_seqno < 49 * Packet::mss) ) {
        send_pending_data();   
    }
}

void NDPFlow::send_pending_data() {
    if (_seqno >= size) {
        return;
    }
    auto bytes_sent = send(_seqno, false);
    _seqno += bytes_sent;
}


void NDPFlow::update_rtx_timer() {
    if (_timer != NULL) {
        _timer->cancelled = true;
    }
    _timer = new NDPRtxTimerEvent(get_current_time() + RTO, this);
    add_to_event_queue(_timer);
}


void NDPRtxTimerEvent::process_event() {
    _flow->_timer = NULL;
    //Find all packets which need RTX, enqueue at back of NIC
    for (auto it = _flow->_sent_times.begin(); it != _flow->_sent_times.end();) {
        auto [seqno, sent_time] = *it;
        if (get_current_time() - sent_time >= NDPFlow::RTO) {
            //Need RTX
            ++_flow->num_retransmits;
            _flow->send(seqno, false);
            it = _flow->_sent_times.erase(it);
        }
        else {
            ++it;
        }

    }
}

