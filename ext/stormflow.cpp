#include <cassert>
#include "stormflow.h"
#include "../coresim/event.h"
#include "../coresim/packet.h"
#include "stormhost.h"

extern double get_current_time();
extern void add_to_event_queue(Event *ev);
constexpr double StormFlow::RTO;

extern uint32_t num_outstanding_packets;

Packet* StormFlow::send(Packet::seqno_t seqno, bool pulled) {
    bool last_packet = (seqno + Packet::mss >= size) ? true : false;
    auto p = new StormPacket(this, src, dst, seqno, Packet::mss + 40, last_packet);
    //std::cout << "Packet has seqno " << p->seqno() << '\n';
    total_pkt_sent++;
    auto h = static_cast<StormHost*>(src);
    //(pulled) ? h->enqueue_front_nic_buffer(p) : h->enqueue_back_nic_buffer(p);
    h->enqueue_back_nic_buffer(p);
    return p;
}

void StormFlow::send_pull(PullPkt::pullctr_t ctr) {
    auto pull = new PullPkt(this, dst, src, ctr);
    add_to_event_queue(new PacketQueuingEvent(get_current_time(), pull, dst->queue));
}

void StormFlow::send_ack(Packet::seqno_t ackno, Packet::seqno_t cum_ack) {
    auto pkt = new StormAckPkt(this, dst, src, ackno, cum_ack);
    add_to_event_queue(new PacketQueuingEvent(get_current_time(), pkt, dst->queue));
}

void StormFlow::send_nack(Packet::seqno_t nackno, bool pull) {
    auto pkt = new StormNackPkt(this, dst, src, nackno, pull);
    add_to_event_queue(new PacketQueuingEvent(get_current_time(), pkt, dst->queue));
}

void StormFlow::receive(Packet *pkt) {
    if (finished) {
        if (pkt->type == STORM_PACKET) {
            ++num_bad_retransmits_received;
        }
        delete pkt;
        return;
    }
    auto sender = static_cast<StormHost*>(src);
    auto recv = static_cast<StormHost*>(dst);
    if (!recv->has_active_flow(this)) {
        recv->add_active_flows(this);
    }
    switch(pkt->type) {
        
        case NOT: {
            auto notify = static_cast<StormNotifyPkt*>(pkt);
            receive_notification(*notify);
            break;
        }
        case REQ: {
            auto req =  static_cast<StormRequestPkt*>(pkt);
            receive_request(*req);
            break;
        }
        case GRANT: {
            auto grant = static_cast<StormGrantPkt*>(pkt);
            receive_grant(*grant);
            break;
        }
        case ACCEPT: {
            auto accept = static_cast<StormAcceptPkt*>(pkt);
            receive_accept(*accept);
            break;
        }
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
                _sender_pullno++;
                //Check rtx queue
                if (!_rtx_list.empty()) {
                    ++num_retransmits;
                    send(*(_rtx_list.begin()), true);
                    _rtx_list.erase(_rtx_list.begin());
                }
                else {
                    if (_seqno + Packet::mss <= size) {
                        send(_seqno, false);
                        _seqno += Packet::mss;
                    }
                }
            }
            break;    
        }
        case STORM_PACKET: {
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
                    _cum_ackno = data_pkt->_seqno + Packet::mss ; //Assumption!! All packets are full sized. Flow size is hence some multiple of mss.
                    //Any other packets which now can be cum_acked?
                    for (auto it = _received.begin(); it != _received.end() && *it == _cum_ackno;) {
                        _cum_ackno += Packet::mss;
                        it = _received.erase(it);
                    }

                    //Update some global variables used for logging utilization
                    if (num_outstanding_packets >= 1) { //Again continuing with the assumption that all packets are full sized.
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

void StormFlow::start_flow() {
    if (size_in_pkt <= 49) {//Flows <= 1BDP get to transmit initial window with no matching
        send_intial_window();
        return;
    }
    static_cast<StormHost*>(src)->enqueue_pending_flow(static_cast<StormHost*>(dst), this);
}

void StormFlow::send_intial_window() {
    while ( (_seqno + Packet::mss <= size) && (_seqno + Packet::mss <= 49 * Packet::mss) ) {
        //std::cout << "Made packet with seqno " << _seqno << " at" << get_current_time() << '\n';
        send(_seqno, false);
        _seqno += Packet::mss;
    }
    //TODO: set RTX timer
}


inline void StormFlow::receive_notification(StormNotifyPkt &notify) {
    static_cast<StormHost*>(dst)->receive_notify(notify);
}

inline void StormFlow::receive_request(StormRequestPkt &request) {
    static_cast<StormHost*>(src)->receive_request(request);
}

inline void StormFlow::receive_grant(StormGrantPkt &grant) {
    static_cast<StormHost*>(dst)->receive_grant(grant);
}

inline void StormFlow::receive_accept(StormAcceptPkt &accept) {
    static_cast<StormHost*>(src)->receive_accept(accept);
}

void StormFlow::send_notification(int epoch) {
    //Notifications go from src to dst
    auto notify = new StormNotifyPkt(this, src, dst, epoch);
    add_to_event_queue(new PacketQueuingEvent(get_current_time(), notify, src->queue));
}

void StormFlow::send_request(int epoch) {
    //Request comes from dst to src
    auto req = new StormRequestPkt(this, dst, src, epoch);
    add_to_event_queue(new PacketQueuingEvent(get_current_time(), req, dst->queue));
}

void StormFlow::send_grant(int epoch) {
    //Grant comes from src to dst again
    auto grant = new StormGrantPkt(this, src, dst, epoch);
    add_to_event_queue(new PacketQueuingEvent(get_current_time(), grant, src->queue));
}

void StormFlow::send_accept(int epoch) {
    //Accepts come from dst to src again
    auto accept = new StormAcceptPkt(this, dst, src, epoch);
    add_to_event_queue(new PacketQueuingEvent(get_current_time(), accept, dst->queue));
}


void StormFlow::update_rtx_timer() {
    if (_timer != NULL) {
        _timer->cancelled = true;
    }
    _timer = new RtxTimerEvent(get_current_time() + RTO, this);
    add_to_event_queue(_timer);
}


void RtxTimerEvent::process_event() {
    _flow->_timer = NULL;
    //Find all packets which need RTX, enqueue at front of NIC
    for (auto it = _flow->_sent_times.begin(); it != _flow->_sent_times.end();) {
        auto [seqno, sent_time] = *it;
        //std::cout << "Possible timeout " << get_current_time() - sent_time << '\n';
        if (get_current_time() - sent_time >= StormFlow::RTO) {
            //Need RTX
            //std::cout << "Timeout! Starting resend.\n";
            ++_flow->num_retransmits;
            _flow->send(seqno, false);
            it = _flow->_sent_times.erase(it);
        }
        else {
            ++it;
        }

    }
}

