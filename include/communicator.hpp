/**
 * @class Communicator
 * @brief Classe template para comunicação entre componentes usando um canal específico
 * 
 * Esta classe implementa um padrão Observer para receber mensagens de forma assíncrona
 * e fornece métodos para envio de mensagens através de um canal de comunicação.
 * 
 * @tparam Channel Tipo do canal de comunicação a ser utilizado
 */
template <typename Channel>
class Communicator: public Concurrent_Observer<typename Channel::Observer::Observed_Data, 
                                               typename Channel::Observer::Observing_Condition>
{
   // Define um alias para a classe base Observer para simplificar o código
    typedef Concurrent_Observer<typename Channel::Observer::Observed_Data,
                              typename Channel::Observer::Observing_Condition> Observer;

    public:
        // Tipos públicos derivados do Channel
        typedef typename Channel::Buffer Buffer;   // Tipo do buffer de comunicação
        typedef typename Channel::Address Address; // Tipo do endereço no canal

    public:
        /**
         * @brief Construtor da classe Communicator
         * @param channel Ponteiro para o canal de comunicação
         * @param address Endereço deste comunicador no canal
         * 
         * Registra este comunicador no canal fornecido com o endereço especificado.
         */
        Communicator(Channel* channel, Address address): {
            _channel = channel; // Inicializa o canal
            _address = address; // Inicializa o endereço
            
            _channel->attach(this, address); // Registra no canal
        }

        /**
         * @brief Destrutor da classe
         * 
         * Remove o registro deste comunicador do canal.
         */
        ~Communicator() { 
            _channel->detach(this, _address); // Remove registro do canal
        }

        /**
         * @brief Envia uma mensagem através do canal
         * @param message Ponteiro para a mensagem a ser enviada
         * @return true se a mensagem foi enviada com sucesso, false caso contrário
         * 
         * A mensagem é enviada como broadcast para todos os destinatários no canal.
         */
        bool send(const Message* message) {
            return (_channel->send(_address, Channel::Address::BROADCAST, message->data(), message->size()) > 0);
        }

        /**
         * @brief Recebe uma mensagem do canal
         * @param message Ponteiro onde a mensagem recebida será armazenada
         * @return true se uma mensagem foi recebida com sucesso, false caso contrário
         * 
         * Este método bloqueia a execução até que uma mensagem esteja disponível.
         */
        bool receive(Message* message) {
            // Bloqueia até receber uma notificação de novos dados
            Buffer* buf = Observer::updated(); 
            
            Channel::Address from; // Endereço de origem da mensagem
            // Recebe os dados do canal
            int size = _channel->receive(buf, &from, message->data(), message->size());
            
            if(size > 0)
                return true; // Mensagem recebida com sucesso
                
            return false; // Falha ao receber mensagem
        }

    private:
        /**
         * @brief Método de callback chamado pelo canal quando novos dados estão disponíveis
         * @param obs Objeto observado (não utilizado nesta implementação)
         * @param c Condição de observação que disparou o callback
         * @param buf Buffer contendo os dados recebidos
         * 
         * Libera qualquer thread bloqueada no método receive().
         */
        void update(typename Channel::Observed* obs, 
                typename Channel::Observer::Observing_Condition c, 
                Buffer* buf) {
            Observer::update(c, buf); // Delega para a classe base
        }

    private:
        Channel* _channel;  // Ponteiro para o canal de comunicação
        Address _address;   // Endereço deste comunicador no canal
};