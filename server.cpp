#include "httpServer.cpp"


int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cout << "Usage: ./server [port]"  << std::endl;
        return 1;
    }
    int port = 0;
    try {
        port = std::stoi(argv[1]);
    }
    catch (...) {
        std::cerr << "Invalid port number" << std::endl;
        return 1;
    }
    if (port < 0 || port > 65535) {
        std::cerr << "Invalid port number" << std::endl;
        return 1;
    }
    std::cout << "Server started on port " << port << std::endl;
    HttpServer server(port);
    server.run();
    return 0;
}