#ifndef ROUTINGTABLE_H
#define ROUTINGTABLE_H

#include "peer.hpp"

#include <memory>
#include <iostream>
#include <utility>
#include <map>
#include <vector>

class RoutingTable
{
public:
    RoutingTable();
    ~RoutingTable();

    //Should be const!
    const std::shared_ptr<const Peer> get(const size_t index) const
    {
        //check if index is in range
        if(m_table.size() > index)
        {
            const std::shared_ptr<const Peer> p = m_table.at(index);
            return p;
        }
        //TODO Implement proper logging
        std::cout << "Peer with index" << index
                  << " not found." << std::endl;
    }

    void append();

    void set();

private:
    std::vector<std::shared_ptr<Peer>> m_table;

};

#endif /* ROUTINGTABLE_H */
