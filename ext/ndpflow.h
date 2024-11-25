#ifndef NDP_FLOW_H
#define NDP_FLOW_H

#include "../coresim/packet.h"
#include "../coresim/flow.h"
#include "../coresim/event.h"
#include <cstdint>
#include <list>
#include <map>
#include <set>

class NDPRtxTimerEvent;

class NDPFlow : public Flow {
    friend class NDPHost;
    friend class NDPRtxTimerEvent;
    friend class PullScheduleEvent;
    public:
        NDPFlow(uint32_t id, double start_time, uint32_t size, Host *src, Host *dst) : 
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
        bool operator< (const NDPFlow &that) const {
            if (size == that.size) {
                return start_time < that.start_time;
            }
            return size < that.size;
        }
        
        static constexpr double RTO = 5e-4; //Rto value

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
        NDPRtxTimerEvent *_timer = NULL;
        bool _is_finished_at_receiver = false;
};

#define RTX_TIMEOUT 30
class NDPRtxTimerEvent : public Event {
    public:
        NDPRtxTimerEvent(double time, NDPFlow *flow) : Event(RTX_TIMEOUT, time), _flow(flow) {};
        virtual void process_event();
    private:
        NDPFlow *_flow;
};
#endif
