//
// Created by jglrxavpok on 25/08/2021.
//
#include "../TestFramework.cpp"
#include <iostream>
#include <asio.hpp>

void print(const asio::error_code&) {
    std::cout << "Hello, world!" << std::endl;
}

int main() {
    using asio::ip::tcp;
    try {
        asio::io_context io_context;

        tcp::resolver resolver(io_context);

        tcp::resolver::results_type endpoints =
                resolver.resolve("localhost", "daytime");

        tcp::socket socket(io_context);
        asio::connect(socket, endpoints);

        while(true) {
            std::array<char, 128> buf;
            asio::error_code error;

            std::size_t len = socket.read_some(asio::buffer(buf), error);
            if (error == asio::error::eof) {
                break;
            } else if(error) {
                throw asio::system_error(error);
            }

            std::cout.write(buf.data(), len);
        }
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}