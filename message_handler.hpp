#include "message.hpp"
#include <functional>

class MessageHandler
{
public:
    MessageHandler ()
    {
        message_queue_ = MessageQueue::GetInstance();
    }

    ~MessageHandler () {}

    void handle_message(){
        /* using namespace std::placeholders; */
        /* std::function<void(int,int)> f = std::bind(&Foo::doSomethingArgs, this, _1, _2); */
        std::function<void(void)> f_test = std::bind(&MessageHandler::test, this);
        f_test();

        std::function<void(int,int)> f = std::bind(&MessageHandler::test2, this, std::placeholders::_1,
                                                                                 std::placeholders::_2);
        f(1,2);
    }

    void test(){ std::cout << "This is a test" << std::endl; }
    void test2(int i, int y){ std::cout << "This is a test " << i + y << std::endl; }

private:
    std::shared_ptr<MessageQueue> message_queue_;

    std::map<std::string, std::function<void(Message)>> open_requests_;
};
