#include "test_support.h"

class HeadersTest : public RequestFixture {
 protected:
  // Stubs a response whose only interesting part is the `token` header.
  static void respondWithToken(int status, const char* body, const char* token) {
    HttpClientStub::setResponse(status, body);
    HttpClientStub::setResponseHeaders({{"token", token}});
  }
};

TEST_F(HeadersTest, HeaderString) {
  HttpClientStub::setResponse(200, "{\"status\":\"ok\"}");
  HttpClientStub::setResponseHeaders({{"token", "\"33a64df551425fcc55e4d42a148795d9f25f89f4\""},
                                      {"Content-Type", "application/json; charset=utf-8"},
                                      {"X-Server-Id", "srv-node-01"}});

  String token;
  String contentType;
  String serverId;

  client.get("/resource")
      .getHeader("token", &token)
      .getHeader("Content-Type", &contentType)
      .getHeader("X-Server-Id", &serverId);

  EXPECT_STREQ(token.c_str(), "\"33a64df551425fcc55e4d42a148795d9f25f89f4\"") << "getHeader token String";
  EXPECT_STREQ(contentType.c_str(), "application/json; charset=utf-8") << "getHeader Content-Type String";
  EXPECT_STREQ(serverId.c_str(), "srv-node-01") << "getHeader custom header String";
}

TEST_F(HeadersTest, CharArray) {
  HttpClientStub::setResponse(200, "{}");
  HttpClientStub::setResponseHeaders({{"token", "W/\"123456789\""}, {"X-Auth-Token", "secret-token-value"}});

  char tokenArr[32] = {0};
  char token[64] = {0};

  client.get("/item").getHeader("token", tokenArr).getHeader("X-Auth-Token", token, sizeof(token));

  EXPECT_STREQ(tokenArr, "W/\"123456789\"") << "getHeader char array reference";
  EXPECT_STREQ(token, "secret-token-value") << "getHeader char pointer with max size";
}

TEST_F(HeadersTest, NumericAndBool) {
  HttpClientStub::setResponse(200, "{}");
  HttpClientStub::setResponseHeaders({{"X-Rate-Limit-Remaining", "42"},
                                      {"X-Timestamp", "1710000050"},
                                      {"X-Temperature", "25.75"},
                                      {"X-Factor", "1.234567"},
                                      {"X-Is-Cached", "true"},
                                      {"X-Is-Admin", "1"}});

  int remaining = 0;
  long timestamp = 0;
  float temp = 0.0f;
  double factor = 0.0;
  bool isCached = false;
  bool isAdmin = false;

  client.get("/status")
      .getHeader("X-Rate-Limit-Remaining", &remaining)
      .getHeader("X-Timestamp", &timestamp)
      .getHeader("X-Temperature", &temp)
      .getHeader("X-Factor", &factor)
      .getHeader("X-Is-Cached", &isCached)
      .getHeader("X-Is-Admin", &isAdmin);

  EXPECT_EQ(remaining, 42) << "getHeader int parsed";
  EXPECT_EQ(timestamp, 1710000050L) << "getHeader long parsed";
  EXPECT_NEAR(temp, 25.75f, 0.01f) << "getHeader float parsed";
  EXPECT_NEAR(factor, 1.234567, 0.00001) << "getHeader double parsed";
  EXPECT_TRUE(isCached) << "getHeader bool true string";
  EXPECT_TRUE(isAdmin) << "getHeader bool 1 string";
}

TEST_F(HeadersTest, CaseInsensitiveAndMissing) {
  HttpClientStub::setResponse(200, "{}");
  HttpClientStub::setResponseHeaders({{"token", "token-lowercase-val"}});

  String token;
  String missing = "default_val";
  int missingInt = 999;

  client.get("/data")
      .getHeader("TOKEN", &token)
      .getHeader("X-Missing-Header", &missing)
      .getHeader("X-Missing-Int", &missingInt);

  EXPECT_STREQ(token.c_str(), "token-lowercase-val") << "getHeader case-insensitive match";
  EXPECT_STREQ(missing.c_str(), "default_val") << "missing header leaves string untouched";
  EXPECT_EQ(missingInt, 999) << "missing header leaves int untouched";
}

