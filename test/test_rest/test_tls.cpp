#include "test_support.h"

namespace {
constexpr long kClockUnsynced = 100L;
constexpr long kClockSynced = 1700000000L;
constexpr const char* kDummyCA = "-----BEGIN CERTIFICATE-----\nMIIB...\n-----END CERTIFICATE-----\n";
}  // namespace

class TlsDefaultTest : public RequestFixture {};

TEST_F(TlsDefaultTest, HttpDoesNotUseSecureClient) {
  ESP32HTTPClient c("http://example.com");
  EXPECT_FALSE(c.hasTlsConfig()) << "Initial client should not have TLS config";

  HttpClientStub::setResponse(200, "{\"ok\":true}");
  c.get("/ping").execute();

  EXPECT_FALSE(HttpClientStub::usedSecureClient) << "http:// must not use secure client";
  EXPECT_EQ(HttpClientStub::lastUrl, "http://example.com/ping") << "Url should match";
}

TEST_F(TlsDefaultTest, HttpsWithoutConfigKeepsLegacyBegin) {
  EXPECT_FALSE(client.hasTlsConfig()) << "HTTPS client without TLS config should have hasTlsConfig == false";

  HttpClientStub::setResponse(200, "{\"ok\":true}");
  client.get("/secure").execute();

  EXPECT_FALSE(HttpClientStub::usedSecureClient)
      << "https:// without TLS config must maintain retrocompatible begin(url)";
  EXPECT_EQ(HttpClientStub::lastUrl, url("/secure")) << "Url should match";
}

TEST_F(TlsDefaultTest, HttpsWithoutConfigHasNoValidatedTls) {
  EXPECT_FALSE(client.hasValidatedTlsConfig()) << "hasValidatedTlsConfig initially false";
  EXPECT_FALSE(client.isValidatedTlsRequired()) << "requireValidatedTls defaults to false";
}

class TlsConfigTest : public RequestFixture {};

TEST_F(TlsConfigTest, SetCACertConfiguresSecureClient) {
  ESP32HTTPClient c("https://secure.example.com");
  c.setCACert(kDummyCA);

  EXPECT_TRUE(c.hasTlsConfig()) << "Client should have TLS configured after setCACert";
  EXPECT_EQ(NetworkClientSecureStub::lastCACert, kDummyCA) << "NetworkClientSecure should receive root CA pointer";
  EXPECT_FALSE(NetworkClientSecureStub::isInsecure) << "Secure client should not be insecure";
}

TEST_F(TlsConfigTest, HttpsWithCAInjectsSecureClient) {
  ESP32HTTPClient c("https://secure.example.com");
  c.setCACert(kDummyCA);

  HttpClientStub::setResponse(200, "{\"status\":\"secure\"}");
  c.get("/data").execute();

  EXPECT_TRUE(HttpClientStub::usedSecureClient) << "https:// with CA configured must inject secure client";
  EXPECT_NE(HttpClientStub::lastSecureClient, nullptr) << "Injected secure client pointer must not be null";
}

TEST_F(TlsConfigTest, HttpIgnoresSecureClientEvenWithCA) {
  ESP32HTTPClient c("https://secure.example.com");
  c.setCACert(kDummyCA);
  c.setUrl("http://plain.example.com");

  HttpClientStub::setResponse(200, "{\"status\":\"plain\"}");
  c.get("/data").execute();

  EXPECT_FALSE(HttpClientStub::usedSecureClient) << "http:// must not use secure client even if CA is configured";
}

TEST_F(TlsConfigTest, SetCACertBundlePassesPointerAndSize) {
  const uint8_t bundle[] = {0x01, 0x02, 0x03, 0x04};
  ESP32HTTPClient c("https://api.example.com");
  c.setCACertBundle(bundle, sizeof(bundle));

  EXPECT_TRUE(c.hasTlsConfig()) << "Bundle configures TLS";
  EXPECT_TRUE(NetworkClientSecureStub::lastBundle == bundle) << "Bundle pointer passed to secure client";
  EXPECT_EQ(NetworkClientSecureStub::lastBundleSize, sizeof(bundle)) << "Bundle size passed";
  EXPECT_FALSE(NetworkClientSecureStub::isInsecure) << "Bundle is not insecure";
}

