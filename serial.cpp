#include <iostream>
#include <string>
#include <asio.hpp>
#define MAXLEN 1024 // maximum buffer size

int main()
{
    asio::io_service io;
    try
    {
        // create a serial port object
        asio::serial_port serial(io);
        serial.open("/dev/ttyM");

        for (;;)
        {
            std::string input;
            std::cout << "tty input: ";
            std::cin >> input;

            if (input == "exit") break;
            asio::write(serial, asio::buffer(input));

            char data[MAXLEN];
            size_t nread = asio::read(
                serial, asio::buffer(data, 64)
            );
            std::string message(data, nread);
            std::cout << "tty output: ";
            std::cout << message << std::endl;
        }
        serial.close();
    }
    catch (asio::system_error& e)
    {
        std::cerr << e.what() << std::endl;
    }
    return 0;
}
