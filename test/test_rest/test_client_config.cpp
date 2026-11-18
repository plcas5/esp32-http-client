#include "test_support.h"

class ClientConfigTest : public RequestFixture {};

TEST_F(ClientConfigTest, ClientConfiguration) {
  client.setContentType("text/plain");
  client.setHeader("X-Custom", "value1");

  HttpClientStub::reset();
  client.post("/upload").body("data", "hello").getBody("ignored", (int*)nullptr);

  EXPECT_EQ(HttpClientStub::lastUrl, "https://example.com/upload") << "url matches";
  EXPECT_EQ(HttpClientStub::lastMethod, "POST") << "method matches";

  client.end();  // covers the end() method
}

TEST_F(ClientConfigTest, RuntimeUrlChange) {
  ESP32HTTPClient client("https://api.initial.com", 80);
  EXPECT_EQ(client.getBaseUrl(), "https://api.initial.com") << "initial baseUrl";
  EXPECT_EQ(client.getPort(), 80) << "initial port";

  HttpClientStub::reset();
  int id = 0;
  client.get("/users").getBody("id", &id);
  EXPECT_EQ(HttpClientStub::lastUrl, "https://api.initial.com:80/users") << "request to initial url";

  client.setBaseUrl("https://api.newhost.org", 443);
  EXPECT_EQ(client.getBaseUrl(), "https://api.newhost.org") << "updated baseUrl";
  EXPECT_EQ(client.getPort(), 443) << "updated port";

  HttpClientStub::reset();
  client.get("/v1/data").getBody("id", &id);
  EXPECT_EQ(HttpClientStub::lastUrl, "https://api.newhost.org:443/v1/data") << "request to updated url and port";

  client.setUrl("http://192.168.1.50");
  client.setPort(8080);
  HttpClientStub::reset();
  client.get("/sensors").getBody("id", &id);
  EXPECT_EQ(HttpClientStub::lastUrl, "http://192.168.1.50:8080/sensors") << "request after setUrl and setPort";
}

TEST_F(ClientConfigTest, Timeout) {
  EXPECT_EQ(client.getTimeout(), 60000) << "default timeout is 60000ms (1 minute)";

  client.setTimeout(8000);
  EXPECT_EQ(client.getTimeout(), 8000) << "custom client timeout";

  HttpClientStub::reset();
  int id = 0;
  client.get("/data").getBody("id", &id);
  EXPECT_EQ(HttpClientStub::lastTimeout, 8000) << "client timeout applied to request";

  HttpClientStub::reset();
  client.get("/quick").timeout(1500).getBody("id", &id);
  EXPECT_EQ(HttpClientStub::lastTimeout, 1500) << "per-request timeout overrides client timeout";
}

TEST_F(ClientConfigTest, FollowRedirects) {
  EXPECT_EQ(client.getRedirectLimit(), 10) << "default redirect limit is 10";
  EXPECT_EQ(HttpClientStub::followRedirects, HTTPC_DISABLE_FOLLOW_REDIRECTS)
      << "HTTPClient redirects are disabled by default";
  EXPECT_EQ(HttpClientStub::redirectLimit, 10) << "HTTPClient default redirect limit is 10";

  client.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  client.setRedirectLimit(4);

  EXPECT_EQ(HttpClientStub::followRedirects, HTTPC_STRICT_FOLLOW_REDIRECTS)
      << "setFollowRedirects forwards strict mode";
  EXPECT_EQ(client.getRedirectLimit(), 4) << "getRedirectLimit returns the limit";
  EXPECT_EQ(HttpClientStub::redirectLimit, 4) << "setRedirectLimit forwards the limit";

  client.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  client.setRedirectLimit(0);

  EXPECT_EQ(HttpClientStub::followRedirects, HTTPC_FORCE_FOLLOW_REDIRECTS) << "setFollowRedirects forwards force mode";
  EXPECT_EQ(HttpClientStub::redirectLimit, 0) << "HTTPClient redirect limit accepts zero";
}