TEST_F(TlsConfigTest, SetClientCertPassesCertAndKey) {
  const char* cert = "CLIENT_CERT_PEM";
  const char* key = "CLIENT_KEY_PEM";
  ESP32HTTPClient c("https://api.example.com");
  c.setClientCert(cert, key);

  EXPECT_EQ(NetworkClientSecureStub::lastCert, cert) << "Client cert passed";
  EXPECT_EQ(NetworkClientSecureStub::lastKey, key) << "Client key passed";
}

TEST_F(TlsConfigTest, SetInsecureClearsCAAndBundle) {
  const uint8_t bundle[] = {0x01, 0x02};
  ESP32HTTPClient c("https://api.example.com");
  c.setCACert(kDummyCA);
  c.setCACertBundle(bundle, sizeof(bundle));
  c.setInsecure();

  EXPECT_TRUE(c.hasTlsConfig()) << "Insecure configures TLS";
  EXPECT_TRUE(NetworkClientSecureStub::isInsecure) << "Secure client set to insecure";
  EXPECT_TRUE(NetworkClientSecureStub::lastCACert == nullptr) << "CA cert cleared on insecure";
  EXPECT_TRUE(NetworkClientSecureStub::lastBundle == nullptr) << "Bundle cleared on insecure";
}

TEST_F(TlsConfigTest, SetCACertOverridesPreviousInsecure) {
  const char* ca = "ROOT_CA_2";
  ESP32HTTPClient c("https://api.example.com");
  c.setInsecure();
  c.setCACert(ca);

  EXPECT_FALSE(NetworkClientSecureStub::isInsecure) << "setCACert must clear insecure flag";
  EXPECT_EQ(NetworkClientSecureStub::lastCACert, ca) << "New CA cert set";
}

TEST_F(TlsConfigTest, SetCACertBundleOverridesPreviousInsecure) {
  const uint8_t bundle[] = {0x01, 0x02};
  ESP32HTTPClient c("https://api.example.com");
  c.setInsecure();
  c.setCACertBundle(bundle, sizeof(bundle));

  EXPECT_FALSE(NetworkClientSecureStub::isInsecure) << "setCACertBundle must clear insecure flag";
  EXPECT_TRUE(NetworkClientSecureStub::lastBundle == bundle) << "New bundle set";
}

TEST_F(TlsConfigTest, SetInsecureOverridesPreviousCACert) {
  ESP32HTTPClient c("https://api.example.com");
  c.setCACert(kDummyCA);
  c.setInsecure();

  EXPECT_TRUE(NetworkClientSecureStub::isInsecure) << "setInsecure must override previous CA";
  EXPECT_TRUE(NetworkClientSecureStub::lastCACert == nullptr) << "CA must be cleared";
}

TEST_F(TlsConfigTest, ApplyCACertBundleFallbackOneArg) {
  struct MockBundle1Arg {
    const uint8_t* b = nullptr;
    void setCACertBundle(const uint8_t* bundle) {
      b = bundle;
    }
  };
  const uint8_t bundle[] = {0x01, 0x02, 0x03, 0x04};
  MockBundle1Arg m;

  EXPECT_TRUE(esp32_http_client_detail::applyCACertBundle(m, bundle, sizeof(bundle), 0)) << "1-arg bundle overload";
  EXPECT_TRUE(m.b == bundle) << "1-arg bundle received pointer";
}

TEST_F(TlsConfigTest, ApplyCACertBundleFallbackNoSetter) {
  struct MockBundleNoArg {};
  const uint8_t bundle[] = {0x01, 0x02, 0x03, 0x04};
  MockBundleNoArg m;

  EXPECT_FALSE(esp32_http_client_detail::applyCACertBundle(m, bundle, sizeof(bundle), 0))
      << "no-arg bundle fallback returns false";
}

class TlsLifecycleTest : public RequestFixture {
 protected:
  ESP32HTTPClient c{"https://api.example.com"};
  int stops = 0;

