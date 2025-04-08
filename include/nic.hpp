


class NIC: public Ethernet, public Conditional_Data_Observed<Buffer<Ethernet::Frame>,
Ethernet::Protocol>, private Engine {
    public:

    Buffer * alloc(Address dst, Protocol_Number prot, unsigned int size);
    int send(Buffer * buf);
    void free(Buffer * buf);
    int receive(Buffer * buf, Address * src, Address * dst, void * data, unsigned int size);

    void attach(Observer * obs, Protocol_Number prot); // possibly inherited
    void detach(Observer * obs, Protocol_Number prot); // possibly inherited
};