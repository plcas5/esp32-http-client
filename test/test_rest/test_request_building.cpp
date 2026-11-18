#include <cstring>

#include "test_support.h"

class RequestBuildingTest : public RequestFixture {};

TEST_F(RequestBuildingTest, AddParamFormatting) {
  RestRequest& req = make("/sensor", HTTP_GET_METHOD);

  req.query("count", 42).query("ratio", 1.5f).query("enabled", true).query("name", "esp32");

  ASSERT_EQ(req._queryParams.size(), 4u) << "query should store 4 params";
  EXPECT_STREQ(req._queryParams[0].value.c_str(), "42") << "int should be formatted without quotes";
  EXPECT_FALSE(req._queryParams[0].quoteValue) << "int should not be quoted";

  EXPECT_STREQ(req._queryParams[1].value.c_str(), "1.5") << "float should use compact format";
  EXPECT_FALSE(req._queryParams[1].quoteValue) << "float should not be quoted";

  EXPECT_STREQ(req._queryParams[2].value.c_str(), "true") << "bool should serialize to true/false";
  EXPECT_FALSE(req._queryParams[2].quoteValue) << "bool should not be quoted";

  EXPECT_STREQ(req._queryParams[3].value.c_str(), "esp32") << "const char* should be copied to buffer";
  EXPECT_TRUE(req._queryParams[3].quoteValue) << "string should be quoted";
}

TEST_F(RequestBuildingTest, ExecuteBuildsUrlAndPayload) {
  HttpClientStub::setResponse(201, R"({"ok":true})");

  {
    RestRequest req = client.post("/users");
    req.query("tenant", 7).body("name", "Pedro").body("age", 21).body("active", true);
  }

  EXPECT_EQ(HttpClientStub::lastMethod, "POST") << "POST should call HTTPClient::POST";
  EXPECT_EQ(HttpClientStub::lastUrl, url("/users?tenant=7")) << "url should include base URL and query";
  EXPECT_EQ(HttpClientStub::lastPayload, "{\"name\":\"Pedro\",\"age\":21,\"active\":true}")
      << "payload should be JSON with proper quoting";
  EXPECT_EQ(client.getStatusCode(), 201) << "client should store last response code";
}

TEST_F(RequestBuildingTest, MethodAliases) {
  RestRequest& putReq = adopt(client.put("/resource"));
  EXPECT_EQ(putReq._method, HTTP_PUT_METHOD) << "put() should map to HTTP_PUT_METHOD";

  RestRequest& updateReq = adopt(client.update("/resource"));
  EXPECT_EQ(updateReq._method, HTTP_PUT_METHOD) << "update() should map to HTTP_PUT_METHOD";

  RestRequest& patchReq = adopt(client.patch("/resource"));
  EXPECT_EQ(patchReq._method, HTTP_PATCH_METHOD) << "patch() should map to HTTP_PATCH_METHOD";

  RestRequest& delReq = adopt(client.del("/resource"));
  EXPECT_EQ(delReq._method, HTTP_DELETE_METHOD) << "del() should map to HTTP_DELETE_METHOD";
}

TEST_F(RequestBuildingTest, AddParamAdvanced) {
  RestRequest& req = make("/test", HTTP_GET_METHOD);

  req.query("d", 3.14159265);
  req.body("f", 2.5f);

  ASSERT_EQ(req._queryParams.size(), 1u) << "query params size";
  ASSERT_EQ(req._bodyParams.size(), 1u) << "body params size";

  EXPECT_STREQ(req._queryParams[0].value.c_str(), "3.14159265") << "double formatting";
  EXPECT_STREQ(req._bodyParams[0].value.c_str(), "2.5") << "float formatting";
}

TEST_F(RequestBuildingTest, StringPathParams) {
  String route = "items";
  String idStr = "42";

  RestRequest& req = make("/{route}/{id}", HTTP_POST_METHOD);
  req.path("route", route).path("id", idStr);

  ASSERT_EQ(req._pathParams.size(), 2u);
  EXPECT_STREQ(req._pathParams[0].value.c_str(), "items");
  EXPECT_STREQ(req._pathParams[1].value.c_str(), "42");
}

