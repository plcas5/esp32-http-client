#ifndef TEST_SUPPORT_H
#define TEST_SUPPORT_H

#include <gtest/gtest.h>

#include "HTTPClient.h"

#define private public
#include "ESP32HTTPClient.h"
#include "RestRequest.h"
#undef private

constexpr const char* kBaseUrl = "https://example.com";

class StringStream : public Stream {
 public:
  explicit StringStream(const std::string& content) : _content(content), _cursor(0) {
  }

  int available() override {
    return _cursor < _content.size() ? static_cast<int>(_content.size() - _cursor) : 0;
  }

  int read() override {
    if (!available()) return -1;
    return static_cast<unsigned char>(_content[_cursor++]);
  }

  int peek() override {
    if (!available()) return -1;
    return static_cast<unsigned char>(_content[_cursor]);
  }

 private:
  std::string _content;
  size_t _cursor;
};

class MockDirectStream : public Stream {
 public:
  std::string data;
  size_t cursor = 0;
  bool availableTimesOut = false;
  int availableLimit = -1;
  int readBytesLimit = -1;
  bool simulateZeroBytesRead = false;

  explicit MockDirectStream(const std::string& d = "") : data(d), cursor(0) {
  }

  int available() override {
    if (availableTimesOut) return 0;
    if (availableLimit >= 0) {
      if (availableLimit > 0) {
        availableLimit--;
        return static_cast<int>(data.size() - cursor);
      }
      return 0;
    }
    return cursor < data.size() ? static_cast<int>(data.size() - cursor) : 0;
  }

  int read() override {
    if (cursor < data.size()) {
      return static_cast<unsigned char>(data[cursor++]);
    }
    return -1;
  }

  int peek() override {
    if (cursor < data.size()) {
      return static_cast<unsigned char>(data[cursor]);
    }
    return -1;
  }

  size_t readBytes(char* buffer, size_t length) override {
    if (simulateZeroBytesRead) {
      simulateZeroBytesRead = false;
      return 0;
    }
    size_t toRead = length;
    if (readBytesLimit >= 0 && toRead > static_cast<size_t>(readBytesLimit)) {
      toRead = static_cast<size_t>(readBytesLimit);
    }
    size_t avail = cursor < data.size() ? data.size() - cursor : 0;
    if (toRead > avail) toRead = avail;
    for (size_t i = 0; i < toRead; i++) {
      buffer[i] = data[cursor++];
    }
    return toRead;
  }
};

class StubFixture : public ::testing::Test {
 protected:
  decltype(ArduinoStub::mockEpoch) savedEpoch = ArduinoStub::mockEpoch;

  void SetUp() override {
    resetAll();
  }
  void TearDown() override {
    resetAll();
    ArduinoStub::mockEpoch = savedEpoch;
  }
  static void resetAll() {
    HttpClientStub::reset();
    NetworkClientSecureStub::reset();
    ArduinoStub::resetLogs();
  }
};

class RequestFixture : public StubFixture {
 protected:
  ESP32HTTPClient client{kBaseUrl};

  std::vector<std::unique_ptr<RestRequest>> owned;

  template <typename Method>
  RestRequest& make(const char* path, Method method) {
    owned.push_back(std::make_unique<RestRequest>(&client, path, method));
    return *owned.back();
  }

  RestRequest& adopt(RestRequest&& r) {
    owned.push_back(std::make_unique<RestRequest>(std::move(r)));
    return *owned.back();
  }

  void TearDown() override {
    for (auto& r : owned) r->_executed = true;
    StubFixture::TearDown();
  }

  static std::string url(const char* path = "") {
    return std::string(kBaseUrl) + path;
  }
};

#endif
