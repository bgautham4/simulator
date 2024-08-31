#include <cassert>
#include "ndphost.h"
#include "../coresim/event.h"
#include "../run/params.h"

extern double get_current_time();
extern void add_to_event_queue(Event *ev);

void NDPHost::schedule_nic_transfer() {
    assert(_nic_busy_evt == NULL);
    auto qpe = (queue->busy) ? queue->queue_proc_event->time : get_current_time() ;
    auto td = queue->get_transmission_delay(queue->bytes_in_queue);
    _nic_busy_evt = new NICScheduleEvent(qpe + td + INFINITESIMAL_TIME, this);
    add_to_event_queue(_nic_busy_evt);
    return;
}

void NICScheduleEvent::process_event() {
    _host->_nic_busy_evt = NULL;
    if (_host->queue->busy) { //Host's NIC is busy because of some other packet?
        _host->schedule_nic_transfer();
        return;
    }
    assert(!_host->_nic_queue.empty());
    auto pkt = _host->_nic_queue.front();
    auto flow = static_cast<NDPFlow*>(pkt->flow);
    flow->mark_sent_time(pkt->seqno(),get_current_time());
    flow->update_rtx_timer();
    add_to_event_queue(new PacketQueuingEvent(get_current_time(), pkt, _host->queue));
    _host->_nic_queue.pop_front();
    if (_host->_nic_queue.empty()) {
        return;
    }
    //Schdeule transfer for next packet
    _host->_nic_busy_evt = new NICScheduleEvent(get_current_time() + _host->queue->get_transmission_delay(pkt->size) + INFINITESIMAL_TIME, _host);
    add_to_event_queue(_host->_nic_busy_evt);
}

void NDPHost::enqueue_back_nic_buffer(StormPacket *pkt) {
    _nic_queue.push_back(pkt);
    if (_nic_busy_evt == NULL) {
        schedule_nic_transfer();
    }
}

void NDPHost::enqueue_front_nic_buffer(StormPacket *pkt) {
     _nic_queue.push_front(pkt);
    if (_nic_busy_evt == NULL) {
        schedule_nic_transfer();
    }
}

void NDPHost::schedule_pull_pacer() {
    assert(_pacer_evt == NULL);
    _pacer_evt = new PullScheduleEvent(get_current_time() + queue->get_transmission_delay(1500) + INFINITESIMAL_TIME, this);
    add_to_event_queue(_pacer_evt);
}

void PullScheduleEvent::process_event() {
    _host->_pacer_evt = NULL;
    using T = decltype(_host->_active_flows)::value_type;
    std::sort(_host->_active_flows.begin(), _host->_active_flows.end(), [](const T &a, const T &b) {
        return *a < *b;
    });
    for (auto it = _host->_active_flows.begin(); it != _host->_active_flows.end();) {//Move through active flows in order of 
    //priority. Check if they have pulls..
        auto flow = *it;
        auto &pull_queue = flow->_pull_queue;
        if (!pull_queue.empty()) {
            flow->send_pull(pull_queue.front());
            pull_queue.pop_front();
            _host->schedule_pull_pacer();
            break;
        }
        //Ending up here would mean top flow is either finished/received last packet
        // or has no pulls at the moment.
        //If finished, then remove from active flows and move to next flow. Else simply move to next flow.
        if (flow->_is_finished_at_receiver) { 
            it = _host->_active_flows.erase(it);
        }
        else {
            ++it;
        }
    }
    //Didnt have any pulls. Pacer is free 
}

