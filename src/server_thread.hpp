#pragma once
#ifndef SERVER_THREAD_H
#define SERVER_THREAD_H

#include "connection_utilities.hpp"
#include "server_thread_interface.hpp"
#include "websocket_server_interface.hpp"
#include "websocket_server_thread.hpp"
#include "server.hpp"

// networking
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// standard
#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <nlohmann/json.hpp>

// webosocket key
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>

// threading
#include <thread>

class Server;

using json = nlohmann::json;

class HttpParsedRaw
{
public:
    HttpParsedRaw(char *buffer);

    std::string toString();

    std::vector<std::string> header_lines;
    std::string body;
};

HttpParsedRaw::HttpParsedRaw(char *buffer)
{
    std::string http = std::string(buffer);
    std::string head_to_body_delimiter = "\r\n\r\n";
    std::string header = http.substr(0, http.find(head_to_body_delimiter) + head_to_body_delimiter.length());
    this->body = http.substr(http.find(head_to_body_delimiter) + head_to_body_delimiter.length());

    std::string header_line;
    std::istringstream header_stream(header);

    while (std::getline(header_stream, header_line))
        if (header_line != "\r")
            header_lines.push_back(header_line);
}

std::string HttpParsedRaw::toString()
{
    std::string result = "";
    for (std::string header_line : header_lines)
        result += "row-> " + header_line + "\n";

    result += "\n\n" + body;
    return result;
}

class HttpParsed : public HttpParsedRaw
{
public:
    HttpParsed(char *buffer) : HttpParsedRaw(buffer)
    {
        std::string request_line = this->header_lines[0];
        std::stringstream request_line_stream(request_line);

        request_line_stream >> method >> path >> version;

        for (int i = 1; i < header_lines.size(); i++)
        {
            std::string header_line = header_lines[i].substr(0, header_lines[i].length() - 1);
            std::string header_name = header_line.substr(0, header_line.find(":"));
            std::string header_value = header_line.substr(header_line.find(":") + 2);
            headers[header_name] = header_value;
        }
    }

    std::string method;
    std::string path;
    std::string version;
    json headers;

private:
};

class ServerThread : public BaseServerThread
{
public:
    ServerThread(std::unique_ptr<ClientConnectionMetadata> connectionMetadata, std::weak_ptr<BaseWebsocketServer> server) : connectionMetadata_(std::move(connectionMetadata)), server_(server)
    {
        std::thread thread(&ServerThread::start_handling, this);
        thread.detach();
    }
    void start_handling() override;
    bool yeet() override { return yeet_flag; }

    ~ServerThread() override
    {
        std::cout << "ServerThread destructor called" << std::endl;
    }

private:
    bool yeet_flag = false;
    std::unique_ptr<ClientConnectionMetadata> connectionMetadata_;
    std::weak_ptr<BaseWebsocketServer> server_;

    bool is_upgrade_request(HttpParsed &httpParsed)
    {
        auto it = httpParsed.headers.find("Upgrade");
        if (it == httpParsed.headers.end())
            return false;

        std::string upgrade = httpParsed.headers["Upgrade"];
        return upgrade == "websocket";
    }

    std::string computeWebsocketAcceptKey(const std::string &websocketKey);

    std::string buildUpgradeResponse(const std::string &websocketAcceptKey)
    {
        std::string response = "HTTP/1.1 101 Switching Protocols\r\n";
        response += "Upgrade: websocket\r\n";
        response += "Connection: Upgrade\r\n";
        response += "Sec-WebSocket-Accept: " + websocketAcceptKey + "\r\n";
        response += "\r\n";
        return response;
    }
};

std::string ServerThread::computeWebsocketAcceptKey(const std::string &websocketKey)
{
    const std::string magicString = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

    std::string concatenated = websocketKey + magicString;

    // SHA1 hash
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char *>(concatenated.c_str()), concatenated.length(), hash);

    // base64 encode
    BIO *bio, *b64;
    BUF_MEM *bufferPtr;

    b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);

    BIO_write(bio, hash, SHA_DIGEST_LENGTH);
    BIO_flush(bio);
    BIO_get_mem_ptr(bio, &bufferPtr);
    BIO_set_close(bio, BIO_NOCLOSE);
    BIO_free_all(bio);

    std::string base64_encoded((*bufferPtr).data, (*bufferPtr).length);
    return base64_encoded;
}

std::string buildUpgradeResponse(const std::string &websocketAcceptKey)
{
    std::string response = "HTTP/1.1 101 Switching Protocols\r\n";
    response += "Upgrade: websocket\r\n";
    response += "Connection: Upgrade\r\n";
    response += "Sec-WebSocket-Accept: " + websocketAcceptKey + "\r\n";
    response += "\r\n";
    return response;
}

void ServerThread::start_handling()
{
    char buffer[4096] = {0};
    while (true)
    {
        memset(buffer, 0, sizeof(buffer));
        int valread = read(this->connectionMetadata_->get(), buffer, sizeof(buffer));
        if (valread <= 0)
        {
            this->yeet_flag = true;
            break;
        }

        HttpParsed http = HttpParsed(buffer);
        if (!is_upgrade_request(http))
            continue;

        // upgrade
        std::string websocketKey = http.headers["Sec-WebSocket-Key"];
        std::string websocketAcceptKey = computeWebsocketAcceptKey(websocketKey);

        std::string response = buildUpgradeResponse(websocketAcceptKey);

        int bytesSent = send(this->connectionMetadata_->get(), response.c_str(), response.length(), 0);
        if (bytesSent == -1)
        {
            std::cerr << "Failed to send response: " << strerror(errno) << '\n';
            this->yeet_flag = true;
            break;
        }

        std::unique_ptr<WebsocketServerThread> websocketServerThread = std::make_unique<WebsocketServerThread>(std::move(this->connectionMetadata_), this->server_);
        this->server_.lock()->upgrade(std::move(websocketServerThread));
        break;
    }
}

#endif // !SERVER_THREAD_H