#include "erpc_client_setup.h"
#include "erpc_tcp_transport.hpp"
#include "erpc_basic_codec.hpp"
#include "erpc_client_manager.h"
#include "erpc_crc16.hpp"
#include "example_client.hpp"
#include <iostream>
#include <thread>
#include <chrono>

class MyMessageBufferFactory : public erpc::MessageBufferFactory
{
public:
    virtual erpc::MessageBuffer create()
    {
        uint8_t *buf = new uint8_t[1024];
        return erpc::MessageBuffer(buf, 1024);
    }

    virtual void dispose(erpc::MessageBuffer *buf)
    {
        erpc_assert(buf);
        if (*buf)
        {
            delete[] buf->get();
        }
    }
};

int main()
{
    erpc::TCPTransport transport("127.0.0.1", 12345, false);
    erpc::Crc16 crc16;
    transport.setCrc16(&crc16);
    if (transport.open() != kErpcStatus_Success)
    {
        std::cout << "Failed to open client transport\n";
        return 1;
    }

    MyMessageBufferFactory msgFactory;
    erpc::BasicCodecFactory codecFactory;
    erpc::ClientManager clientManager;
    clientManager.setMessageBufferFactory(&msgFactory);
    clientManager.setTransport(&transport);
    clientManager.setCodecFactory(&codecFactory);
    erpcShim::Communication_client client(&clientManager);

    while(1)
    {
        // Example communication 
        uint8_t value = 42;
        status result = client.SendRequest(value);
    
        std::cout << "Client sent: " << static_cast<int>(value) << std::endl;
        std::cout << "Client received status: " << static_cast<int>(result) << std::endl;

        std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    transport.close();

    return 0;
}