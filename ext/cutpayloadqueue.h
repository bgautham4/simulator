#ifndef CP_QUEUE_H
#define CP_QUEUE_H
#include "../coresim/queue.h"
#include "../coresim/packet.h"

#include <cstdint>
#include <deque>
#include <cstddef>
#include <array>

class CutPayloadQueue : public Queue {
    public:
        CutPayloadQueue(uint32_t id, double rate, int limit_bytes, int location) : Queue(id,rate,limit_bytes,location)
        {};
        virtual void enque(Packet *packet);
        virtual Packet *deque();
    private:
        static constexpr size_t NUM_PRIO_LEVELS = 2;
        /*Queue serves priority levels HI and LOW with a
        ratio of 10:1
        */
        static constexpr int LOW_RATIO = 1;
        static constexpr int HI_RATIO = 10;
        typedef enum PrioLevels: int {LOW = 0,HI = 1} PrioLevels;
        PrioLevels serv = HI; //Current priority level beign serviced.
        int counter = 0; //Counter to track the weighted service
        //Priority queues
        std::array<std::deque<Packet*>, NUM_PRIO_LEVELS> prio_queues = {};
        //Bytes in each queue
        std::array<uint32_t, NUM_PRIO_LEVELS> bytes_in_queues = {0,0};
};
#endif
