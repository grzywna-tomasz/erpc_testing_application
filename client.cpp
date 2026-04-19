#include "erpc_client_setup.h"
#include "erpc_tcp_transport.hpp"
#include "erpc_basic_codec.hpp"
#include "erpc_client_manager.h"
#include "erpc_crc16.hpp"
#include "example_client.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <sstream>
#include <iomanip>

template< typename T >
std::string int_to_hex( T i )
{
  std::stringstream stream;
  stream << "0x" << std::setfill ('0') << std::setw(sizeof(T)*2) << std::hex << i;
  return stream.str();
}

constexpr int ADDITIONAL_DATA_0X200_SIZE {3};

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

class ErrorPrinter
{
public:
    void print(const Erpc_ErrorInfo_t &error_info)
    {        
        if (error_info.error_id == DET_SERVER_FAILED_TO_POLL)
        {
            print_Error0x200_ServerFailedToPoll(error_info.additional_data);
        }
        else
        {
            const std::string error_description {"Unknown error ID: " + int_to_hex(static_cast<int>(error_info.error_id))};
            print_ErrorDuringDecoding(error_description, error_info.additional_data);
        }
    }

private:
    void print_Error0x200_ServerFailedToPoll(const list_uint8_1_t &additional_data)
    {
        if (additional_data.elementsCount == ADDITIONAL_DATA_0X200_SIZE)
        {
            std::cout << "Server failed to poll, app version: 0x" << std::hex << static_cast<int>((additional_data.elements[1] << 8) | additional_data.elements[0]) << std::dec << "\n";
            std::cout << "Fail code: " << static_cast<int>(additional_data.elements[2]) << "\n";
        }
        else
        {
            const std::string error_description {"Cannot interpret error 0x200, expected " + std::to_string(ADDITIONAL_DATA_0X200_SIZE) + " received " + std::to_string(additional_data.elementsCount)};
            print_ErrorDuringDecoding(error_description, additional_data);
        }
    }

    void print_ErrorDuringDecoding(const std::string &error_description, const list_uint8_1_t &additional_data)
    {
        std::cout << error_description << "\n";
        std::cout << "Additional data count: " << additional_data.elementsCount << "\n";
        std::cout << "Additional data: " << std::hex;
        for (uint8_t additional_data_index = 0; additional_data.elementsCount > additional_data_index; ++additional_data_index)
        {
            std::cout << static_cast<int>(additional_data.elements[additional_data_index]);
        }
        std::cout << "\n";
    }
};

class ApplicationClient
{
public:
    ApplicationClient(erpcShim::Communication_client *communication_client) : 
        m_communication_client {communication_client}
    {

    }

    void AppVersion_Get()
    {
        std::cout << "App version: " << std::hex << m_communication_client->AppVersion_Get() << std::dec << std::endl;
    }

    void Errors_GetAll()
    {
        list_Erpc_ErrorInfo_t_1_t *errors = m_communication_client->Errors_GetAll();
        if (errors)
        {
            std::cout << "Received " << errors->elementsCount << " errors:\n";
            for (uint32_t i = 0; i < errors->elementsCount; ++i)
            {
                m_error_printer.print(errors->elements[i]);
            }
        }
    }

    void ClearErrors()
    {
        std::cout << "Clear errors status: " << print(m_communication_client->Errors_ClearAll());
    }


private:
    erpcShim::Communication_client *m_communication_client;
    ErrorPrinter m_error_printer;

    std::string_view print(Erpc_Status_t status)
    {
        if (status == ERPC_OK)
        {
            return "Operation successful\n";
        }
        else
        {
            return "Operation failed\n";
        }
    }
};


int main()
{
    erpc::TCPTransport transport("192.168.100.10", 50000, false);
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

    ApplicationClient appClient {new erpcShim::Communication_client(&clientManager)};
    // erpcShim::Communication_client client(&clientManager);

    while(1)
    {
        std::cout << "Request action: ";

        std::string action;
        std::cin >> action;
        if (action == "exit")
        {
            break;
        }
        else if (action == "help")
        {
            std::cout << "Available actions:\n\
            version - get application version\n\
            errors - get all errors\n\
            clear - clear all errors\n\
            exit - exit application\n";
        }
        else if (action == "version")
        {
            appClient.AppVersion_Get();
        }
        else if (action == "errors")
        {
            appClient.Errors_GetAll();
        }
        else if (action == "clear")
        {
            appClient.ClearErrors();
        }
        else
        {
            std::cout << "Unknown action\n";
        }
        std::cout << "\n";
    }

    transport.close();

    return 0;
}