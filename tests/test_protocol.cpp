// Protocol 契约层测试：makeOk/makeError 结构、字段常量、toJsonLine 往返、枚举值。
#include "test_util.hpp"
#include "../backend/Common/include/Protocol.hpp"

#include <jsoncpp/json/json.h>
#include <algorithm>
#include <sstream>

using namespace hyperticket;

static void test_make_ok()
{
    Json::Value ok = makeOk();
    CHECK_STR_EQ(ok[field::kStatus].asString(), status::kOk);
    CHECK(!ok.isMember(field::kReason));
}

static void test_make_error()
{
    Json::Value e = makeError("BOOM");
    CHECK_STR_EQ(e[field::kStatus].asString(), status::kErr);
    CHECK_STR_EQ(e[field::kReason].asString(), "BOOM");
}

static void test_enum_values()
{
    // 与历史协议保持一致（client/server 共享同一组值）。
    CHECK_EQ((int)LOGIN, 1);
    CHECK_EQ((int)REGISTER, 2);
    CHECK_EQ((int)EXIT, 3);
    CHECK_EQ((int)VIEW, 4);
    CHECK_EQ((int)ORDER, 5);
    CHECK_EQ((int)VIEW_MY, 6);
    CHECK_EQ((int)CANCEL, 7);
}

static void test_json_line_roundtrip()
{
    Json::Value v = makeOk();
    v[field::kToken] = "abc123";
    std::string line = toJsonLine(v);
    // 必须以换行结尾，且只有一行
    CHECK(!line.empty() && line.back() == '\n');
    CHECK_EQ((int)std::count(line.begin(), line.end(), '\n'), 1);

    // 解析回来字段一致
    Json::CharReaderBuilder b;
    Json::Value parsed;
    std::string errs;
    std::istringstream iss(line);
    CHECK(Json::parseFromStream(b, iss, &parsed, &errs));
    CHECK_STR_EQ(parsed[field::kStatus].asString(), status::kOk);
    CHECK_STR_EQ(parsed[field::kToken].asString(), "abc123");
}

int main()
{
    RUN_TEST(test_make_ok);
    RUN_TEST(test_make_error);
    RUN_TEST(test_enum_values);
    RUN_TEST(test_json_line_roundtrip);
    return TEST_SUMMARY();
}
