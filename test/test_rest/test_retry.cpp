#include "test_support.h"

class RetryTest : public RequestFixture {};

TEST_F(RetryTest, AutoRetry) {
  HttpClientStub::setResponse(-1, "");  // first attempt fails

  ESP32HTTPClient client("http://retry");
  client.get("/").getBody("i", (int*)nullptr);

  EXPECT_EQ(client.getStatusCode(), -1) << "status code propagates from retry loop";
}

TEST_F(RetryTest, MaxRetry) {
  EXPECT_EQ(client.getMaxRetry(), 1) << "default maxRetry is 1";

  client.setMaxRetry(3);
  EXPECT_EQ(client.getMaxRetry(), 3) << "custom maxRetry";

  HttpClientStub::reset();
  HttpClientStub::queueResponse(-1, "");
  HttpClientStub::queueResponse(-1, "");
  HttpClientStub::queueResponse(200, "{\"success\":true}");

  bool success = false;
  client.get("/retry-ok").getBody("success", &success);
  EXPECT_TRUE(success) << "succeeded after retrying";
  EXPECT_EQ(HttpClientStub::requestCount, 3) << "made 3 attempts";
  EXPECT_EQ(client.getStatusCode(), 200) << "status code 200 after recovery";

  HttpClientStub::reset();
  HttpClientStub::queueResponse(-1, "");
  HttpClientStub::queueResponse(-1, "");
  HttpClientStub::queueResponse(200, "{\"success\":true}");

  success = false;
  client.get("/retry-fail").retry(0).getBody("success", &success);
  EXPECT_TRUE(!success) << "failed when retry is 0";
  EXPECT_EQ(HttpClientStub::requestCount, 1) << "only 1 attempt when retry is 0";
  EXPECT_EQ(client.getStatusCode(), -1) << "status code is -1";
}

TEST_F(RetryTest, RetryWithClientHeadersAndBody) {
  HttpClientStub::queueResponse(-1, "");
  HttpClientStub::queueResponse(200, "{\"ok\":true}");

  ESP32HTTPClient client("https://example.com");
  client.setHeader("X-Custom", "Val");
  client.setContentType("application/json");

  {
    RestRequest req = client.post("/items");
    req.maxRetry(1);
    req.body("name", "item1");
  }

  EXPECT_EQ(client.getStatusCode(), 200) << "status code 200 after retry";
  EXPECT_EQ(HttpClientStub::requestCount, 2) << "2 attempts made";
}