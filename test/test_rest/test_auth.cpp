#include "test_support.h"

class AuthTest : public RequestFixture {};

TEST_F(AuthTest, AuthBearer) {
  client.bearer("eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9");

  EXPECT_EQ(client._headers.size(), 1u) << "headers size after bearer";
  EXPECT_STREQ(client._headers[0].name, "Authorization") << "header name should be Authorization";
  EXPECT_STREQ(client._headers[0].value, "Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9") << "bearer token format";

  int id = 0;
  client.get("/secure").getBody("id", &id);

  bool found = false;
  for (const auto& h : HttpClientStub::lastHeaders) {
    if (h.first == "Authorization" && h.second == "Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9") {
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found) << "Authorization header should be sent with request";
}

TEST_F(AuthTest, AuthBearerOverwrite) {
  client.bearer("new-token-456");

  EXPECT_EQ(client._headers.size(), 1u) << "bearer overwrite should not duplicate header";
  EXPECT_STREQ(client._headers[0].value, "Bearer new-token-456") << "bearer header updated";
}

TEST_F(AuthTest, AuthBasic) {
  client.basic("admin", "123456");

  EXPECT_EQ(client._headers.size(), 1u) << "headers size after basic";
  EXPECT_STREQ(client._headers[0].name, "Authorization") << "header name should be Authorization";
  EXPECT_STREQ(client._headers[0].value, "Basic YWRtaW46MTIzNDU2") << "basic auth encoding for admin:123456";

  int id = 0;
  client.get("/admin").getBody("id", &id);

  bool found = false;
  for (const auto& h : HttpClientStub::lastHeaders) {
    if (h.first == "Authorization" && h.second == "Basic YWRtaW46MTIzNDU2") {
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found) << "Basic Authorization header should be sent with request";

  client.basic("user", "pass");
  EXPECT_STREQ(client._headers[0].value, "Basic dXNlcjpwYXNz") << "basic auth encoding for user:pass";

  client.basic("Aladdin", "open sesame");
  EXPECT_STREQ(client._headers[0].value, "Basic QWxhZGRpbjpvcGVuIHNlc2FtZQ==")
      << "basic auth encoding for Aladdin:open sesame";
}

TEST_F(AuthTest, AuthApiKey) {
  client.apiKey("x-api-key", "secret-key-123");

  EXPECT_EQ(client._headers.size(), 1u) << "headers size after apiKey";
  EXPECT_STREQ(client._headers[0].name, "x-api-key") << "api key header name";
  EXPECT_STREQ(client._headers[0].value, "secret-key-123") << "api key header value";

  int val = 0;
  client.get("/api/v1").getBody("v", &val);

  bool found = false;
  for (const auto& h : HttpClientStub::lastHeaders) {
    if (h.first == "x-api-key" && h.second == "secret-key-123") {
      found = true;
      break;
    }
  }
  EXPECT_TRUE(found) << "API key header should be sent with request";

  client.apiKey("x-api-key", "new-secret-456");
  EXPECT_EQ(client._headers.size(), 1u) << "apiKey overwrite should not duplicate header";
  EXPECT_STREQ(client._headers[0].value, "new-secret-456") << "apiKey header updated";
}

TEST_F(AuthTest, AuthCookie) {
  client.cookie("session_id", "abc123").cookie("device_id", "esp32-01");

  EXPECT_EQ(client._headers.size(), 1u) << "headers size after multiple cookies should be 1 (appended)";
  EXPECT_STREQ(client._headers[0].name, "Cookie") << "cookie header name";
  EXPECT_STREQ(client._headers[0].value, "session_id=abc123; device_id=esp32-01")
      << "cookie header value correctly formatted";

  int val = 0;
  client.get("/api/v1").getBody("v", &val);

  bool found = false;
  for (const auto& h : HttpClientStub::lastHeaders) {
    if (h.first == "Cookie" && h.second == "session_id=abc123; device_id=esp32-01") {
      found = true;
      break;
    }
  }

  EXPECT_TRUE(found) << "Cookie header should be sent with request";
}

TEST_F(AuthTest, CookieNullParams) {
  client.cookie(nullptr, "value");
  client.cookie("name", nullptr);
  client.cookie(nullptr, nullptr);

  EXPECT_EQ(client._headers.size(), 0) << "null cookie name/value should not add header";
}

TEST_F(AuthTest, AuthEdgeCases) {
  client.bearer(nullptr);
  EXPECT_STREQ(client._headers[0].value, "Bearer ") << "bearer with null token";

  client.basic(nullptr, nullptr);
  EXPECT_STREQ(client._headers[0].value, "Basic Og==") << "basic with null creds";

  client.apiKey("X-Custom-Key", nullptr);
  EXPECT_STREQ(client._headers[1].value, "") << "apiKey with null key";
}

TEST_F(AuthTest, Base64EncodeEdgeCases) {

  // Length % 3 == 2: "use:p" -> 5 characters -> "dXNlOnA="
  client.basic("use", "p");
  EXPECT_EQ(client._headers.size(), 1u) << "basic auth header added for len % 3 == 2";
  EXPECT_STREQ(client._headers[0].value, "Basic dXNlOnA=") <<"base64 encoding with len % 3 == 2";

  // Length % 3 == 1: "a:bc" -> 4 characters -> "YTpiYw=="
  client.basic("a", "bc");
  EXPECT_STREQ(client._headers[0].value, "Basic YTpiYw==") << "base64 encoding with len % 3 == 1";

  // Length % 3 == 0: "ab:def" -> 6 characters -> "YWI6ZGVm"
  client.basic("ab", "def");
  EXPECT_STREQ(client._headers[0].value, "Basic YWI6ZGVm") << "base64 encoding with len % 3 == 0";
}