  void SetUp() override {
    RequestFixture::SetUp();
    c.setCACert("INIT_CA");
    HttpClientStub::setResponse(200, "{}");
    c.get("/1").execute();
    stops = NetworkClientSecureStub::stopCallCount;
  }

  void expectStopCalled(const char* what) {
    EXPECT_GT(NetworkClientSecureStub::stopCallCount, stops) << what << " must call stop()";
    stops = NetworkClientSecureStub::stopCallCount;
  }
};

TEST_F(TlsLifecycleTest, SetCACertCallsStop) {
  c.setCACert("NEW_CA");
  expectStopCalled("setCACert");
}

TEST_F(TlsLifecycleTest, SetInsecureCallsStop) {
  c.setInsecure();
  expectStopCalled("setInsecure");
}

TEST_F(TlsLifecycleTest, SetClientCertCallsStop) {
  c.setClientCert("C", "K");
  expectStopCalled("setClientCert");
}

TEST_F(TlsLifecycleTest, SetCACertBundleCallsStop) {
  c.setCACertBundle(nullptr, 0);
  expectStopCalled("setCACertBundle");
}

TEST_F(TlsLifecycleTest, SetBaseUrlCallsStop) {
  c.setBaseUrl("https://other.example.com");
  expectStopCalled("setBaseUrl");
}

TEST_F(TlsLifecycleTest, SetPortCallsStop) {
  c.setPort(8443);
  expectStopCalled("setPort");
}

TEST_F(TlsLifecycleTest, EndCallsStop) {
  c.end();
  expectStopCalled("end()");
}

TEST_F(TlsLifecycleTest, ReconnectAfterEndUsesSecureClient) {
  c.end();
  c.setCACert("RECONNECT_CA");
  HttpClientStub::setResponse(200, "{\"reconnected\":true}");
  c.get("/2").execute();

  EXPECT_TRUE(HttpClientStub::usedSecureClient) << "Must reconnect using secure client after end()";
  EXPECT_EQ(c.getStatusCode(), 200) << "Reconnected request should succeed";
}

class TlsRetryTest : public RequestFixture {};

TEST_F(TlsRetryTest, RetryReusesSecureClientAndSucceeds) {
  ESP32HTTPClient c("https://retry.example.com");
  c.setCACert("CA_RETRY");
  c.setMaxRetry(1);

  HttpClientStub::queueResponse(HTTPC_ERROR_CONNECTION_LOST, "");
  HttpClientStub::queueResponse(200, "{\"success\":true}");
  c.get("/data").execute();

  EXPECT_EQ(c.getStatusCode(), 200) << "Request should succeed after retry";
  EXPECT_EQ(HttpClientStub::requestCount, 2) << "Two attempts should have been made";
  EXPECT_TRUE(HttpClientStub::usedSecureClient) << "Retry must use secure client";
  EXPECT_NE(HttpClientStub::lastSecureClient, nullptr) << "Secure client must not be null";
}

TEST_F(TlsRetryTest, RetryExhaustedReportsError) {
  ESP32HTTPClient c("https://retry.example.com");
  c.setCACert("CA_RETRY");
  c.setMaxRetry(1);

  HttpClientStub::queueResponse(HTTPC_ERROR_CONNECTION_LOST, "");
  HttpClientStub::queueResponse(HTTPC_ERROR_CONNECTION_LOST, "");
  c.get("/data").execute();

  EXPECT_EQ(HttpClientStub::requestCount, 2) << "One attempt plus one retry should have been made";
  EXPECT_EQ(c.getStatusCode(), HTTPC_ERROR_CONNECTION_LOST) << "Status should be the last error";
  EXPECT_TRUE(c.hasError()) << "hasError() must be true when all retries fail";
}

class TlsProtocolTest : public RequestFixture {};

TEST_F(TlsProtocolTest, GraphqlPostUsesSecureConnection) {
  const char* ca = "GRAPHQL_CA";
  ESP32HTTPClient c("https://api.example.com");
  c.setCACert(ca);

  HttpClientStub::setResponse(200, "{\"data\":{\"name\":\"ESP32\"}}");
  c.graphqlPost("/graphql").query("{ name }").execute();

  EXPECT_TRUE(HttpClientStub::usedSecureClient) << "GraphQL POST must use unified secure connection";
  EXPECT_EQ(NetworkClientSecureStub::lastCACert, ca) << "GraphQL CA cert passed";
}

