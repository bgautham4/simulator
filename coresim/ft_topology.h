#ifndef FATREETOPO_H
#define FATREETOPO_H
#include "topology.h"

class FatTreeTopology : public Topology {
    public:
        FatTreeTopology(
        		uint32_t k,
	        	double bandwidth,
        		uint32_t queue_type
                );
		bool is_same_rack(Host* a, Host*b);
		uint32_t get_rack_num(Host* a);
		bool is_same_pod(Host* a, Host*b);
		uint32_t get_pod_num(Host* a);
		Queue* get_host_next_hop(Packet *p, Queue *q);
		Queue* get_edge_next_hop(Packet *p, Queue *q);
		Queue* get_agg_next_hop(Packet *p, Queue *q);
		Queue* get_core_next_hop(Packet *p, Queue *q);
        void set_up_parameter();

        virtual Queue* get_next_hop(Packet *p, Queue *q);
        virtual double get_control_pkt_rtt(int host_id);
        virtual double get_oracle_fct(Flow* f);
        virtual bool is_same_rack(int a, int b);
        virtual int num_hosts_per_tor();

        uint32_t num_edge_switches;
        uint32_t num_agg_switches;
        uint32_t num_core_switches;
        uint32_t _k;
        std::vector<FatTreeSwitch*> edge_switches;
        std::vector<FatTreeSwitch*> agg_switches;
        std::vector<FatTreeSwitch*> core_switches;
};

#endif
