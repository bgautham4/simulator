#include "ft_topology.h"
#include "../run/params.h"

extern DCExpParams params;

FatTreeTopology::FatTreeTopology(
        uint32_t k, 
        double bandwidth,
        uint32_t queue_type
        ) : Topology () {
	this->_k = k;
    this->num_hosts = k * k *k / 4;
    this->num_edge_switches = k * k / 2;
    this->num_agg_switches = k * k / 2;
    this->num_core_switches = k * k / 4;

    //Capacities
    double c = bandwidth;

    // Create Hosts
    for (uint32_t i = 0; i < this->num_hosts; i++) {
        hosts.push_back(Factory::get_host(i, c, queue_type, params.host_type)); 
    }
    // Create Switches
    for (uint32_t i = 0; i < num_edge_switches; i++) {
        FatTreeSwitch* sw = new FatTreeSwitch(i, k, c, queue_type, FAT_TREE_EDGE_SWITCH);
        if (i == 0) {
        	sw->queue_to_arbiter = Factory::get_queue(num_edge_switches + 
        		num_agg_switches + num_core_switches, c, params.queue_size, queue_type, 0, 2);
        }
        edge_switches.push_back(sw); // TODO make generic
        switches.push_back(sw);
    }
    for (uint32_t i = 0; i < num_agg_switches; i++) {
        FatTreeSwitch* sw = new FatTreeSwitch(i + num_edge_switches, k, c, queue_type, FAT_TREE_AGG_SWITCH);
        agg_switches.push_back(sw); // TODO make generic
        switches.push_back(sw);
    }
    for (uint32_t i = 0; i < num_core_switches; i++) {
        FatTreeSwitch* sw = new FatTreeSwitch(i + 
        	num_edge_switches + num_agg_switches, k, c, queue_type, FAT_TREE_CORE_SWITCH);
        core_switches.push_back(sw);
        switches.push_back(sw);
    }
    //Connect host queues
    for (uint32_t i = 0; i < num_hosts; i++) {
        hosts[i]->queue->set_src_dst(hosts[i], edge_switches[2 * i / k]);
        // std::cout << "Linking Host " << i << " to Edge " << edge_switches[2 * i / k]->id << "\n";
    }

    // For edge switches -- REMAINING
    for (uint32_t i = 0; i < num_edge_switches; i++) {
    	uint32_t pod = i / (k / 2);
    	// Queues to Hosts
    	for (uint32_t j = 0; j < k / 2; j++) {
    		Queue *q = edge_switches[i]->queues[j];
        	q->set_src_dst(edge_switches[i], hosts[i * k / 2 + j]);
    	}
    	// Queues to Agg
    	for (uint32_t j = 0; j < k / 2; j++) {
            Queue *q = edge_switches[i]->queues[j + k / 2];
            q->set_src_dst(edge_switches[i], agg_switches[ pod * k / 2 + j]);
            // std::cout << "Linking Edge  " << edge_switches[i]->id << " to Agg " << agg_switches[ pod * k / 2 + j]->id << "\n";
        }


    }

    // For agg switches
    for (uint32_t i = 0; i < num_agg_switches; i++) {
        // Queues to Edge Switches
    	uint32_t pod = i / (k / 2);
    	uint32_t base = i % (k / 2) * (k / 2);
        for (uint32_t j = 0; j < k / 2; j++) { // TODO make generic
            Queue *q = agg_switches[i]->queues[j];
            q->set_src_dst(agg_switches[i], edge_switches[pod * k / 2 + j]);
            // std::cout << "Linking Agg " << agg_switches[i]->id << " to Edge" << edge_switches[pod * k / 2 + j]->id << "\n";
        }
        // Queues to Core
        for (uint32_t j = 0; j < k / 2; j++) {
            Queue *q = agg_switches[i]->queues[j + k / 2];
            q->set_src_dst(agg_switches[i], core_switches[j + base]);
            // std::cout << "Linking Agg " << agg_switches[i]->id << " to Core" << core_switches[j + base]->id << "\n";
        }
    }

    //For core switches -- PERFECT
    for (uint32_t i = 0; i < num_core_switches; i++) {
    	uint32_t base = i / (k / 2);
        for (uint32_t j = 0; j < k; j++) {
            Queue *q = core_switches[i]->queues[j];
            q->set_src_dst(core_switches[i], agg_switches[k / 2 * j + base]);
            // std::cout << "Linking Core " << core_switches[i]->id << " to Agg" << agg_switches[k / 2 * j + base]->id << "\n";
        }
    }
    auto rtt = (6 * params.propagation_delay + (1500 * 8 / params.bandwidth) * 6) * 2;
    params.BDP = ceil(rtt * params.bandwidth / 1500 / 8);
}

bool FatTreeTopology::is_same_rack(int a, int b) {
    // assume arbiter is at pod 0 and edge switch 0;
    if(a / (_k / 2) == b / (_k / 2))
        return true;
    return false;
}

bool FatTreeTopology::is_same_rack(Host* a, Host*b) {
        return is_same_rack(a->id, b->id);
}

uint32_t FatTreeTopology::get_rack_num(Host* a) {
	    return a->id / (_k / 2);
}

bool FatTreeTopology::is_same_pod(Host* a, Host*b) {
	if(a->id / (_k * _k / 4) == b->id / (_k * _k / 4))
		return true;
	return false;
}

uint32_t FatTreeTopology::get_pod_num(Host* a) {
    return a->id / (_k * _k / 4);
}