TEST_F(TlsProtocolTest, SoapUsesSecureConnection) {
  const char* ca = "SOAP_CA";
  ESP32HTTPClient c("https://soap.example.com");
  c.setCACert(ca);

  HttpClientStub::setResponse(200, "<soap:Envelope><soap:Body><res>ok</res></soap:Body></soap:Envelope>");
  c.soap("/service").action("DoWork").execute();

  EXPECT_TRUE(HttpClientStub::usedSecureClient) << "SOAP must use unified secure connection";
  EXPECT_EQ(NetworkClientSecureStub::lastCACert, ca) << "SOAP CA cert passed";
}

TEST_F(TlsProtocolTest, JsonRpcUsesSecureConnection) {
  const char* ca = "RPC_CA";
  ESP32HTTPClient c("https://rpc.example.com");
  c.setCACert(ca);

  HttpClientStub::setResponse(200, "{\"jsonrpc\":\"2.0\",\"result\":\"pong\",\"id\":1}");
  c.jsonRpc("/api").method("ping").id(1).execute();

  EXPECT_TRUE(HttpClientStub::usedSecureClient) << "JSON-RPC must use unified secure connection";
  EXPECT_EQ(NetworkClientSecureStub::lastCACert, ca) << "JSON-RPC CA cert passed";
}

class TlsErrorTest : public RequestFixture {};

TEST_F(TlsErrorTest, InitialErrorsAreEmpty) {
  ESP32HTTPClient c("https://fail.example.com");
  c.setCACert("FAIL_CA");

  EXPECT_EQ(c.getTlsError(), 0) << "Initial TLS error should be 0";
  EXPECT_STREQ(c.getTlsErrorMessage().c_str(), "") << "Initial TLS error message should be empty";
  EXPECT_STREQ(c.getTlsErrorHint().c_str(), "") << "Initial TLS error hint should be empty";
}

TEST_F(TlsErrorTest, TlsErrorReflectsSecureClientError) {
  ESP32HTTPClient c("https://fail.example.com");
  c.setCACert("FAIL_CA");
  NetworkClientSecureStub::lastErrorCode = -0x2700;
  NetworkClientSecureStub::lastErrorMessage = "X509 - Certificate verification failed";

  EXPECT_EQ(c.getTlsError(), -0x2700) << "getTlsError returns mbedtls code";
  EXPECT_STREQ(c.getTlsErrorMessage().c_str(), "X509 - Certificate verification failed")
      << "getTlsErrorMessage returns description";
}

TEST_F(TlsErrorTest, OnErrorCallbackReceivesDetailedMessage) {
  ESP32HTTPClient c("https://fail.example.com");
  c.setCACert("FAIL_CA");
  NetworkClientSecureStub::lastErrorCode = -0x2700;
  NetworkClientSecureStub::lastErrorMessage = "X509 - Certificate verification failed";
  HttpClientStub::setResponse(HTTPC_ERROR_CONNECTION_REFUSED, "");

  bool fired = false;
  int code = 0;
  String msg;
  c.onError([&](int cd, const char* m) {
    fired = true;
    code = cd;
    msg = m;
  });

  c.get("/test").execute();

  EXPECT_TRUE(fired) << "onError callback should be called on connection/handshake failure";
  EXPECT_EQ(code, HTTPC_ERROR_CONNECTION_REFUSED) << "Error code should match HTTPClient code";
  EXPECT_GE(msg.indexOf("Connection Refused"), 0) << "Error message should include Connection Refused";
  EXPECT_GE(msg.indexOf("-0x2700"), 0) << "Error message should include mbedTLS hex code";
  EXPECT_GE(msg.indexOf("Hint:"), 0) << "Error message should include diagnostic hint";
}

TEST_F(TlsErrorTest, ObservabilityFiresOnFailure) {
  ESP32HTTPClient c("https://fail.example.com");
  c.setCACert("FAIL_CA");
  HttpClientStub::setResponse(HTTPC_ERROR_CONNECTION_REFUSED, "");

  bool fired = false;
  c.onObservability([&](const ObservabilityMetrics&) { fired = true; });
  c.get("/test").execute();

  EXPECT_TRUE(fired) << "Observability callback should fire on failure";
}

