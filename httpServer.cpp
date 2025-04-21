#include <sstream>
#include <regex>
#include <fstream>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <unordered_map>
#include <string>

const std::string HTML_FOLDER = "../html";
const std::string STATIC = "../static/";

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
        POST,
        INVALID
    };

    HttpRequest(int client_sockfd, std::string request) : client_sockfd(client_sockfd) {
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
            method = Method::INVALID;
            return;
        }

        // Look for body Content-Length
        std::regex contentLengthRegex(R"(Content-Length:\s*(\d+))", std::regex::icase);
        std::smatch match;
        if (std::regex_search(request, match, contentLengthRegex)) {
            std::string lengthStr = match[1];
            int contentLength = std::stoi(lengthStr);
            std::cout << "Content-Length: " << contentLength << std::endl;
            // Receive contentLength bytes from client_sockfd
            char buf[contentLength];
            if (recv(client_sockfd, buf, contentLength, 0) == -1) {
                std::cerr << "Error reading from socket" << std::endl;
                return;
            }

            // Only parse body with key=value&key2=value2
            std::string bodyStr = std::string(buf, contentLength);
            body = parseBody(bodyStr);
        }

        // Get user agent
        std::regex userAgentRegex(R"(User-Agent:\s*(.*))", std::regex::icase);
        if (std::regex_search(request, match, userAgentRegex)) {
            userAgent = match[1];
        }
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

    const std::unordered_map<std::string, std::string>& getBody() const {
        return body;
    }

    std::string getUserAgent() const {
        return userAgent;
    }

private:
    Method method;
    int client_sockfd;
    std::string requestTarget;
    std::string protocol;
    std::string userAgent;
    std::unordered_map<std::string, std::string> body;

    std::unordered_map<std::string, std::string> parseBody(const std::string& bodyStr) {
        // Only parse body with key=value&key2=value2...
        std::unordered_map<std::string, std::string> ret;
        std::istringstream ss(bodyStr);
        std::string pair;
        while (std::getline(ss, pair, '&')) {
            std::size_t pos = pair.find('=');
            if (pos != std::string::npos) {
                std::string key = pair.substr(0, pos);
                std::string value = pair.substr(pos + 1);
                ret[key] = value;
            }
        }
        return ret;
    }
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
        std::string response = 
            "HTTP/1.1 " + std::to_string(code) + " " + httpStatusMessages.at(code) + "\r\n"
            "Content-Type: text/html; charset=UTF-8\r\n"
            "Connection: close\r\n"
            "Content-Length: " + std::to_string(body.size()) + "\r\n"
            "\r\n" +
            body;
        send(client_sockfd, response.c_str(), response.length(), 0);
    }

    void sendFile(int code, const std::string& path, const std::string& filename) const {
        std::ifstream file(path, std::ios::binary);

        std::stringstream ss;
        if (file) {
            ss << file.rdbuf();
            std::string body = ss.str();

            std::string response = 
                "HTTP/1.1 " + std::to_string(code) + " " + httpStatusMessages.at(code) + "\r\n"
                "Content-Type: application/octet-stream\r\n"
                "Content-Disposition: attachment; filename=\"" + filename + "\"\r\n"
                "Content-Length: " + std::to_string(body.size()) + "\r\n"
                "Connection: close\r\n"
                "\r\n" +
                body;

            send(client_sockfd, response.c_str(), response.length(), 0);
        }
        else {
            std::ifstream file404(HTML_FOLDER + "/404.html");
            ss << file404.rdbuf();
            sendResponse(404, ss.str());
        }

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
            HttpRequest httpRequest(client_sockfd, request);
            // std::cout << request << std::endl;
            HttpResponse httpResponse(client_sockfd, httpRequest);
            if (httpRequest.getMethod() == HttpRequest::Method::GET) {
                std::string path = httpRequest.getRequestTarget();
                std::cout << "Requested: " << path << std::endl;

                if (httpRequest.getRequestTarget() == "/user-agent") {
                    httpResponse.sendResponse(200, httpRequest.getUserAgent()); 
                    continue;
                }

                // If path starts with /static, look in static folder
                if (httpRequest.getRequestTarget().find("/static") == 0) {
                    // Find the next /
                    std::size_t pos = httpRequest.getRequestTarget().find("/", 1);
                    // Substring from the next /
                    std::string file = httpRequest.getRequestTarget().substr(pos + 1);
                    path = STATIC + file;
                    std::cout << path << std::endl;
                    httpResponse.sendFile(200, path, file);
                    continue;
                }

                if (httpRequest.getRequestTarget() == "/") {
                    std::cout << "Requested index file" << std::endl;
                    path = "/index";
                }
                std::ifstream file(HTML_FOLDER + path + ".html");
                std::ostringstream ss;
                if (!file.is_open()) {
                    // Send 404 error        
                    std::cout << "Requested file not found" << std::endl;
                    std::ifstream file404(HTML_FOLDER + "/404.html");
                    ss << file404.rdbuf();
                    httpResponse.sendResponse(404, ss.str());
                    continue;
                }
                ss << file.rdbuf();
                httpResponse.sendResponse(200, ss.str());
            }
            else if (httpRequest.getMethod() == HttpRequest::Method::POST) {
                std::cout << "Received POST request" << std::endl;
                std::unordered_map<std::string, std::string> body = httpRequest.getBody();
                std::string response = "You entered: <br>";
                for (const auto& [key, value] : body) {
                    response += key + ": " + value + "<br>";
                }
                httpResponse.sendResponse(200, response);
            }
            close(client_sockfd);
        }
    }
};