TEST_F(HeadersTest, CombinedWithGetBody) {
  HttpClientStub::setResponse(200, "{\"id\":101,\"name\":\"Sensor Node\"}");
  HttpClientStub::setResponseHeaders({{"token", "\"token-abc\""}, {"X-Server", "nginx"}});

  int id = 0;
  char name[32] = {0};
  String token;
  String server;

  client.post("/todos/1")
      .body("title", "Check status")
      .getBody("id", &id)
      .getBody("name", name, sizeof(name))
      .getHeader("token", &token)
      .getHeader("X-Server", &server);

  EXPECT_EQ(id, 101) << "getBody id extracted alongside headers";
  EXPECT_STREQ(name, "Sensor Node") << "getBody name extracted alongside headers";
  EXPECT_STREQ(token.c_str(), "\"token-abc\"") << "getHeader token extracted with getBody";
  EXPECT_STREQ(server.c_str(), "nginx") << "getHeader server extracted with getBody";
}

TEST_F(HeadersTest, Retries) {
  HttpClientStub::queueResponse(HTTPC_ERROR_CONNECTION_LOST, "");
  HttpClientStub::queueResponseHeaders({});

  HttpClientStub::queueResponse(200, "{\"success\":true}");
  HttpClientStub::queueResponseHeaders({{"X-Retry-Header", "after-retry-value"}});

  String headerVal;
  bool success = false;

  client.get("/retry-target").retry(2).getHeader("X-Retry-Header", &headerVal).getBody("success", &success);

  EXPECT_TRUE(success) << "request succeeded on retry";
  EXPECT_STREQ(headerVal.c_str(), "after-retry-value") << "header parsed on retry";
  EXPECT_EQ(HttpClientStub::requestCount, 2) << "2 attempts made";
}

TEST_F(HeadersTest, MoveConstructor) {
  HttpClientStub::setResponse(200, "{\"msg\":\"ok\"}");
  HttpClientStub::setResponseHeaders({{"token", "moved-token"}});

  String token;
  String msg;

  {
    RestRequest req1 = client.get("/moved");
    req1.getHeader("token", &token).getBody("msg", &msg);
    RestRequest req2 = std::move(req1);
  }

  EXPECT_STREQ(token.c_str(), "moved-token") << "header binding moved and executed";
  EXPECT_STREQ(msg.c_str(), "ok") << "body binding moved and executed";
}

TEST_F(HeadersTest, OnPost) {
  respondWithToken(201, "{\"id\":1}", "post-token-1");

  String token;
  client.post("/items").body("name", "widget").getHeader("token", &token);

  EXPECT_STREQ(token.c_str(), "post-token-1") << "getHeader on POST";
  EXPECT_EQ(HttpClientStub::lastMethod, "POST");
}

TEST_F(HeadersTest, OnPut) {
  respondWithToken(200, "{\"id\":1}", "put-token-2");

  String token;
  client.put("/items/1").body("name", "widget-updated").getHeader("token", &token);

  EXPECT_STREQ(token.c_str(), "put-token-2") << "getHeader on PUT";
  EXPECT_EQ(HttpClientStub::lastMethod, "PUT");
}

TEST_F(HeadersTest, OnPatch) {
  respondWithToken(200, "{\"id\":1}", "patch-token-3");

  String token;
  client.patch("/items/1").body("name", "widget-patched").getHeader("token", &token);

  EXPECT_STREQ(token.c_str(), "patch-token-3") << "getHeader on PATCH";
  EXPECT_EQ(HttpClientStub::lastMethod, "PATCH");
}

TEST_F(HeadersTest, OnDelete) {
  respondWithToken(200, "{\"deleted\":true}", "delete-token-4");

  String token;
  client.del("/items/1").getHeader("token", &token);

  EXPECT_STREQ(token.c_str(), "delete-token-4") << "getHeader on DELETE";
  EXPECT_EQ(HttpClientStub::lastMethod, "DELETE");
}

TEST_F(HeadersTest, OnUpdateAlias) {
  respondWithToken(200, "{\"id\":1}", "update-token-5");

  String token;
  client.update("/items/1").body("name", "widget-updated-alias").getHeader("token", &token);

  EXPECT_STREQ(token.c_str(), "update-token-5") << "getHeader on update alias";
  EXPECT_EQ(HttpClientStub::lastMethod, "PUT") << "update() calls PUT";
}