TEST_F(TlsErrorTest, SuccessfulRequestHasNoError) {
  ESP32HTTPClient c("https://ok.example.com");
  c.setCACert("OK_CA");
  HttpClientStub::setResponse(200, "{}");

  bool fired = false;
  c.onError([&](int, const char*) { fired = true; });
  c.get("/ok").execute();

  EXPECT_FALSE(fired) << "onError must not fire on success";
  EXPECT_FALSE(c.hasError()) << "hasError() must be false on success";
  EXPECT_STREQ(c.getTlsErrorHint().c_str(), "") << "TLS hint must be empty on success";
}

class TlsHintTest : public RequestFixture {
 protected:
  static String hint(int code, long epoch = kClockSynced) {
    return ESP32HTTPClient::tlsErrorToHint(code, epoch);
  }
};

TEST_F(TlsHintTest, ZeroIsEmpty) {
  EXPECT_STREQ(ESP32HTTPClient::tlsErrorToHint(0).c_str(), "") << "tlsErrorToHint(0) is empty";
}

TEST_F(TlsHintTest, CertVerifyFailedWithUnsyncedClockMentionsNtp) {
  String h = hint(-0x2700, 100000L);
  EXPECT_GE(h.indexOf("before 2023"), 0) << "0x2700 with old clock mentions before 2023";
  EXPECT_GE(h.indexOf("NTP"), 0) << "0x2700 with old clock mentions NTP";
}

TEST_F(TlsHintTest, CertVerifyFailedWithSyncedClockMentionsOpenssl) {
  EXPECT_GE(hint(-0x2700).indexOf("openssl s_client -showcerts"), 0)
      << "0x2700 with synced clock mentions openssl s_client";
}

TEST_F(TlsHintTest, InvalidPemMentionsBeginCertificate) {
  EXPECT_GE(hint(-0x2180).indexOf("BEGIN CERTIFICATE"), 0) << "0x2180 hint mentions PEM format";
  EXPECT_GE(hint(-0x3D00).indexOf("BEGIN CERTIFICATE"), 0) << "0x3D00 hint mentions PEM format";
}

TEST_F(TlsHintTest, AllocationFailureMentionsClientEnd) {
  EXPECT_GE(hint(-0x7F00).indexOf("client.end()"), 0) << "0x7F00 hint mentions client.end()";
  EXPECT_GE(hint(-0x0010).indexOf("client.end()"), 0) << "0x0010 hint mentions client.end()";
}

TEST_F(TlsHintTest, FatalAlert) {
  EXPECT_GE(hint(-0x7780).indexOf("Fatal TLS alert"), 0) << "0x7780 hint mentions fatal alert";
}

TEST_F(TlsHintTest, PeerClosed) {
  EXPECT_GE(hint(-0x7280).indexOf("Peer closed"), 0) << "0x7280 hint mentions peer closed";
}

TEST_F(TlsHintTest, ClientCertRequiredMentionsSetClientCert) {
  EXPECT_GE(hint(-0x7680).indexOf("setClientCert"), 0) << "0x7680 hint mentions setClientCert";
}

TEST_F(TlsHintTest, TimeoutOrResetMentionsSetTimeout) {
  EXPECT_GE(hint(-0x6800).indexOf("setTimeout"), 0) << "0x6800 hint mentions setTimeout";
  EXPECT_GE(hint(-0x004C).indexOf("setTimeout"), 0) << "0x004C hint mentions setTimeout";
}

TEST_F(TlsHintTest, ConnectFailureMentionsDns) {
  EXPECT_GE(hint(-0x0042).indexOf("DNS"), 0) << "0x0042 hint mentions DNS";
}

TEST_F(TlsHintTest, UnknownCodeWithUnsyncedClockMentionsClock) {
  EXPECT_GE(hint(-0x6000, 500L).indexOf("before 2023"), 0) << "unknown code with old clock mentions before 2023";
}

