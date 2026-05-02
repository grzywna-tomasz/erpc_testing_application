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
#include "erpc_port.h"
#include <fstream>
#include "nlohmann/json.hpp"
#include <optional>
#include <sstream>

template< typename T >
std::string int_to_hex( T i )
{
  std::stringstream stream;
  stream << "0x" << std::setfill ('0') << std::setw(sizeof(T)*2) << std::hex << i;
  return stream.str();
}

int nibbleStrToInt(char c)
{
    int ret_val;
    if ((c >= 'a') && (c <= 'f'))
    {
        ret_val = c - 'a' + 10;
    }
    else if ((c >= '0') && (c <= '9'))
    {
        ret_val = c - '0';
    }
    else if ((c >= 'A') && (c <= 'F'))
    {
        ret_val = c - 'A' + 10;
    }
    return ret_val;
}

std::vector<uint8_t> hexToBytes(const std::string& hex)
{
    int start_index = 0;
    if (hex.compare(0, 2, "0x") == 0)
    {
        start_index = 2;
    }

    /* Divide by 2 to get number of bytes, not nibbles */
    std::vector<uint8_t> ret_val((hex.length() - start_index) / 2);

    for (int index = start_index; index < hex.length(); index += 2)
    {
        ret_val[(index - start_index) / 2] = nibbleStrToInt(hex[index]) * 16 + nibbleStrToInt(hex[index + 1]);
    }

    return ret_val;
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

typedef struct
{
    uint32_t address;
    uint32_t size;
} VariableData_t;

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

class JsonVariableReader
{
public:
    JsonVariableReader(const std::string& path_to_file)
    {
        try 
        {
            std::ifstream file(path_to_file);
            m_json = nlohmann::json::parse(file);
        }
        catch (const std::exception& e)
        {
            std::cerr << "Error: " << e.what() << "\n";
        }
    }

    std::optional<VariableData_t> getVariableData(const std::string& var_name) const
    {
        if (!m_json.contains(var_name))
        {
            return std::nullopt;
        }
        else
        {
            return VariableData_t {.address = m_json[var_name]["addr"], .size = m_json[var_name]["size"]};
        }
    }

    std::optional<int> getVariableSize(const std::string& var_name) const
    {
        if (!m_json.contains(var_name))
        {
            return std::nullopt;
        }
        else
        {
            return int {m_json[var_name]["size"]};
        }
    }

private:
    nlohmann::json m_json {};
};

class ApplicationClient
{
public:
    ApplicationClient(erpcShim::Communication_client *communication_client, const std::string& path_to_file) : 
        m_communication_client {communication_client}, m_variable_reader {path_to_file}
    {

    }

    void AppVersion_Get()
    {
        std::cout << "App version: " << std::hex << m_communication_client->AppVersion_Get() << std::dec << std::endl;
    }

    void Errors_GetAll()
    {
        list_Erpc_ErrorInfo_t_1_t *errors = (list_Erpc_ErrorInfo_t_1_t *)erpc_malloc(sizeof(list_Erpc_ErrorInfo_t_1_t));
        Erpc_Status_t result {m_communication_client->Errors_GetAll(errors)};
        if ((ERPC_OK == result) && (errors))
        {
            std::cout << "Received " << errors->elementsCount << " errors:\n";
            for (uint32_t i = 0; i < errors->elementsCount; ++i)
            {
                m_error_printer.print(errors->elements[i]);
            }
        }
        else
        {
            std::cout << "Failed to receive Errors_GetAll\n";
        }

        free(errors);
    }

    void ClearErrors()
    {
        std::cout << "Clear errors status: " << print(m_communication_client->Errors_ClearAll());
    }

    void Debug_ReadVariableFromRam(const std::string& var_name)
    {
        std::optional<VariableData_t> variable {m_variable_reader.getVariableData(var_name)};
        if (variable)
        {
            list_uint8_1_t *variable_value = (list_uint8_1_t *)erpc_malloc(sizeof(list_uint8_1_t));
            if (ERPC_OK == m_communication_client->Memory_Read(variable.value().address, variable.value().size, variable_value))
            {
                if (variable_value)
                {
                    std::cout << var_name << " has value 0x" << std::hex << std::setfill('0');
                    for (int i = 0; i < variable_value->elementsCount; ++i)
                    {
                        // std::cout << std::format("{:02x}", variable_value->elements[i]);
                        std::cout << std::setw(2) << static_cast<int>(variable_value->elements[i]);
                    }
                    std::cout << std::dec << std::setfill(' ') << "\n";
                }
            }
            else
            {
                std::cout << "Reading variable from RAM failed\n";
            }
            free(variable_value);
        }
        else
        {
            std::cout << "Variable: " << var_name << "not in json\n";
        }
    }

    void Debug_WriteVariableToRam(const std::string& var_name, uint8_t *data, uint32_t data_size)
    {
        std::optional<VariableData_t> variable {m_variable_reader.getVariableData(var_name)};
        if (variable)
        {
            if (data_size == variable.value().size)
            {
                const list_uint8_1_t data_to_send {.elements = data, .elementsCount = data_size};
                if (ERPC_OK == m_communication_client->Memory_Write(variable.value().address, &data_to_send))
                {
                    std::cout << var_name << " written successfully\n";
                }
                else
                {
                    std::cout << "failed to write " << var_name << "\n";
                }
            }
            else
            {
                std::cout << "Failed to write variable " << var_name << "size do not match (expected " << variable.value().size << ", given " << data_size << ")\n";
            }
        }
        else
        {
            std::cout << "Variable: " << var_name << "not in json\n";
        }
    }

private:
    erpcShim::Communication_client *m_communication_client;
    ErrorPrinter m_error_printer;
    JsonVariableReader m_variable_reader;

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

    void free(list_Erpc_ErrorInfo_t_1_t *errors)
    {
        if (errors)
        {
            if (errors->elements)
            {
                for (int i = 0; i < errors->elementsCount; ++i)
                {
                    if (errors->elements[i].additional_data.elements)
                    {
                        erpc_free(errors->elements[i].additional_data.elements);
                    }
                }
                erpc_free(errors->elements);
            }
            erpc_free(errors);
        }
    }

    void free(list_uint8_1_t *list)
    {
        if (list)
        {
            if (list->elements)
            {
                erpc_free(list->elements);
            }
            erpc_free(list);
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

    ApplicationClient appClient {new erpcShim::Communication_client(&clientManager), "../../../Nucleo_F439ZI/build/Debug/variables.json"};
    // erpcShim::Communication_client client(&clientManager);

    while(1)
    {
        std::cout << "Request action: ";

        std::string line;
        // std::cin >> action;
        std::getline(std::cin, line);
        std::istringstream iss(line);
        std::vector<std::string> args;
        std::string word;
        while (iss >> word)
        {
            args.push_back(word);
        }


        if (args.at(0) == "exit")
        {
            break;
        }
        else if (args.at(0) == "help")
        {
            std::cout << "Available actions:\n\
            version - get application version\n\
            errors - get all errors\n\
            clear - clear all errors\n\
            read XXX - read variable from RAM, XXX is variable name\n\
            write XXX 0xYYY - write data to RAM at address XXX with 0xYYY data\n\
            exit - exit application\n";
        }
        else if (args.at(0) == "version")
        {
            appClient.AppVersion_Get();
        }
        else if (args.at(0) == "errors")
        {
            appClient.Errors_GetAll();
        }
        else if (args.at(0) == "clear")
        {
            appClient.ClearErrors();
        }
        else if (args.at(0) == "read")
        {
            appClient.Debug_ReadVariableFromRam(args.at(1));
        }
        else if (args.at(0) == "write")
        {
            std::vector<uint8_t> data = hexToBytes(args.at(2));
            for (int i = 0; i < 10; i++)
            {
                std::cout << static_cast<int>(data.data()[i]);
            }
            std::cout << "\n";
            appClient.Debug_WriteVariableToRam(args.at(1), data.data(), data.size());
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
