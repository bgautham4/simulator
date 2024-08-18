#ifndef STORM_HOST_H
#define STORM_HOST_H

#include "factory.h"
#include "../coresim/node.h"
#include "../coresim/event.h"
#include "stormflow.h"
#include <cstdint>
#include <deque>
#include <queue>
#include <unordered_map>
#include <utility>
#include <vector>
#include <cassert>

constexpr double cRTT =  5.216e-6;
constexpr double beta = 1.3;


class StormHost;
class QueueToNIC;


class PullOrderingComparator {
    public:
    bool operator() (const StormFlow *a, const StormFlow *b) {
        return (a->size_in_pkt - a->num_packets_received) < (b->size_in_pkt - b->num_packets_received);
    }
};

class NICProcessingEvent;
class PacerScheduleEvent;
class StormHost : public Host {
    friend class NotPhaseEvent;
    friend class ReqPhaseEvent;
    friend class GrantPhaseEvent;
    friend class AcceptPhaseEvent;
    friend class NICProcessingEvent;
    friend class PacerScheduleEvent;
    public:
        StormHost(uint32_t id, double rate, uint32_t queue_type) : Host(id, rate, queue_type,NORMAL_HOST) {
            //start_clock(1.0);
        }
        void enqueue_pending_flow(StormHost *dst, StormFlow *flow) {
            _dst_to_flow_map[dst].insert(flow);
        }
        void add_active_flows(StormFlow *flow) { _active_flows.insert(flow); }
        bool pacer_free() {return _pacer_evt == NULL;}
        
        void schedule_pull_pacer();

        //Queuing on NIC
        void enqueue_back_nic_buffer(StormPacket *pkt);
        void enqueue_front_nic_buffer(StormPacket *pkt);
        
        //To control matching process
        void start_clock(double time);//Start clock for host, used to sync each phase.
        void send_notifications();
        void send_requests();
        void send_grant();
        void send_accept();

        //
        bool has_active_flow(StormFlow* f) { return _active_flows.find(f) != _active_flows.end(); }
        void receive_notify(StormNotifyPkt &pkt) {
            assert(pkt._epoch <= _epoch);
            //std::cout << "Got NOTIFY from " << pkt.flow << " epoch " << pkt._epoch << " host_epoch " << _epoch << '\n';
            if (pkt._epoch != _epoch) {
                return;
            }
            auto flow = static_cast<StormFlow*>(pkt.flow);
            _notify_list.push_back(flow);
        }
        uint32_t get_cardinality() {
            return _notify_list.size();
        }
        void receive_request(StormRequestPkt &pkt) {
            assert(pkt._epoch <= _epoch);
            //std::cout << "Got REQ from " << pkt.flow << " epoch " << pkt._epoch << " host_epoch " << _epoch << '\n';
            if (pkt._epoch != _epoch) {
                return;
            }
            auto flow = static_cast<StormFlow*>(pkt.flow);
            _req_list.push_back(flow);    
        }
        void receive_grant(StormGrantPkt &pkt) {
            assert(pkt._epoch <= _epoch);
            //std::cout << "Got GRANT from " << pkt.flow << " epoch " << pkt._epoch << " host_epoch " << _epoch << '\n';
            if (pkt._epoch != _epoch) {
                return;
            }
            auto flow = static_cast<StormFlow*>(pkt.flow);
            _grant_list.push_back(flow);
        }
        void receive_accept(StormAcceptPkt &pkt) {
            assert(pkt._epoch <= _epoch);
            //std::cout << "Got ACCEPT from " << pkt.flow << " epoch " << pkt._epoch << " host_epoch " << _epoch << '\n';
            if (pkt._epoch != _epoch) {
                return;
            }
            auto flow = static_cast<StormFlow*>(pkt.flow);
            _accept_list.push_back(flow);
        }
    private:
        int _epoch = 0;
        std::unordered_map<StormHost*, std::set<StormFlow*>> _dst_to_flow_map;
        //used set instead of prio_queue because need priority based ordering, but still need to be able
        //to erase anywhere within container. (Cant do that with queue).
        std::vector<StormFlow*> _notify_list; 
        decltype(_notify_list) _req_list;
        decltype(_notify_list) _grant_list;
        decltype(_notify_list) _accept_list;
        StormFlow* _matched_target = NULL;
        std::set<StormFlow*, PullOrderingComparator> _active_flows;
        PacerScheduleEvent *_pacer_evt = NULL;
        //To manage and schedule packets onto NIC
        void schedule_nic_transfer();
        NICProcessingEvent *_nic_busy_evt = NULL;
        std::deque<StormPacket*> _nic_queue; //To buffer packets till they get access to NIC.

        static double alpha;
};

class NICProcessingEvent : public Event {
    public:
    NICProcessingEvent(double time, StormHost *host) : Event(QUEUE_PROCESSING, time), _host(host)
    {}
    virtual void process_event();
    private:
    StormHost *_host;
};

class PacerScheduleEvent : public Event {// To unlock the pacer:
    public:
        PacerScheduleEvent(double time, StormHost *host) : Event(QUEUE_PROCESSING, time), _host(host)
        {}
        virtual void process_event();
    private:
        StormHost *_host;
};


#define NOT_EVENT 31
#define REQ_EVENT 32
#define GRANT_EVENT 33
#define ACCEPT_EVENT 34

class NotPhaseEvent : public Event {
    public:
        NotPhaseEvent(double time, StormHost *host) : Event(NOT_EVENT, time), _host(host)
        {}
        virtual void process_event();
    private:
        StormHost *_host;

};

class ReqPhaseEvent : public Event { //Should also start flow transmission here..
    public:
        ReqPhaseEvent(double time, StormHost *host) : Event(REQ_EVENT,time),_host(host) 
        {}
        virtual void process_event();
    private:
        StormHost *_host;
};

class GrantPhaseEvent : public Event {
    public:
        GrantPhaseEvent(double time, StormHost *host) : Event(GRANT_EVENT, time), _host(host)
        {}
        virtual void process_event();
    private:
        StormHost *_host;
};

class AcceptPhaseEvent : public Event {
    public:
        AcceptPhaseEvent(double time, StormHost *host) : Event(ACCEPT_EVENT, time), _host(host)
        {}
        virtual void process_event();
    private:
        StormHost *_host;
};

template<typename K, typename T>
std::vector<K> get_keys(std::unordered_map<K,T>& dict) {
    std::vector<K> keys;
    for (auto &[k,v] : dict) {
        keys.push_back(k);
    }
    return keys;
}
#endif