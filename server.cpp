#include <iostream>
#include "erpc_simple_server.hpp"
#include "erpc_tcp_transport.hpp"
#include "erpc_basic_codec.hpp"
#include "erpc_message_buffer.hpp"
#include "erpc_crc16.hpp"

#include "example_server.hpp"
#include "example_interface.hpp"

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
        if (buf && *buf)
        {
            delete[] buf->get();
        }
    }
};

class Communication_impl : public erpcShim::Communication_interface
{
public:
    virtual status SendRequest(uint8_t test_variable) override
    {
        std::cout << "Server received: " << (int)test_variable << std::endl;
        return (test_variable > 0) ? ERPC_OK : ERPC_NOT_OK;
    }
};

int main()
{
    Communication_impl impl;
    erpcShim::Communication_service service(&impl);

    const char* host = "0.0.0.0";
    uint16_t port = 12345;
    erpc::TCPTransport transport(host, port, true);
    erpc::Crc16 crc16;
    transport.setCrc16(&crc16);
    if (transport.open() != kErpcStatus_Success)
    {
        std::cerr << "Failed to open server transport\n";
        return 1;
    }

    MyMessageBufferFactory msgFactory;
    erpc::BasicCodecFactory codecFactory;
    erpc::SimpleServer server;
    server.setTransport(&transport);
    server.setCodecFactory(&codecFactory);
    server.setMessageBufferFactory(&msgFactory);
    server.addService(&service);

    std::cout << "eRPC TCP server listening on " << host << ":" << port << std::endl;

    server.run();

    return 0;
}