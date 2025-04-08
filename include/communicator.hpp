


class Communicator {
public:
    bool send(const Message * message);

    bool receive(Message * message);
    

private:
    void update(typename Channel::Observed * obs, typename Channel::Observer::Observing_Condition c, Buffer * buf);

};
