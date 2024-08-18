#include <algorithm>
#include <cassert>
#include <random>
#include "stormhost.h"
#include "../coresim/event.h"
#include "../run/params.h"
std::mt19937 PRNG(0);

double StormHost::alpha = -4.3;

extern double get_current_time();
extern void add_to_event_queue(Event *ev);



void StormHost::schedule_nic_transfer() {
    assert(_nic_busy_evt == NULL);
    auto qpe = (queue->busy) ? queue->queue_proc_event->time : get_current_time() ;
    auto td = queue->get_transmission_delay(queue->bytes_in_queue);
    _nic_busy_evt = new NICProcessingEvent(qpe + td + INFINITESIMAL_TIME, this);
    add_to_event_queue(_nic_busy_evt);
    return;
}

void NICProcessingEvent::process_event() {
    _host->_nic_busy_evt = NULL;
    if (_host->queue->busy) { //Host's NIC is busy because of some other packet?
        _host->schedule_nic_transfer();
        return;
    }
    assert(!_host->_nic_queue.empty());
    auto pkt = _host->_nic_queue.front();
    auto flow = static_cast<StormFlow*>(pkt->flow);
    flow->mark_sent_time(pkt->seqno(),get_current_time());
    flow->update_rtx_timer();
    add_to_event_queue(new PacketQueuingEvent(get_current_time(), pkt, _host->queue));
    _host->_nic_queue.pop_front();
    if (_host->_nic_queue.empty()) {
        return;
    }
    //Schdeule transfer for next packet
    _host->_nic_busy_evt = new NICProcessingEvent(get_current_time() + _host->queue->get_transmission_delay(pkt->size) + INFINITESIMAL_TIME, _host);
    add_to_event_queue(_host->_nic_busy_evt);
}

void StormHost::enqueue_back_nic_buffer(StormPacket *pkt) {
    _nic_queue.push_back(pkt);
    if (_nic_busy_evt == NULL) {
        schedule_nic_transfer();
    }
}

void StormHost::enqueue_front_nic_buffer(StormPacket *pkt) {
     _nic_queue.push_front(pkt);
    if (_nic_busy_evt == NULL) {
        schedule_nic_transfer();
    }
}

void StormHost::schedule_pull_pacer() {
    assert(_pacer_evt == NULL);
    _pacer_evt = new PacerScheduleEvent(get_current_time() + queue->get_transmission_delay(1500) + INFINITESIMAL_TIME, this);
    add_to_event_queue(_pacer_evt);
}

void PacerScheduleEvent::process_event() {
    _host->_pacer_evt = NULL;
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

void StormHost::send_notifications() {
    //Remove finished flows:
    for (auto it = _dst_to_flow_map.begin(); it != _dst_to_flow_map.end();) {
        if (it->second.empty()) {
            it = _dst_to_flow_map.erase(it);
        }
        else {
            ++it;
        }
    }
    auto dsts = get_keys(_dst_to_flow_map);
    std::shuffle(dsts.begin(), dsts.end(), PRNG);
    for (size_t i = 0; i < dsts.size() && i<2; ++i) {
        auto &flows = _dst_to_flow_map[dsts[i]];
        auto flow = *(flows.begin());
        flow->send_notification(_epoch);
    }
}

void StormHost::send_requests() {
    for (auto flow : _notify_list) {
        flow->send_request(_epoch);
    }
}

void StormHost::send_grant() {
    if (_req_list.empty()) {
        return;
    }
    //Form a PMF out of the requests
    std::vector<double> weigths(_req_list.size(),0);
    std::transform(_req_list.begin(), _req_list.end(), weigths.begin(),
    [](decltype(_req_list)::value_type &x){
        auto receiver = static_cast<StormHost*>(x->dst);
        auto N = receiver->get_cardinality(); //Get number of notifications received by this receiver
        return std::pow(N, alpha); //Alpha probabilistic weigths
    });

    std::discrete_distribution<size_t> pmf(weigths.begin(), weigths.end());

    //Sample and send
    auto flow = _req_list[pmf(PRNG)];
    flow->send_grant(_epoch);
}

void StormHost::send_accept() {
    if (_grant_list.empty()) {
        return;
    }
    //Sample one grant uniformly and send.
    std::uniform_int_distribution<size_t> udist(0, _grant_list.size() - 1);
    auto flow = _grant_list[udist(PRNG)];
    flow->send_accept(_epoch);
}

//Scheduling phases

void NotPhaseEvent::process_event() {
    _host->_notify_list.clear();
    _host->_req_list.clear();
    _host->_grant_list.clear();

    //Begin transmission:
    assert(_host->_accept_list.size() <= 1);
    _host->_matched_target = (_host->_accept_list.empty()) ? NULL : _host->_accept_list[0];
    _host->_accept_list.clear();
    if (_host->_matched_target == NULL) {
        //Do nothing
    }
    else {
        auto dst = static_cast<StormHost*>(_host->_matched_target->dst);
        auto &pending_flows = _host->_dst_to_flow_map.at(dst); //Will throw if we dont find this dst
        assert(pending_flows.count(_host->_matched_target)); 
        _host->_matched_target->send_intial_window();
        pending_flows.erase(_host->_matched_target);
    }
    //Advance epoch and send notifications
    _host->_epoch++;
    _host->send_notifications();

    //Schedule req phase
    add_to_event_queue(new ReqPhaseEvent(get_current_time() + cRTT * beta / 2, _host));

}
void ReqPhaseEvent::process_event() {
    //Send requests to all hosts and schedule grant phase
    _host->send_requests();
    add_to_event_queue(new GrantPhaseEvent(get_current_time() + cRTT * beta / 2, _host));    
}

void GrantPhaseEvent::process_event() {
    //Send grant
    _host->send_grant();
    //Schedule accept phase
    add_to_event_queue(new AcceptPhaseEvent(get_current_time() + cRTT * beta / 2, _host));
   
}

void AcceptPhaseEvent::process_event() {
    //Got any grants? Send an accept to a grant
    _host->send_accept();
    //Schedule next epoch's notify phase
    add_to_event_queue(new NotPhaseEvent(get_current_time() + cRTT * beta / 2, _host));
}

void StormHost::start_clock(double time) {
    add_to_event_queue(new NotPhaseEvent(time, this));
}
