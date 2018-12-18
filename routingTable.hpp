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
    RoutingTable(std::shared_ptr<Peer> self,
                 std::shared_ptr<Peer> predecessor,
                 std::shared_ptr<Peer> successor)
        : m_self(self), m_predecessor(predecessor), m_successor(successor),
          m_fingerTable()
    {}

    ~RoutingTable() {}

    //Should be const!
    const std::shared_ptr<const Peer> get(const size_t index) const
    {
        //check if index is in range
        if(m_fingerTable.size() > index)
        {
            const std::shared_ptr<const Peer> p = m_fingerTable.at(index);
            return p;
        }
        //TODO Implement proper logging
    }

    void set_self(std::shared_ptr<Peer> self)
    {
        m_self = self;
    }

    std::shared_ptr<Peer> get_self()
    {
        return m_self;
    }

    void set_predecessor(std::shared_ptr<Peer> predecessor)
    {
        m_predecessor = predecessor;
    }

    std::shared_ptr<Peer> get_predecessor()
    {
        return m_predecessor;
    }

    void set_successor(std::shared_ptr<Peer> successor)
    {
        m_successor = successor;
        auto it = m_fingerTable.begin();
        m_fingerTable.insert(it, successor);
    }

    std::shared_ptr<Peer> get_successor()
    {
        return m_successor;
    }

    std::shared_ptr<std::vector<std::shared_ptr<Peer>>> get_fingerTable()
    {
        return std::make_shared<std::vector<std::shared_ptr<Peer>>>(m_fingerTable);
    }

    //TODO: Pass an ostream to print to something instead of cout
    void print()
    {
        std::cout << "######## FINGERTABLE START ########" << std::endl;
        if(m_self != nullptr){
            std::cout << "Self: \n";
            m_self->print();
        }
        if(m_successor != nullptr){
            std::cout << "\nSuccessor: \n";
             m_successor->print();
        }
        if(m_predecessor != nullptr){
            std::cout << "\nPredecessor: \n";
              m_predecessor->print();
        }
        std::cout << "\nFingertable: \n";
        for(const auto& finger : m_fingerTable){
            finger->print();
            std::cout << '\n';
        }
        std::cout << "######## FINGERTABLE END ########" << std::endl;
    }

    void append();

    void set();

    std::vector<std::shared_ptr<Peer>> m_fingerTable;
private:
    //Fingertable containing multiple peers,
    //used to lookup keys

    //Peer Object holding information about itself
    std::shared_ptr<Peer> m_self;

    //Peer Object holding information about
    //the next peer in the Ring
    std::shared_ptr<Peer> m_successor;

    //Peer Object holding information about
    //the previous Peer in the Ring
    std::shared_ptr<Peer> m_predecessor;


};

#endif /* ROUTINGTABLE_H */
