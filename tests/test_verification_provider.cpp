#include "../backend/Server/include/VerificationProvider.hpp"
#include "../backend/Domain/include/ServiceUtil.hpp"

#include <jsoncpp/json/json.h>

#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace
{
    int failures = 0;

    void expect(bool condition, const char *message)
    {
        if (!condition)
        {
            std::cerr << "FAIL: " << message << '\n';
            ++failures;
        }
    }
}

int main()
{
    const std::string path = "/tmp/hyperticket-verification-inbox-" +
                             std::to_string(static_cast<long long>(getpid())) + ".jsonl";
    std::remove(path.c_str());

    hyperticket::VerificationConfig config;
    config.development_inbox_enabled = true;
    config.development_inbox_path = path;
    hyperticket::VerificationProvider provider(config);

    std::string error;
    expect(provider.sendCode("EMAIL", "user@example.com", "123456", "REGISTER", error),
           "development provider should write email code");
    expect(error.empty(), "successful delivery should not set an error");

    struct stat info{};
    expect(stat(path.c_str(), &info) == 0, "development inbox should exist");
    expect((info.st_mode & 0777) == 0600, "development inbox should be mode 0600");

    std::ifstream input(path);
    std::string line;
    std::getline(input, line);
    Json::Value record;
    Json::CharReaderBuilder reader;
    std::string parseError;
    std::istringstream stream(line);
    expect(Json::parseFromStream(reader, stream, &record, &parseError),
           "development inbox entry should be valid JSON");
    expect(record["channel"].asString() == "EMAIL", "channel should be recorded");
    expect(record["destination"].asString() == "user@example.com", "destination should be recorded");
    expect(record["purpose"].asString() == "REGISTER", "purpose should be recorded");
    expect(record["code"].asString() == "123456", "code should be available to tests");

    const std::string hash = hyperticket::hashPassword("123456");
    expect(provider.verifyCode("123456", hash), "provider should verify the correct code");
    expect(!provider.verifyCode("654321", hash), "provider should reject an incorrect code");

    error.clear();
    expect(!provider.sendCode("PUSH", "device", "123456", "REGISTER", error),
           "unsupported channel should be rejected");

    std::remove(path.c_str());
    if (failures == 0) std::cout << "verification provider tests passed\n";
    return failures == 0 ? 0 : 1;
}
