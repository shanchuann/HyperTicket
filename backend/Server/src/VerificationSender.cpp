#include "../include/VerificationSender.hpp"

#include "../../ChronoLite/include/Logger.hpp"

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/ssl.h>

#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <sstream>
#include <string>

namespace hyperticket
{
    namespace
    {
        std::string base64(const std::string &input)
        {
            std::string output(4 * ((input.size() + 2) / 3), '\0');
            const int length = EVP_EncodeBlock(
                reinterpret_cast<unsigned char *>(&output[0]),
                reinterpret_cast<const unsigned char *>(input.data()),
                static_cast<int>(input.size()));
            output.resize(static_cast<size_t>(length));
            return output;
        }

        bool safeMailbox(const std::string &value)
        {
            return !value.empty() && value.size() <= 254 &&
                   value.find('@') != std::string::npos &&
                   value.find('\r') == std::string::npos &&
                   value.find('\n') == std::string::npos;
        }

        int connectTcp(const std::string &host, int port, std::string &errorOut)
        {
            addrinfo hints{};
            hints.ai_family = AF_UNSPEC;
            hints.ai_socktype = SOCK_STREAM;
            addrinfo *result = nullptr;
            const std::string service = std::to_string(port);
            if (getaddrinfo(host.c_str(), service.c_str(), &hints, &result) != 0)
            {
                errorOut = "smtp_dns_failed";
                return -1;
            }
            int fd = -1;
            for (addrinfo *it = result; it; it = it->ai_next)
            {
                fd = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
                if (fd >= 0 && connect(fd, it->ai_addr, it->ai_addrlen) == 0) break;
                if (fd >= 0) close(fd);
                fd = -1;
            }
            freeaddrinfo(result);
            if (fd < 0) errorOut = "smtp_connect_failed";
            return fd;
        }

        bool readResponse(SSL *ssl, int expected, std::string &errorOut)
        {
            std::string all;
            char buffer[1024];
            while (all.size() < 16384)
            {
                const int n = SSL_read(ssl, buffer, sizeof(buffer));
                if (n <= 0)
                {
                    errorOut = "smtp_read_failed";
                    return false;
                }
                all.append(buffer, static_cast<size_t>(n));
                size_t start = 0;
                while (true)
                {
                    const size_t end = all.find("\r\n", start);
                    if (end == std::string::npos) break;
                    const std::string line = all.substr(start, end - start);
                    if (line.size() >= 4 && line[3] == ' ')
                    {
                        int code = 0;
                        try { code = std::stoi(line.substr(0, 3)); } catch (...) {}
                        if (code != expected)
                        {
                            errorOut = "smtp_rejected_" + std::to_string(code);
                            return false;
                        }
                        return true;
                    }
                    start = end + 2;
                }
            }
            errorOut = "smtp_response_too_large";
            return false;
        }

        bool writeAll(SSL *ssl, const std::string &data, std::string &errorOut)
        {
            size_t sent = 0;
            while (sent < data.size())
            {
                const int n = SSL_write(ssl, data.data() + sent,
                                        static_cast<int>(data.size() - sent));
                if (n <= 0)
                {
                    errorOut = "smtp_write_failed";
                    return false;
                }
                sent += static_cast<size_t>(n);
            }
            return true;
        }

        bool command(SSL *ssl, const std::string &value, int expected,
                     std::string &errorOut)
        {
            return writeAll(ssl, value + "\r\n", errorOut) &&
                   readResponse(ssl, expected, errorOut);
        }
    }

    bool VerificationSender::sendEmailCode(const std::string &destination,
                                             const std::string &code,
                                             const std::string &purpose,
                                             std::string &errorOut)
    {
        if (!config_.email_enabled || !config_.smtp_use_tls)
        {
            errorOut = "smtp_not_configured";
            return false;
        }
        const std::string from = config_.smtp_from.empty()
            ? config_.smtp_username : config_.smtp_from;
        if (!safeMailbox(destination) || !safeMailbox(from) ||
            config_.smtp_from_name.find('\r') != std::string::npos ||
            config_.smtp_from_name.find('\n') != std::string::npos)
        {
            errorOut = "invalid_mailbox";
            return false;
        }

        int fd = connectTcp(config_.smtp_host, config_.smtp_port, errorOut);
        if (fd < 0) return false;
        SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
        SSL *ssl = ctx ? SSL_new(ctx) : nullptr;
        if (!ctx || !ssl)
        {
            errorOut = "smtp_tls_init_failed";
            if (ssl) SSL_free(ssl);
            if (ctx) SSL_CTX_free(ctx);
            close(fd);
            return false;
        }
        SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, nullptr);
        SSL_CTX_set_default_verify_paths(ctx);
        SSL_set_tlsext_host_name(ssl, config_.smtp_host.c_str());
        SSL_set1_host(ssl, config_.smtp_host.c_str());
        SSL_set_fd(ssl, fd);

        bool ok = SSL_connect(ssl) == 1;
        if (!ok) errorOut = "smtp_tls_connect_failed";
        if (ok) ok = readResponse(ssl, 220, errorOut);
        if (ok) ok = command(ssl, "EHLO hyperticket.local", 250, errorOut);
        if (ok) ok = command(ssl, "AUTH LOGIN", 334, errorOut);
        if (ok) ok = command(ssl, base64(config_.smtp_username), 334, errorOut);
        if (ok) ok = command(ssl, base64(config_.smtp_auth_code), 235, errorOut);
        if (ok) ok = command(ssl, "MAIL FROM:<" + from + ">", 250, errorOut);
        if (ok) ok = command(ssl, "RCPT TO:<" + destination + ">", 250, errorOut);
        if (ok) ok = command(ssl, "DATA", 354, errorOut);
        if (ok)
        {
            const std::string action = purpose == "PASSWORD_RESET"
                ? "reset your password"
                : (purpose == "CONTACT" ? "verify your contact information" : "verify your account");
            std::ostringstream message;
            message << "From: " << config_.smtp_from_name << " <" << from << ">\r\n"
                    << "To: <" << destination << ">\r\n"
                    << "Subject: HyperTicket verification code\r\n"
                    << "MIME-Version: 1.0\r\n"
                    << "Content-Type: text/html; charset=UTF-8\r\n"
                    << "Content-Transfer-Encoding: 8bit\r\n\r\n"
                    << "<div style=\"font-family:Arial,sans-serif;max-width:560px;margin:auto\">"
                    << "<h2>HyperTicket</h2><p>Use this code to " << action << ":</p>"
                    << "<p style=\"font-size:30px;font-weight:700;letter-spacing:6px\">"
                    << code << "</p><p>This code expires in 5 minutes. "
                    << "Do not share it with anyone.</p></div>\r\n.\r\n";
            ok = writeAll(ssl, message.str(), errorOut) && readResponse(ssl, 250, errorOut);
        }
        if (ok) command(ssl, "QUIT", 221, errorOut);
        SSL_shutdown(ssl);
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(fd);
        if (!ok) LOG_ERROR << "SMTP send failed: " << errorOut;
        return ok;
    }

    bool VerificationSender::sendMockSmsCode(const std::string &destination,
                                               const std::string &,
                                               const std::string &,
                                               std::string &errorOut)
    {
        if (!config_.mock_sms_enabled)
        {
            errorOut = "mock_sms_disabled";
            return false;
        }
        if (destination.size() != 11)
        {
            errorOut = "invalid_phone";
            return false;
        }
        return true;
    }
}
