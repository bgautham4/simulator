#ifndef CP_QUEUE_H
#define CP_QUEUE_H
#include "../coresim/queue.h"
#include "../coresim/packet.h"

#include <cstdint>
#include <deque>
#include <cstddef>

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
        static constexpr uint32_t LOW_RATIO = 1;
        static constexpr uint32_t HI_RATIO = 10;
        typedef enum PrioLevels {LOW = 0,HI = 1} PrioLevels;
        PrioLevels serv = HI; //Current priority level beign serviced.
        uint32_t counter = 0; //Counter to track the weighted service
        //Priority queues
        std::deque<Packet*> prio_queues[NUM_PRIO_LEVELS] = {};
        //Bytes in each queue
        uint32_t bytes_in_queues[NUM_PRIO_LEVELS] = {0,0};

};
#endif