TEST_F(RequestBuildingTest, QueryParamsStringAndNumericTypes) {
  RestRequest& req = make("/x", HTTP_GET_METHOD);
  req.query("filter", String("active"))
      .query("u_int", 500u)
      .query("u_long", 12345678UL)
      .query("i64", 9876543210LL)
      .query("u64", 18446744073709551615ULL);

  ASSERT_EQ(req._queryParams.size(), 5u);
  EXPECT_STREQ(req._queryParams[0].value.c_str(), "active");
  EXPECT_TRUE(req._queryParams[0].quoteValue) << "String must be quoted";
  EXPECT_STREQ(req._queryParams[1].value.c_str(), "500");
  EXPECT_STREQ(req._queryParams[2].value.c_str(), "12345678");
  EXPECT_STREQ(req._queryParams[3].value.c_str(), "9876543210");
  EXPECT_STREQ(req._queryParams[4].value.c_str(), "18446744073709551615");
}

TEST_F(RequestBuildingTest, BodyParamsStringAndNumericTypes) {
  String title = "asd";

  RestRequest& req = make("/x", HTTP_POST_METHOD);
  req.body("title", title)
      .body("desc", String("inline-string"))
      .body("u_int", 42u)
      .body("u_long", 99999999UL)
      .body("i64", 12345678901234LL)
      .body("u64", 99999999999999ULL);

  ASSERT_EQ(req._bodyParams.size(), 6u);
  EXPECT_STREQ(req._bodyParams[0].value.c_str(), "asd");
  EXPECT_TRUE(req._bodyParams[0].quoteValue) << "String must be quoted";
  EXPECT_STREQ(req._bodyParams[1].value.c_str(), "inline-string");
  EXPECT_TRUE(req._bodyParams[1].quoteValue) << "temporary String must be quoted";
  EXPECT_STREQ(req._bodyParams[2].value.c_str(), "42");
  EXPECT_FALSE(req._bodyParams[2].quoteValue);
  EXPECT_STREQ(req._bodyParams[3].value.c_str(), "99999999");
  EXPECT_FALSE(req._bodyParams[3].quoteValue);
  EXPECT_STREQ(req._bodyParams[4].value.c_str(), "12345678901234");
  EXPECT_FALSE(req._bodyParams[4].quoteValue);
  EXPECT_STREQ(req._bodyParams[5].value.c_str(), "99999999999999");
  EXPECT_FALSE(req._bodyParams[5].quoteValue);
}

TEST_F(RequestBuildingTest, LongParamValuesAreNotTruncated) {
  String longVal = "0123456789012345678901234567890123456789012345678901234567890123456789ABCDEFGHIJ";

  RestRequest& req = make("/x", HTTP_POST_METHOD);
  req.body("long", longVal).body("longc", longVal.c_str()).query("q", longVal);

  ASSERT_EQ(req._bodyParams.size(), 2u);
  ASSERT_EQ(req._queryParams.size(), 1u);
  EXPECT_STREQ(req._bodyParams[0].value.c_str(), longVal.c_str()) << "String body";
  EXPECT_STREQ(req._bodyParams[1].value.c_str(), longVal.c_str()) << "const char* body";
  EXPECT_STREQ(req._queryParams[0].value.c_str(), longVal.c_str()) << "query";
}

TEST_F(RequestBuildingTest, PostWithStringBodyParam) {
  HttpClientStub::setResponse(200, "{\"success\":true}");
  {
    String postTitle = "asd";
    client.post("/todos/1").body("title", postTitle);
  }

  EXPECT_EQ(HttpClientStub::lastMethod, "POST");
  EXPECT_EQ(HttpClientStub::lastUrl, url("/todos/1"));
  EXPECT_EQ(HttpClientStub::lastPayload, "{\"title\":\"asd\"}");
}

TEST_F(RequestBuildingTest, GetBodyIntoCharArray) {
  HttpClientStub::setResponse(200, "{\"name\":\"device-1\"}");
  char nameBuffer[32] = {0};

  client.get("/device").getBody("name", nameBuffer);

  EXPECT_STREQ(nameBuffer, "device-1");
}

TEST_F(RequestBuildingTest, QueryParametersInURL) {
  int id = 0;
  client.get("/users").query("page", 2).query("limit", 20).query("search", "pedro").getBody("id", &id);

  EXPECT_EQ(HttpClientStub::lastUrl, url("/users?page=2&limit=20&search=pedro")) << "query params built in url";

  HttpClientStub::reset();
  client.get("/sensors").query("active", true).query("threshold", 4.5f).getBody("id", &id);

  EXPECT_EQ(HttpClientStub::lastUrl, url("/sensors?active=true&threshold=4.5")) << "typed query params built in url";
}

