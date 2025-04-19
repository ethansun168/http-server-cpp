#include <sstream>
#include <fstream>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <unordered_map>
#include <string>

const std::string HTML_FOLDER = "../html";

const std::unordered_map<int, std::string> httpStatusMessages = {
    // 1xx Informational
    {100, "Continue"},
    {101, "Switching Protocols"},
    {102, "Processing"},
    {103, "Early Hints"},

    // 2xx Success
    {200, "OK"},
    {201, "Created"},
    {202, "Accepted"},
    {203, "Non-Authoritative Information"},
    {204, "No Content"},
    {205, "Reset Content"},
    {206, "Partial Content"},
    {207, "Multi-Status"},
    {208, "Already Reported"},
    {226, "IM Used"},

    // 3xx Redirection
    {300, "Multiple Choices"},
    {301, "Moved Permanently"},
    {302, "Found"},
    {303, "See Other"},
    {304, "Not Modified"},
    {305, "Use Proxy"},
    {307, "Temporary Redirect"},
    {308, "Permanent Redirect"},

    // 4xx Client Errors
    {400, "Bad Request"},
    {401, "Unauthorized"},
    {402, "Payment Required"},
    {403, "Forbidden"},
    {404, "Not Found"},
    {405, "Method Not Allowed"},
    {406, "Not Acceptable"},
    {407, "Proxy Authentication Required"},
    {408, "Request Timeout"},
    {409, "Conflict"},
    {410, "Gone"},
    {411, "Length Required"},
    {412, "Precondition Failed"},
    {413, "Payload Too Large"},
    {414, "URI Too Long"},
    {415, "Unsupported Media Type"},
    {416, "Range Not Satisfiable"},
    {417, "Expectation Failed"},
    {418, "I'm a teapot"},
    {421, "Misdirected Request"},
    {422, "Unprocessable Entity"},
    {423, "Locked"},
    {424, "Failed Dependency"},
    {425, "Too Early"},
    {426, "Upgrade Required"},
    {428, "Precondition Required"},
    {429, "Too Many Requests"},
    {431, "Request Header Fields Too Large"},
    {451, "Unavailable For Legal Reasons"},

    // 5xx Server Errors
    {500, "Internal Server Error"},
    {501, "Not Implemented"},
    {502, "Bad Gateway"},
    {503, "Service Unavailable"},
    {504, "Gateway Timeout"},
    {505, "HTTP Version Not Supported"},
    {506, "Variant Also Negotiates"},
    {507, "Insufficient Storage"},
    {508, "Loop Detected"},
    {510, "Not Extended"},
    {511, "Network Authentication Required"}
};

class HttpRequest {
public:
    enum class Method {
        GET,
        POST
    };

    HttpRequest(std::string request) {
        std::stringstream ss(request);
        std::string m;
        ss >> m >> requestTarget >> protocol;
        if (m == "GET") {
            method = Method::GET;
        }
        else if (m == "POST") {
            method = Method::POST;
        }
        else {
            std::cerr << "Invalid request method" << std::endl;
        }
        // std::string line;
        // while (std::getline(ss, line)) {
        //     if (line == "") {
        //         break;
        //     }
        // }
    }

    Method getMethod() const {
        return method;
    }

    std::string getRequestTarget() const {
        return requestTarget;
    }

    std::string getProtocol() const {
        return protocol;
    }

private:
    Method method;
    std::string requestTarget;
    std::string protocol;
    // std::map<std::string, std::string> headers;
    std::string body;
};

inline std::ostream& operator<<(std::ostream& os, HttpRequest::Method method) {
    switch(method) {
        case HttpRequest::Method::GET:
            os << "GET";
            return os;
        case HttpRequest::Method::POST:
            os << "POST";
            return os;
        default:
            os << "Invalid method";
            return os;
    }
}

class HttpResponse {
public:
    HttpResponse(int client_sockfd, const HttpRequest& request) : client_sockfd(client_sockfd), request(request) {}

    void sendResponse(int code, const std::string& body) const {
        // TODO: based on request
        std::string response = 
            "HTTP/1.1 " + std::to_string(code) + " " + httpStatusMessages.at(code) + "\r\n"
            "Content-Type: text/html; charset=UTF-8\r\n"
            "Content-Length: " + std::to_string(body.size()) + "\r\n"
            "\r\n" +
            body;

        send(client_sockfd, response.c_str(), response.length(), 0);
    }

private:
    int client_sockfd;
    HttpRequest request;
};

class HttpServer {
private:
    int port;
    int sockfd;
    struct sockaddr_in server_addr;

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
            std::string request = "";
            while(!httpHeaderEnd(request)) {
                char buf;
                if (recv(client_sockfd, &buf, 1, 0) == -1) {
                    std::cerr << "Error reading from socket" << std::endl;
                    break;
                }
                request += buf;
            }
            HttpRequest httpRequest(request);
            // std::cout << request << std::endl;
            HttpResponse httpResponse(client_sockfd, httpRequest);
            if (httpRequest.getMethod() == HttpRequest::Method::GET) {
                std::string path = httpRequest.getRequestTarget();
                std::cout << "Requested: " << path << std::endl;
                if (httpRequest.getRequestTarget() == "/") {
                    std::cout << "Requested index file" << std::endl;
                    path = "/index.html";
                }
                std::ifstream file(HTML_FOLDER + path);
                if (!file.is_open()) {
                    // Send 404 error        
                    std::cout << "Requested file not found" << std::endl;
                    httpResponse.sendResponse(404, "file not found");
                    continue;
                }
                std::ostringstream ss;
                ss << file.rdbuf();
                httpResponse.sendResponse(200, ss.str());
            }

            // std::cout << "Request type: " << httpRequest.getMethod() << std::endl;
            // std::cout << "Request target: " << httpRequest.getRequestTarget() << std::endl;
            close(client_sockfd);
        }
    }
};