TEST_F(TlsHintTest, UnknownCodeWithSyncedClockIsGenericHandshake) {
  EXPECT_GE(hint(-0x6000).indexOf("TLS handshake failed"), 0)
      << "unknown code with synced clock mentions TLS handshake failed";
}

class TlsClientHintTest : public RequestFixture {};

TEST_F(TlsClientHintTest, UnsyncedClockDetectedOnHttpsFailure) {
  ArduinoStub::mockEpoch = kClockUnsynced;
  ESP32HTTPClient c("https://ntp-missing.example.com");
  c.setCACert("SOME_CA");
  HttpClientStub::setResponse(HTTPC_ERROR_CONNECTION_REFUSED, "");

  c.get("/ping").retry(0).execute();

  EXPECT_GE(c.getTlsErrorHint().indexOf("before 2023"), 0) << "getTlsErrorHint detects unsynced clock on HTTPS -1";
  EXPECT_GE(c.getErrorMessage().indexOf("before 2023"), 0) << "getErrorMessage includes NTP hint when clock < 2023";
}

TEST_F(TlsClientHintTest, DefaultTlsFailureAdvisesConfiguringRootCA) {
  ArduinoStub::mockEpoch = kClockSynced;
  ESP32HTTPClient c("https://default-tls.example.com");
  HttpClientStub::setResponse(HTTPC_ERROR_CONNECTION_REFUSED, "");

  c.get("/ping").retry(0).execute();

  EXPECT_GE(c.getTlsErrorHint().indexOf("without a configured Root CA"), 0)
      << "getTlsErrorHint advises configuring Root CA on default TLS failure";
}

TEST_F(TlsClientHintTest, HttpFailureHasNoTlsHint) {
  ArduinoStub::mockEpoch = kClockUnsynced;
  ESP32HTTPClient c("http://plain.example.com");
  HttpClientStub::setResponse(HTTPC_ERROR_CONNECTION_REFUSED, "");

  c.get("/ping").retry(0).execute();

  EXPECT_EQ(c.getTlsErrorHint().indexOf("before 2023"), -1) << "http:// failure must not produce a TLS clock hint";
}

class TlsFailClosedTest : public RequestFixture {};

TEST_F(TlsFailClosedTest, ErrorToStringForNotValidated) {
  EXPECT_STREQ(ESP32HTTPClient::errorToString(HTTPC_ERROR_TLS_NOT_VALIDATED).c_str(), "TLS Validation Required")
      << "errorToString(-12) returns TLS Validation Required";
}

TEST_F(TlsFailClosedTest, SetInsecureLogsWarning) {
  ESP32HTTPClient c("https://insecure.example.com");
  c.setInsecure();

  EXPECT_GE(ArduinoStub::warningLogCount, 1) << "setInsecure() logs a security warning";
  EXPECT_NE(ArduinoStub::lastWarningLog.find("setInsecure()"), std::string::npos) << "Warning mentions setInsecure()";
}

TEST_F(TlsFailClosedTest, InsecureIsNotValidatedTls) {
  ESP32HTTPClient c("https://insecure.example.com");
  c.setInsecure();

  EXPECT_FALSE(c.hasValidatedTlsConfig()) << "setInsecure() does not count as validated TLS";
}

TEST_F(TlsFailClosedTest, ExecutingInsecureRequestLogsWarningAgain) {
  ESP32HTTPClient c("https://insecure.example.com");
  c.setInsecure();
  int before = ArduinoStub::warningLogCount;

  HttpClientStub::setResponse(200, "{\"ok\":true}");
  c.get("/data").execute();

  EXPECT_GT(ArduinoStub::warningLogCount, before) << "Executing HTTPS with setInsecure() logs warning";
}

TEST_F(TlsFailClosedTest, DefaultTlsPathLogsWarning) {
  ESP32HTTPClient c("https://unconfigured.example.com");
  HttpClientStub::setResponse(200, "{\"ok\":true}");
  c.get("/data").execute();

  EXPECT_GE(ArduinoStub::warningLogCount, 1) << "HTTPS with DEFAULT_TLS logs a warning";
  EXPECT_NE(ArduinoStub::lastWarningLog.find("without setCACert()"), std::string::npos)
      << "Warning mentions missing setCACert()";
}

