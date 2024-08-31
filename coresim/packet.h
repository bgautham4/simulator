#ifndef PACKET_H
#define PACKET_H

#include "flow.h"
#include "node.h"
#include <cstdint>
#include <stdint.h>
// TODO: Change to Enum
#define NORMAL_PACKET 0
#define ACK_PACKET 1

#define RTS_PACKET 3
#define CTS_PACKET 4
#define OFFER_PACKET 5
#define DECISION_PACKET 6
#define CAPABILITY_PACKET 7
#define STATUS_PACKET 8
#define FASTPASS_RTS 9
#define FASTPASS_SCHEDULE 10

//New Packets for NDP + STORM.
#define NOT 12
#define REQ 13
#define GRANT 14
#define ACCEPT 15
#define PULL 16
#define STORM_ACK 17
#define STORM_NACK 18
#define STORM_PACKET 19

class FastpassEpochSchedule;

class Packet {
    public:
        using seqno_t = uint32_t;
        using bytes_t = uint32_t;
        Packet(double sending_time, Flow *flow, uint32_t seq_no, uint32_t pf_priority,
                uint32_t size, Host *src, Host *dst);
        double sending_time;
        Flow *flow;
        seqno_t seq_no;
        uint32_t pf_priority;
        bytes_t size;
        Host *src;
        Host *dst;
        uint32_t unique_id;
        static uint32_t instance_count;
        int remaining_pkts_in_batch;
        int capability_seq_num_in_data;

        uint32_t type; // Normal or Ack packet
        double total_queuing_delay;
        double last_enque_time;

        int capa_data_seq;
        bool is_header = false;
        virtual void strip_payload() {
            is_header = true;
            size = 40;
        }
        static constexpr bytes_t mss = 1460;
};

class PlainAck : public Packet {
    public:
        PlainAck(Flow *flow, uint32_t seq_no_acked, uint32_t size, Host* src, Host* dst);
};

class Ack : public Packet {
    public:
        Ack(Flow *flow, uint32_t seq_no_acked, std::vector<uint32_t> sack_list,
                uint32_t size,
                Host* src, Host *dst);
        uint32_t sack_bytes;
        std::vector<uint32_t> sack_list;
};

class RTSCTS : public Packet {
    public:
        //type: true if RTS, false if CTS
        RTSCTS(bool type, double sending_time, Flow *f, uint32_t size, Host *src, Host *dst);
};

class RTS : public Packet{
    public:
        RTS(Flow *flow, Host *src, Host *dst, double delay, int iter);
        double delay;
        int iter;
};

class OfferPkt : public Packet{
    public:
        OfferPkt(Flow *flow, Host *src, Host *dst, bool is_free, int iter);
        bool is_free;
        int iter;
};

class DecisionPkt : public Packet{
    public:
        DecisionPkt(Flow *flow, Host *src, Host *dst, bool accept);
        bool accept;
};

class CTS : public Packet{
    public:
        CTS(Flow *flow, Host *src, Host *dst);
};

class CapabilityPkt : public Packet{
    public:
        CapabilityPkt(Flow *flow, Host *src, Host *dst, double ttl, int remaining, int cap_seq_num, int data_seq_num);
        double ttl;
        int remaining_sz;
        int cap_seq_num;
        int data_seq_num;
};

class StatusPkt : public Packet{
    public:
        StatusPkt(Flow *flow, Host *src, Host *dst, int num_flows_at_sender);
        double ttl;
        bool num_flows_at_sender;
};


class FastpassRTS : public Packet
{
    public:
        FastpassRTS(Flow *flow, Host *src, Host *dst, int remaining_pkt);
        int remaining_num_pkts;
};

class FastpassSchedulePkt : public Packet
{
    public:
        FastpassSchedulePkt(Flow *flow, Host *src, Host *dst, FastpassEpochSchedule* schd);
        FastpassEpochSchedule* schedule;
};


class StormNotifyPkt : public Packet {
    friend class StormFlow;
    friend class StormHost;
    public:
        StormNotifyPkt(Flow *flow, Host *src, Host *dst, int epoch) : Packet(0,flow,0,0,40,src,dst),_epoch(epoch) {
            type = NOT;
            is_header = true;
        };
    private:
        int _epoch;
};

class StormRequestPkt : public Packet {
    friend class StormFlow;
    friend class StormHost;
    public:
        StormRequestPkt(Flow *flow, Host *src, Host *dst, int epoch) : Packet(0,flow,0,0,40,src,dst),_epoch(epoch) {
            type = REQ;
            is_header = true;
        };
    private:
        int _epoch;
};

class StormGrantPkt : public Packet {
    friend class StormFlow;
    friend class StormHost;
    public:
        StormGrantPkt(Flow *flow, Host *src, Host *dst, int epoch) : Packet(0,flow,0,0,40,src,dst), _epoch(epoch) {
            type = GRANT;
            is_header = true;
        };
    private:
        int _epoch;
};

class StormAcceptPkt : public Packet {
    friend class StormFlow;
    friend class StormHost;
    public:
        StormAcceptPkt(Flow *flow, Host *src, Host *dst, int epoch) : Packet(0,flow,0,0,40,src,dst), _epoch(epoch){
            type = ACCEPT;
            is_header = true;
        };
    private:
        int _epoch;
};


class PullPkt : public Packet {
    friend class StormFlow;
    friend class StormHost;
    //Reuse by NDP
    friend class NDPHost;
    friend class NDPFlow;
    public:
        using pullctr_t = uint32_t;
        PullPkt(Flow *flow, Host *src, Host *dst, pullctr_t ctr) : Packet(0,flow, 0,0,40,src,dst),_ctr(ctr) {
            type = PULL;
            is_header = true;
        };
    private:
        pullctr_t _ctr;
};

class StormAckPkt : public Packet {
    friend class StormFlow;
    friend class StormHost;
    //Reuse by NDP
    friend class NDPHost;
    friend class NDPFlow;
    public:
        StormAckPkt(Flow *flow, Host *src, Host *dst, seqno_t ackno, seqno_t cum_ack) : Packet(0,flow,0,0,40,src,dst), _ackno(ackno), _cum_ackno(cum_ack){
            type = STORM_ACK;
        }
    private:
        seqno_t _ackno;
        seqno_t _cum_ackno;
};

class StormNackPkt : public Packet {
    friend class StormFlow;
    friend class StormHost;
    //Reuse by NDP
    friend class NDPHost;
    friend class NDPFlow;
    public:
        StormNackPkt(Flow *flow, Host *src, Host *dst, seqno_t nackno, bool pull) : Packet(0,flow,0,0,40,src,dst), _nackno(nackno), _pull(pull)
        {
            type = STORM_NACK;
        }
    private:
        seqno_t _nackno;
        bool _pull; //Optimization for fast RTX
};

class StormPacket : public Packet {
    friend class StormFlow;
    friend class StormHost;
    //Reuse by NDP
    friend class NDPHost;
    friend class NDPFlow;
    public:
        StormPacket(Flow *flow, Host *src, Host *dst, seqno_t seqno, bytes_t size, bool last_packet) : Packet(0,flow,0,0,size,src,dst), _seqno(seqno),_last_packet(last_packet){
            type = STORM_PACKET;
        }
        seqno_t seqno() {return _seqno;}
    private:
        seqno_t _seqno;
        bool _last_packet;
};

#endif

