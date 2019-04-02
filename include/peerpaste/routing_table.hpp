class RoutingTable
{
public:
    typedef std::shared_ptr<Peer> PeerPtr;
    RoutingTable () : self_(std::make_shared<Peer>()), predecessor_(nullptr) {}

    void print() const
    {
        return;
        std::cout << "#### ROUTINGTABLE BEGIN ####" << '\n';
        std::cout << "SELF:" << '\n';
        self_->print();
        std::cout << "PREDECESSOR: " << '\n';
        if(predecessor_ != nullptr){
            predecessor_->print();
        }
        std::cout << "FINGERTABLE: " << '\n';
        for(const auto peer : peers_){
            peer->print();
            std::cout << "######" << '\n';
        }
        std::cout << "#### ROUTINGTABLE END ####" << '\n';
    }

    const PeerPtr get_self() const
    {
        return self_;
    }

    void set_self(const PeerPtr self)
    {
        self_ = self;
    }

    const PeerPtr get_predecessor() const
    {
        return predecessor_;
    }

    void set_predecessor(const PeerPtr predecessor)
    {
        predecessor_ = predecessor;
    }

    void set_successor(const PeerPtr sucessor)
    {
        if(size() == 0) {
            push_back(sucessor);
            return;
        }
        peers_[0] = sucessor;
    }

    const PeerPtr get_successor() const
    {
        if(size() != 0){
            return peers_.front();
        }
        return nullptr;
    }

    const size_t size() const noexcept
    {
        return peers_.size();
    }

    void push_back(PeerPtr peer) noexcept
    {
        peers_.push_back(peer);
    }

    std::vector<PeerPtr> get_peers() const noexcept
    {
        return peers_;
    }

    /*
     * Checks if successor and predecessor are set
     */
    bool is_valid()
    {
        const auto successor = get_successor();
        const auto predecessor = get_predecessor();
        if(successor != nullptr && successor != self_){
            if(predecessor != nullptr && predecessor != self_){
                return true;
            }
        }
        return false;
    }

private:
    PeerPtr self_;
    PeerPtr predecessor_;
    std::vector<PeerPtr> peers_;
};
