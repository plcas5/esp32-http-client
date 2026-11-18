#include "test_support.h"

class ErrorToStringTest : public ::testing::TestWithParam<std::pair<int, const char*>> {};

TEST_P(ErrorToStringTest, ReturnsExpectedMessage) {
  const auto& param = GetParam();

  EXPECT_STREQ(ESP32HTTPClient::errorToString(param.first).c_str(), param.second);
}

INSTANTIATE_TEST_SUITE_P(
    ClientErrors, ErrorToStringTest,
    ::testing::Values(std::make_pair(-1, "Connection Refused"), std::make_pair(-2, "Send Header Failed"),
                      std::make_pair(-3, "Send Payload Failed"), std::make_pair(-4, "Not Connected"),
                      std::make_pair(-5, "Connection Lost"), std::make_pair(-6, "No Stream"),
                      std::make_pair(-7, "No HTTP Server"), std::make_pair(-8, "Too Less RAM"),
                      std::make_pair(-9, "Encoding Error"), std::make_pair(-10, "Stream Write Error"),
                      std::make_pair(-11, "Read Timeout"), std::make_pair(-99, "Unknown Client Error")),
    [](const ::testing::TestParamInfo<ErrorToStringTest::ParamType>& info) {
      return "Code" + std::to_string(-info.param.first);
    });

INSTANTIATE_TEST_SUITE_P(HttpStatuses, ErrorToStringTest,
                         ::testing::Values(std::make_pair(0, "Not Executed"), std::make_pair(200, "OK"),
                                           std::make_pair(201, "Created"), std::make_pair(202, "Accepted"),
                                           std::make_pair(204, "No Content"), std::make_pair(400, "Bad Request"),
                                           std::make_pair(401, "Unauthorized"), std::make_pair(403, "Forbidden"),
                                           std::make_pair(404, "Not Found"), std::make_pair(405, "Method Not Allowed"),
                                           std::make_pair(408, "Request Timeout"), std::make_pair(409, "Conflict"),
                                           std::make_pair(429, "Too Many Requests"),
                                           std::make_pair(500, "Internal Server Error"),
                                           std::make_pair(501, "Not Implemented"), std::make_pair(502, "Bad Gateway"),
                                           std::make_pair(503, "Service Unavailable"),
                                           std::make_pair(504, "Gateway Timeout")),
                         [](const ::testing::TestParamInfo<ErrorToStringTest::ParamType>& info) {
                           return "Status" + std::to_string(info.param.first);
                         });

// Fallback messages for status classes without a specific message
INSTANTIATE_TEST_SUITE_P(StatusClassFallbacks, ErrorToStringTest,
                         ::testing::Values(std::make_pair(299, "Success"), std::make_pair(302, "Redirection"),
                                           std::make_pair(418, "Client Error"), std::make_pair(599, "Server Error"),
                                           std::make_pair(600, "Unknown HTTP Status")),
                         [](const ::testing::TestParamInfo<ErrorToStringTest::ParamType>& info) {
                           return "Status" + std::to_string(info.param.first);
                         });

class ErrorHandlingTest : public RequestFixture {};

TEST_F(ErrorHandlingTest, HttpErrorStatus) {
  HttpClientStub::setResponse(404, R"({"error":"not found"})");

  int id = 0;
  client.get("/missing").getBody("id", &id);

  EXPECT_EQ(client.getStatusCode(), 404);
  EXPECT_TRUE(client.hasError());
  EXPECT_FALSE(client.isSuccess());
  EXPECT_STREQ(client.getErrorMessage().c_str(), "Not Found");
}

TEST_F(ErrorHandlingTest, ClientError) {
  HttpClientStub::setResponse(-1, "");

  int id = 0;
  client.get("/offline").retry(0).getBody("id", &id);

  EXPECT_EQ(client.getStatusCode(), -1);
  EXPECT_TRUE(client.hasError());
  EXPECT_FALSE(client.isSuccess());
  EXPECT_STREQ(client.getErrorMessage().c_str(), "Connection Refused");
}

TEST_F(ErrorHandlingTest, SuccessStatus) {
  HttpClientStub::setResponse(200, R"({"id":10})");

  int id = 0;
  client.get("/ok").getBody("id", &id);

  EXPECT_EQ(client.getStatusCode(), 200);
  EXPECT_FALSE(client.hasError());
  EXPECT_TRUE(client.isSuccess());
  EXPECT_STREQ(client.getErrorMessage().c_str(), "OK");
}

TEST_F(ErrorHandlingTest, RequestCallbacksOnSuccess) {
  HttpClientStub::setResponse(200, R"({"id":123})");

  int successCode = 0;
  int errCode = 0;
  int respCode = 0;
  int id = 0;

  client.get("/success")
      .onSuccess([&](int code) { successCode = code; })
      .onError([&](int code, const char*) { errCode = code; })
      .onResponse([&](int code) { respCode = code; })
      .getBody("id", &id);

  EXPECT_EQ(successCode, 200) << "onSuccess called on 200";
  EXPECT_EQ(respCode, 200) << "onResponse called on 200";
  EXPECT_EQ(errCode, 0) << "onError not called on 200";
  EXPECT_EQ(id, 123) << "body parsed on 200";
}

