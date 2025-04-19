#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

class HttpServer {
private:
    int port;
    int sockfd;
    struct sockaddr_in server_addr;
public:
    HttpServer(int port) {
        this->port = port;
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            std::cerr << "Error creating socket" << std::endl;
            exit(1);
        }
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(port);
        int optval = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
            close(sockfd);
            exit(1);
        }
        if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            std::cerr << "Error binding socket" << std::endl;
            exit(1);
        }
        if (listen(sockfd, 5) < 0) {
            std::cerr << "Error listening on socket" << std::endl;
            exit(1);
        }
        std::cout << "Listening for connections..." << std::endl;
    }

    bool httpHeaderEnd(const std::string& header) const {
        // Returns true if the string ends with \r\n\r\n
        const std::string suffix = "\r\n\r\n";
        if(header.length() >= suffix.length() &&
            header.compare(header.length() - suffix.length(), suffix.length(), suffix) == 0) {
            return true;
        }
        else{
            return false;
        }
    }

    void run() {
        while (true) {
            struct sockaddr_in client_addr;
            socklen_t client_addr_len = sizeof(client_addr);
            int client_sockfd = accept(sockfd, (struct sockaddr *)&client_addr, &client_addr_len);
            if (client_sockfd < 0) {
                std::cerr << "Error accepting connection" << std::endl;
                exit(1);
            }
            std::cout << "Connection accepted from " << inet_ntoa(client_addr.sin_addr) << ":" << ntohs(client_addr.sin_port) << std::endl;
            // Read one byte at a time until \r\n\r\n
            std::string request = "";
            // char buf[4096];
            // int bytes_received = recv(client_sockfd, &buf, sizeof(buf), 0);
            // std::cout << bytes_received << std::endl;
            // buf[bytes_received] = '\0';
            // std::cout << buf << std::endl;

            while(!httpHeaderEnd(request)) {
                char buf;
                if (recv(client_sockfd, &buf, 1, 0) == -1) {
                    std::cerr << "Error reading from socket" << std::endl;
                    break;
                }
                std::cout << buf;
                request += buf;
            }
            std::cout << request << std::endl;
            // std::cout << "hi";
            close(client_sockfd);
        }
    }
};