TEST_F(TlsFailClosedTest, BlocksUnconfiguredHttps) {
  ESP32HTTPClient c("https://strict.example.com");
  c.requireValidatedTls(true);
  EXPECT_TRUE(c.isValidatedTlsRequired()) << "isValidatedTlsRequired() is true";

  bool called = false;
  int code = 0;
  String msg;
  c.get("/secret")
      .onError([&](int cd, const char* m) {
        called = true;
        code = cd;
        msg = m;
      })
      .execute();

  EXPECT_EQ(HttpClientStub::requestCount, 0) << "Fail-closed mode must not dispatch network request";
  EXPECT_EQ(c.getStatusCode(), HTTPC_ERROR_TLS_NOT_VALIDATED) << "Status code is HTTPC_ERROR_TLS_NOT_VALIDATED (-12)";
  EXPECT_TRUE(c.hasError()) << "hasError() is true when blocked by requireValidatedTls";
  EXPECT_TRUE(called) << "onError callback fired when blocked by requireValidatedTls";
  EXPECT_EQ(code, HTTPC_ERROR_TLS_NOT_VALIDATED) << "onError received -12";
  EXPECT_GE(msg.indexOf("requireValidatedTls(true)"), 0) << "Error message explains requireValidatedTls failure";
  EXPECT_GE(ArduinoStub::errorLogCount, 1) << "Error log emitted on fail-closed refusal";
}

TEST_F(TlsFailClosedTest, BlocksSetInsecureOnRest) {
  ESP32HTTPClient c("https://strict.example.com");
  c.requireValidatedTls(true);
  c.setInsecure();

  c.get("/rest").execute();

  EXPECT_EQ(c.getStatusCode(), HTTPC_ERROR_TLS_NOT_VALIDATED) << "setInsecure() blocked by requireValidatedTls on REST";
  EXPECT_GE(c.getTlsErrorHint().indexOf("setInsecure() is not allowed"), 0)
      << "Hint explains setInsecure is not allowed";
  EXPECT_EQ(HttpClientStub::requestCount, 0) << "No network request sent when blocked";
}

TEST_F(TlsFailClosedTest, BlocksSoap) {
  ESP32HTTPClient c("https://strict.example.com");
  c.requireValidatedTls(true);
  c.setInsecure();

  c.soap("/ws").action("Act").body("<x/>").execute();

  EXPECT_EQ(c.getStatusCode(), HTTPC_ERROR_TLS_NOT_VALIDATED) << "SOAP blocked by requireValidatedTls";
  EXPECT_EQ(HttpClientStub::requestCount, 0) << "No network request sent when blocked";
}

TEST_F(TlsFailClosedTest, BlocksGraphql) {
  ESP32HTTPClient c("https://strict.example.com");
  c.requireValidatedTls(true);
  c.setInsecure();

  c.graphql("/graphql").query("{ ping }").execute();

  EXPECT_EQ(c.getStatusCode(), HTTPC_ERROR_TLS_NOT_VALIDATED) << "GraphQL blocked by requireValidatedTls";
  EXPECT_EQ(HttpClientStub::requestCount, 0) << "No network request sent when blocked";
}

TEST_F(TlsFailClosedTest, BlocksJsonRpc) {
  ESP32HTTPClient c("https://strict.example.com");
  c.requireValidatedTls(true);
  c.setInsecure();

  c.jsonRpc("/rpc").method("ping").execute();

  EXPECT_EQ(c.getStatusCode(), HTTPC_ERROR_TLS_NOT_VALIDATED) << "JSON-RPC blocked by requireValidatedTls";
  EXPECT_EQ(HttpClientStub::requestCount, 0) << "No network request sent when blocked";
}

TEST_F(TlsFailClosedTest, AllowsRequestsAfterSetCACert) {
  ESP32HTTPClient c("https://strict.example.com");
  c.requireValidatedTls(true);
  c.setInsecure();

  c.setCACert("-----BEGIN CERTIFICATE-----\nVALID\n-----END CERTIFICATE-----\n");
  EXPECT_TRUE(c.hasValidatedTlsConfig()) << "hasValidatedTlsConfig() is true after setCACert";

  HttpClientStub::setResponse(200, "{\"ok\":true}");
  c.get("/rest").execute();
  EXPECT_EQ(c.getStatusCode(), 200) << "HTTPS succeeds in fail-closed mode after setCACert";
}