Queue *FatTreeTopology::get_next_hop(Packet *p, Queue *q) {
    if (q->dst->type == HOST) {
        return NULL; // Packet Arrival
    }
    // At host level
    if(q->src->type == HOST) {
    	return get_host_next_hop(p, q);
    }
    // At switch level
    else if (q->src->type == SWITCH) {
	    if (((Switch *)q->src)->switch_type == FAT_TREE_EDGE_SWITCH) {
	    	return get_edge_next_hop(p, q);
	    }
	    else if (((Switch *)q->src)->switch_type == FAT_TREE_AGG_SWITCH) {
	    	return get_agg_next_hop(p, q);
	    } else if(((Switch *)q->src)->switch_type == FAT_TREE_CORE_SWITCH) {
	    	return get_core_next_hop(p,q);
	    }
    }
    assert(false);
}

Queue *FatTreeTopology::get_host_next_hop(Packet *p, Queue *q) {
    assert(q->src->type == HOST); 
    assert (p->src->id == q->src->id);
	// Same Rack or not
    if (is_same_rack(p->src, p->dst)) {
    	    return ((Switch *) q->dst)->queues[p->dst->id % (_k / 2)];
    }
    else {
        uint32_t hash_port = 0;
        if(params.load_balancing == 0)
            hash_port = q->spray_counter++% (_k / 2);
        else if(params.load_balancing == 1)
            hash_port = (p->src->id + p->dst->id + p->flow->id) % (_k / 2);
        return ((Switch *) q->dst)->queues[_k / 2 + hash_port];
    }
}

Queue* FatTreeTopology::get_edge_next_hop(Packet *p, Queue *q) {
	assert(((Switch *)q->src)->switch_type == FAT_TREE_EDGE_SWITCH);
	assert(((Switch *)q->dst)->switch_type == FAT_TREE_AGG_SWITCH);

	if(is_same_pod(p->src, p->dst)) {
		uint32_t edge_num = get_rack_num(p->dst) % (_k / 2);
		return ((Switch *) q->dst)->queues[edge_num];
	} else {
        uint32_t hash_port = 0;
        if(params.load_balancing == 0)
            hash_port = q->spray_counter++% (_k / 2);
        else if(params.load_balancing == 1)
            hash_port = (p->src->id + p->dst->id + p->flow->id) % (_k / 2);
        return ((Switch *) q->dst)->queues[_k / 2 + hash_port];
	}
}

Queue* FatTreeTopology::get_agg_next_hop(Packet *p, Queue *q) {
	assert(((Switch *)q->src)->switch_type == FAT_TREE_AGG_SWITCH);
    if(((Switch *)q->dst)->switch_type == FAT_TREE_EDGE_SWITCH) {
    	uint32_t queue_num = p->dst->id % (_k / 2);
    	return ((Switch *) q->dst)->queues[queue_num];
    } else {
    	assert(((Switch *)q->dst)->switch_type == FAT_TREE_CORE_SWITCH);
      	uint32_t pod_num = get_pod_num(p->dst);
	    return ((Switch *) q->dst)->queues[pod_num];
    }
    assert(false);
    return NULL;
}

Queue* FatTreeTopology::get_core_next_hop(Packet *p, Queue *q) {
	assert(((Switch *)q->src)->switch_type == FAT_TREE_CORE_SWITCH);
	assert(((Switch *)q->dst)->switch_type == FAT_TREE_AGG_SWITCH);
	uint32_t edge_num = get_rack_num(p->dst) % (_k / 2);
	return ((Switch *) q->dst)->queues[edge_num];
}

double FatTreeTopology::get_oracle_fct(Flow *f) {
    int num_hops = 6;
    if (is_same_rack(f->src, f->dst)) {
        num_hops = 2;
    } else if(is_same_pod(f->src, f->dst)) {
    	num_hops = 4;
    }
    double propagation_delay;
    if (params.ddc != 0) { 
        if (num_hops == 2) {
            propagation_delay = 0.440;
        }
        if (num_hops == 4) {
            propagation_delay = 2.040;
        }
    }
    else {
        propagation_delay = 2 * 1000000.0 * num_hops * f->src->queue->propagation_delay; //us
    }
   
    double pkts = (double) f->size / params.mss;
    uint32_t np = floor(pkts);
    uint32_t leftover = (pkts - np) * params.mss;
	double incl_overhead_bytes = (params.mss + f->hdr_size) * np + leftover;
    if(leftover > 0) {
        incl_overhead_bytes += f->hdr_size;
    }

    double bandwidth = f->src->queue->rate / 1000000.0; // For us
    double transmission_delay;
    if (params.cut_through) {
    	assert(false);
        //std::cout << "pd: " << propagation_delay << " td: " << transmission_delay << std::endl;
    }
    else {
		transmission_delay = (incl_overhead_bytes + f->hdr_size) * 8.0 / bandwidth;
		// last packet and 1 ack
		if (leftover != params.mss && leftover != 0) {
			// less than mss sized flow. the 1 packet is leftover sized.
			transmission_delay += (num_hops - 1) * (leftover + 2 * params.hdr_size) * 8.0 / (bandwidth);
			
		} else {
			// 1 packet is full sized
			transmission_delay += (num_hops - 1) * (params.mss + 2 * params.hdr_size) * 8.0 / (bandwidth);
		}
    }
    return (propagation_delay + transmission_delay); //us
}

double FatTreeTopology::get_control_pkt_rtt(int host_id) {
    if(host_id / (_k /2) == 0) {
        return (2 * params.propagation_delay + 2 * (params.hdr_size * 8 / params.bandwidth)) * 2;
    } else if(host_id / (_k * _k / 4) == 0) {
        return (4 * params.propagation_delay + 4 * (params.hdr_size * 8 / params.bandwidth)) * 2;
    } else {
    	return (6 * params.propagation_delay + 6 * (params.hdr_size * 8 / params.bandwidth)) * 2;
    }
}

int FatTreeTopology::num_hosts_per_tor() {
    return int(_k * _k / 4);
}
