#ifndef STORM_FLOW_H
#define STORM_FLOW_H

#include "../coresim/packet.h"
#include "../coresim/flow.h"
#include "../coresim/event.h"
#include <cstdint>
#include <list>
#include <map>
#include <set>

class RtxTimerEvent;

class StormFlow : public Flow {
    friend class StormHost;
    friend class RtxTimerEvent;
    friend class PacerScheduleEvent;
    public:
        StormFlow(uint32_t id, double start_time, uint32_t size, Host *src, Host *dst) : 
        Flow(id, start_time, size, src, dst) {
        }
        virtual void start_flow();
        virtual void receive(Packet *pkt);
        Packet::bytes_t send(Packet::seqno_t seqno, bool pulled);
        void send_pending_data();
        void send_intial_window();

        void pull_packets(PullPkt::pullctr_t pullno);

        void send_ack(Packet::seqno_t seqno_acked, Packet::seqno_t cum_ack);
        void send_nack(Packet::seqno_t seqno_nacked, bool pull);
        void send_pull(PullPkt::pullctr_t ctr);

        //receive functions for special packets..
        void receive_notification(StormNotifyPkt &notify);
        void receive_request(StormRequestPkt &request);
        void receive_grant(StormGrantPkt &grant);
        void receive_accept(StormAcceptPkt &accept);

        //send fucntions for special packets
        void send_notification(int epoch);
        void send_request(int epoch);
        void send_grant(int epoch);
        void send_accept(int epoch);

        void mark_sent_time(Packet::seqno_t seqno, double time) {
            _sent_times[seqno] = time;
        }

        void update_rtx_timer();
        //for priority based pull servicing and logging
        uint32_t num_packets_received = 0;
        uint32_t num_trimmed_packets_received = 0;
        uint32_t num_bad_retransmits_received  = 0;
        uint32_t num_retransmits = 0;
        //For flow ordering
        bool operator< (const StormFlow &that) {
            if (size == that.size) {
                return start_time < that.start_time;
            }
            return size < that.size;
        }
        
        static constexpr double RTO = 1e-3; //Rto value

    private:
        std::map<Packet::seqno_t, double> _sent_times; //Record packet seqno to the time it was sent at.
        std::set<Packet::seqno_t> _rtx_list; //Packets awaiting rtx.
        std::set<Packet::seqno_t> _received; // Temp buffer to hold out of order packets, remove once we can cum_ack
        std::deque<PullPkt::pullctr_t> _pull_queue; //Pull queue for flow. Host must order each pull queue based on priority. 

        PullPkt::pullctr_t _sender_pullno = 0;
        PullPkt::pullctr_t _receiver_pullno = 0;

        Packet::seqno_t _seqno = 0;
        Packet::seqno_t _highest_recvd_ackno = 0;
        Packet::seqno_t _cum_ackno = 0; //Number of all received bytes
        Packet::seqno_t _sender_cum_ackno = 0;
        RtxTimerEvent *_timer = NULL;
        bool _is_finished_at_receiver = false;

        bool _dst_informed = false; //Destination has seen atleast single packet from this flow?

};

#define RTX_TIMEOUT 30
class RtxTimerEvent : public Event {
    public:
        RtxTimerEvent(double time, StormFlow *flow) : Event(RTX_TIMEOUT, time), _flow(flow) {};
        virtual void process_event();
    private:
        StormFlow *_flow;
};
#endif