TEST_F(TlsFailClosedTest, AllowsRequestsAfterSetCACertBundle) {
  ESP32HTTPClient c("https://strict.example.com");
  c.requireValidatedTls(true);

  const uint8_t bundle[] = {0x01, 0x02};
  c.setCACertBundle(bundle, sizeof(bundle));
  EXPECT_TRUE(c.hasValidatedTlsConfig()) << "hasValidatedTlsConfig() is true after setCACertBundle";

  HttpClientStub::setResponse(200, "{\"ok\":true}");
  c.get("/rest").execute();
  EXPECT_EQ(c.getStatusCode(), 200) << "HTTPS succeeds in fail-closed mode after setCACertBundle";
}

TEST_F(TlsFailClosedTest, SetInsecureAfterCACertRemovesValidatedConfig) {
  ESP32HTTPClient c("https://strict.example.com");
  c.setCACert(kDummyCA);
  c.setInsecure();

  EXPECT_FALSE(c.hasValidatedTlsConfig()) << "setInsecure() after setCACert must drop validated TLS config";
}

TEST_F(TlsFailClosedTest, DisablingRequirementAllowsUnconfiguredHttps) {
  ESP32HTTPClient c("https://strict.example.com");
  c.requireValidatedTls(true);
  c.requireValidatedTls(false);
  EXPECT_FALSE(c.isValidatedTlsRequired()) << "requireValidatedTls(false) disables fail-closed mode";

  HttpClientStub::setResponse(200, "{\"ok\":true}");
  c.get("/data").execute();

  EXPECT_EQ(HttpClientStub::requestCount, 1) << "Request must be dispatched once requirement is disabled";
  EXPECT_EQ(c.getStatusCode(), 200) << "Request should succeed once requirement is disabled";
}

TEST_F(TlsFailClosedTest, PlainHttpIsNotBlocked) {
  ESP32HTTPClient c("http://plain.example.com");
  c.requireValidatedTls(true);

  HttpClientStub::setResponse(200, "{\"ok\":true}");
  c.get("/data").execute();

  EXPECT_EQ(HttpClientStub::requestCount, 1) << "http:// is not subject to validated TLS requirement";
  EXPECT_EQ(c.getStatusCode(), 200) << "http:// request should succeed in fail-closed mode";
}

class TlsKeepAliveTest : public RequestFixture {};

TEST_F(TlsKeepAliveTest, SessionReusesSameSecureClient) {
  ESP32HTTPClient c("https://keepalive.example.com");
  c.setCACert("KEEPALIVE_CA");

  HttpClientStub::setResponse(200, "{\"r\":1}");
  c.get("/one").execute();
  void* first = HttpClientStub::lastSecureClient;
  int stops1 = NetworkClientSecureStub::stopCallCount;

  HttpClientStub::setResponse(200, "{\"r\":2}");
  c.get("/two").execute();
  void* second = HttpClientStub::lastSecureClient;
  int stops2 = NetworkClientSecureStub::stopCallCount;

  EXPECT_NE(first, nullptr) << "Client 1 must be non-null";
  EXPECT_EQ(first, second) << "Persistent TLS session must reuse the same client pointer";
  EXPECT_EQ(stops1, stops2) << "Keep-Alive requests must not stop client between requests";
}

TEST_F(TlsKeepAliveTest, ReconfigureBetweenRequestsStopsClient) {
  ESP32HTTPClient c("https://keepalive.example.com");
  c.setCACert("KEEPALIVE_CA");

  HttpClientStub::setResponse(200, "{}");
  c.get("/one").execute();
  int before = NetworkClientSecureStub::stopCallCount;

  c.setCACert("OTHER_CA");
  HttpClientStub::setResponse(200, "{}");
  c.get("/two").execute();

  EXPECT_GT(NetworkClientSecureStub::stopCallCount, before) << "Reconfiguring TLS must stop the client";
}