TEST_F(ErrorHandlingTest, RequestCallbacksOnError) {
  HttpClientStub::setResponse(500, R"({"error":"fail"})");

  int successCode = 0;
  int errCode = 0;
  int respCode = 0;
  std::string errMsg;
  int id = 0;

  client.get("/error")
      .onSuccess([&](int code) { successCode = code; })
      .onError([&](int code, const char* msg) {
        errCode = code;
        errMsg = msg;
      })
      .onResponse([&](int code) { respCode = code; })
      .getBody("id", &id);

  EXPECT_EQ(successCode, 0) << "onSuccess not called on 500";
  EXPECT_EQ(respCode, 500) << "onResponse called on 500";
  EXPECT_EQ(errCode, 500) << "onError called on 500";
  EXPECT_EQ(errMsg, "Internal Server Error") << "onError received the right message";
}

TEST_F(ErrorHandlingTest, ClientLevelCallbacks) {
  int clientSuccess = 0;
  int clientError = 0;
  int clientResponse = 0;
  client.onSuccess([&](int code) { clientSuccess = code; });
  client.onError([&](int code, const char*) { clientError = code; });
  client.onResponse([&](int code) { clientResponse = code; });

  HttpClientStub::setResponse(200, R"({"id":456})");
  int id = 0;
  client.get("/client-level").getBody("id", &id);

  EXPECT_EQ(clientSuccess, 200) << "client onSuccess triggered";
  EXPECT_EQ(clientResponse, 200) << "client onResponse triggered";
  EXPECT_EQ(clientError, 0) << "client onError not triggered on 200";
}

TEST_F(ErrorHandlingTest, ClientTwoParamOnError) {
  HttpClientStub::setResponse(404, "not found");

  int capturedCode = 0;
  std::string capturedMsg;
  client.onError([&](int code, const char* msg) {
    capturedCode = code;
    capturedMsg = msg ? msg : "";
  });

  client.get("/missing");

  EXPECT_EQ(capturedCode, 404);
  EXPECT_EQ(capturedMsg, "Not Found");
}

TEST_F(ErrorHandlingTest, ClientSingleParamOnError) {
  HttpClientStub::setResponse(500, "error");

  int errCode = 0;
  HttpResponseCallback callback = [&](int code) { errCode = code; };
  client.onError(callback);

  client.get("/test");

  EXPECT_EQ(errCode, 500) << "client single-param onError invoked";
}

TEST_F(ErrorHandlingTest, ClientSingleParamOnErrorCanBeCleared) {
  client.onError([](int) {});
  ASSERT_TRUE(!!client._onErrorCb) << "callback must be set before clearing it";

  client.onError(static_cast<HttpResponseCallback>(nullptr));

  EXPECT_TRUE(!client._onErrorCb) << "client onError cleared";
}

TEST_F(ErrorHandlingTest, RequestSingleParamOnError) {
  HttpClientStub::setResponse(500, "error");

  int errCode = 0;
  HttpResponseCallback callback = [&](int code) { errCode = code; };
  {
    RestRequest req = client.get("/test");
    req.onError(callback);
  }

  EXPECT_EQ(errCode, 500) << "request single-param onError invoked";
}

TEST_F(ErrorHandlingTest, RequestSingleParamOnErrorCanBeCleared) {
  RestRequest& req = make("/test", HTTP_GET_METHOD);
  req.onError([](int) {});
  ASSERT_TRUE(!!req._onErrorCb) << "callback must be set before clearing it";

  req.onError(static_cast<HttpResponseCallback>(nullptr));

  EXPECT_TRUE(!req._onErrorCb) << "request onError cleared";
}

TEST_F(ErrorHandlingTest, ObservabilityMetrics) {
  bool callbackFired = false;
  client.onObservability([&callbackFired](const ObservabilityMetrics& metrics) {
    callbackFired = true;
    EXPECT_GE(static_cast<long long>(metrics.totalTimeMs), 0LL) << "total time should be tracked";
    EXPECT_GE(static_cast<long long>(metrics.ttfbMs), 0LL) << "TTFB should be tracked";
    EXPECT_GT(static_cast<long long>(metrics.txBytes), 0LL) << "TX bytes should be > 0";
    EXPECT_EQ(static_cast<long long>(metrics.rxBytes), 11LL) << "RX bytes should match response size";
    EXPECT_EQ(static_cast<long long>(metrics.retries), 0LL) << "retries should be 0";
    EXPECT_GT(static_cast<long long>(metrics.freeHeapBefore), 0LL) << "free heap before should be > 0";
    EXPECT_GT(static_cast<long long>(metrics.freeHeapAfter), 0LL) << "free heap after should be > 0";
  });

  HttpClientStub::setResponse(200, R"({"ok":true})");
  client.get("/api").execute();

  EXPECT_TRUE(callbackFired) << "observability callback should have been fired";
}