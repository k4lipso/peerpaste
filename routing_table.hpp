class RoutingTable
{
public:
    typedef std::shared_ptr<Peer> PeerPtr;
    RoutingTable () : self_(std::make_shared<Peer>()), predecessor_(std::make_shared<Peer>()) {}

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

private:
    PeerPtr self_;
    PeerPtr predecessor_;
    std::vector<PeerPtr> peers_;
};
