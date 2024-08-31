#ifndef NDP_HOST_H
#define NDP_HOST_H

#include "factory.h"
#include "../coresim/node.h"
#include "../coresim/event.h"
#include "ndpflow.h"
#include <cstdint>
#include <deque>
#include <queue>
#include <unordered_map>
#include <utility>
#include <vector>
#include <cassert>
#include <algorithm>

class NDPHost;

class NICScheduleEvent;
class PullScheduleEvent;
class NDPHost : public Host {

    friend class NICScheduleEvent;
    friend class PullScheduleEvent;
    public:
        NDPHost(uint32_t id, double rate, uint32_t queue_type) : Host(id, rate, queue_type,NORMAL_HOST) {
        }

        void add_active_flows(NDPFlow *flow) { _active_flows.push_back(flow); }
        bool pacer_free() {return _pacer_evt == NULL;}
        
        void schedule_pull_pacer();

        //Queuing on NIC
        void enqueue_back_nic_buffer(StormPacket *pkt);
        void enqueue_front_nic_buffer(StormPacket *pkt);
        
        //
        bool has_active_flow(NDPFlow* f) { return std::find(_active_flows.begin(), _active_flows.end(), f) != _active_flows.end(); }
       
    private:
        int _epoch = 0;
        std::unordered_map<NDPHost*, std::set<NDPFlow*>> _dst_to_flow_map;
        std::vector<NDPFlow*> _active_flows;
        PullScheduleEvent *_pacer_evt = NULL;
        //To manage and schedule packets onto NIC
        void schedule_nic_transfer();
        NICScheduleEvent *_nic_busy_evt = NULL;
        std::deque<StormPacket*> _nic_queue; //To buffer packets till they get access to NIC.
};

class NICScheduleEvent : public Event {
    public:
    NICScheduleEvent(double time, NDPHost *host) : Event(QUEUE_PROCESSING, time), _host(host)
    {}
    virtual void process_event();
    private:
    NDPHost *_host;
};

class PullScheduleEvent : public Event {// To unlock the pacer:
    public:
        PullScheduleEvent(double time, NDPHost *host) : Event(QUEUE_PROCESSING, time), _host(host)
        {}
        virtual void process_event();
    private:
        NDPHost *_host;
};

#endif