TEST_F(RequestBuildingTest, PathParameters) {
  int id = 0;
  client.get("/users/{id}").path("id", 15).getBody("id", &id);

  EXPECT_EQ(HttpClientStub::lastUrl, url("/users/15")) << "path parameter {id} replaced with 15";

  HttpClientStub::reset();
  client.get("/users/{id}").path("{id}", 15).getBody("id", &id);

  EXPECT_EQ(HttpClientStub::lastUrl, url("/users/15")) << "path parameter with explicit {id} format";

  HttpClientStub::reset();
  client.get("/orgs/{org}/users/{userId}/posts/{postId}")
      .path("org", "google")
      .path("userId", 42)
      .path("postId", 101)
      .getBody("id", &id);

  EXPECT_EQ(HttpClientStub::lastUrl, url("/orgs/google/users/42/posts/101")) << "multiple path parameters";

  HttpClientStub::reset();
  client.get("/devices/{uuid}/telemetry").path("uuid", "123e4567-e89b-12d3-a456-426614174000").getBody("id", &id);

  EXPECT_EQ(HttpClientStub::lastUrl, url("/devices/123e4567-e89b-12d3-a456-426614174000/telemetry"))
      << "uuid path parameter";
}

TEST_F(RequestBuildingTest, PathAndQueryCombined) {
  int id = 0;
  client.get("/users/{id}/orders")
      .path("id", 15)
      .query("page", 2)
      .query("limit", 20)
      .query("search", "pedro")
      .getBody("id", &id);

  EXPECT_EQ(HttpClientStub::lastUrl, url("/users/15/orders?page=2&limit=20&search=pedro"))
      << "combined path and query params";
}

TEST_F(RequestBuildingTest, PathEdgeCases) {
  int id = 0;
  client.get("/static/path").path("none", 123).getBody("id", &id);

  EXPECT_EQ(HttpClientStub::lastUrl, url("/static/path")) << "path without placeholders unaffected";

  HttpClientStub::reset();
  client.get(nullptr).path("id", 10).getBody("id", &id);

  EXPECT_EQ(HttpClientStub::lastUrl, url()) << "null path handled safely";

  HttpClientStub::reset();
  {
    RestRequest req1(&client, "/users/{id}", HTTP_GET_METHOD);
    req1.path("id", 99);
    RestRequest req2(std::move(req1));
    req2.getBody("id", &id);
  }

  EXPECT_EQ(HttpClientStub::lastUrl, url("/users/99")) << "move constructor preserves path params";
}

TEST_F(RequestBuildingTest, ExecuteCustomPortWithPath) {
  HttpClientStub::setResponse(200, "{}");
  ESP32HTTPClient custom("http://test.com/api", 8080);
  custom.get("/data").getBody("x", (int*)nullptr);

  EXPECT_EQ(HttpClientStub::lastUrl, "http://test.com:8080/api/data") << "port injection with path";
}

TEST_F(RequestBuildingTest, ExecuteNullClient) {
  RestRequest req(nullptr, "/path", HTTP_GET_METHOD);
  req.execute();
  EXPECT_TRUE(req._executed) << "executed flag set for null client";
}

TEST_F(RequestBuildingTest, MoveConstructor) {
  RestRequest req1(&client, "/move", HTTP_GET_METHOD);
  req1.query("a", 1);

  RestRequest& req2 = adopt(std::move(req1));

  EXPECT_TRUE(req1._executed) << "req1 should be marked executed after move";
  EXPECT_EQ(req2._queryParams.size(), 1u) << "req2 should have params";
}

TEST_F(RequestBuildingTest, AllHttpMethods) {
  ESP32HTTPClient custom(kBaseUrl, 8080);

  custom.put("/").getBody("i", (int*)nullptr);
  EXPECT_EQ(HttpClientStub::lastMethod, "PUT") << "put uses PUT";

  custom.patch("/").getBody("i", (int*)nullptr);
  EXPECT_EQ(HttpClientStub::lastMethod, "PATCH") << "patch uses PATCH";

  custom.del("/").getBody("i", (int*)nullptr);
  EXPECT_EQ(HttpClientStub::lastMethod, "DELETE") << "del uses DELETE";
}
