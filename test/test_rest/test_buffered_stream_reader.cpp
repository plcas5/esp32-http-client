#include <cstdio>
#include <cstring>

#include "test_support.h"

class BufferedStreamReaderTest : public RequestFixture {
 protected:
  // Makes the HTTP stub serve its response body from `stream` (size = -1 means chunked)
  static void serve(Stream& stream, int size) {
    HttpClientStub::customStream = &stream;
    HttpClientStub::customSize = size;
  }
  static void serveChunked(Stream& stream) {
    serve(stream, -1);
  }

  // Runs a GET on the fixture's client and returns the value bound to key
  String fetch(const char* key) {
    String val;
    client.get("/test").getBody(key, &val);
    return val;
  }

  // Parses body directly through a BufferedStreamReader (bypasses the HTTP stub)
  static void parse(RestRequest& req, const std::string& body, bool chunked) {
    StringStream stream(body);
    BufferedStreamReader reader(&stream, chunked);
    req.parseResponse(reader);
  }

  // Encodes payload as a single HTTP chunk followed by the terminating chunk
  static std::string encodeChunked(const std::string& payload) {
    char hex[24];
    snprintf(hex, sizeof(hex), "%zx\r\n", payload.size());
    return std::string(hex) + payload + "\r\n0\r\n\r\n";
  }
};

TEST_F(BufferedStreamReaderTest, ChunkedTransferEncoding) {
  RestRequest& req = make("/chunked", HTTP_GET_METHOD);
  char buf[32] = {0};
  req.getBody("msg", buf, sizeof(buf));

  parse(req, "e\r\n{\"msg\":\"hello \r\n7\r\nworld\"}\r\n0\r\n\r\n", true);

  EXPECT_STREQ(buf, "hello world") << "chunked data should be correctly reassembled inside parseResponse";
}

TEST_F(BufferedStreamReaderTest, LargePayloadRefill) {
  RestRequest& req = make("/large", HTTP_GET_METHOD);
  char buf[32] = {0};
  req.getBody("end", buf, sizeof(buf));

  std::string largeData = "{\"pad\":\"";
  largeData.append(600, 'A');
  largeData += "\",\"end\":\"found\"}";

  parse(req, largeData, false);

  EXPECT_STREQ(buf, "found") << "should successfully read beyond 512 bytes buffer size in parseResponse";
}

TEST_F(BufferedStreamReaderTest, NonChunkedWithCustomStream) {
  MockDirectStream stream(R"({"val":"H"})");
  serve(stream, static_cast<int>(stream.data.size()));

  EXPECT_STREQ(fetch("val").c_str(), "H") << "custom directStream read";
}

TEST_F(BufferedStreamReaderTest, TimeoutInNonChunkedRefill) {
  MockDirectStream stream;
  stream.availableTimesOut = true;
  serve(stream, 10);

  EXPECT_STREQ(fetch("val").c_str(), "") << "timeout in non-chunked returns empty";
}

TEST_F(BufferedStreamReaderTest, NonChunkedLargeBuffer) {
  std::string largeJson = "{\"large\":\"" + std::string(700, 'X') + "\"}";
  MockDirectStream stream(largeJson);
  serve(stream, static_cast<int>(largeJson.size()));

  EXPECT_EQ(static_cast<size_t>(fetch("").length()), largeJson.size()) << "whole 700+ bytes body must be read";
}

TEST_F(BufferedStreamReaderTest, ChunkedWithExtensionsAndEmptyHexLines) {
  MockDirectStream stream("\r\n13;ext=test\r\n{\"msg\":\"WORLD\"}\r\n0\r\n\r\n");
  serveChunked(stream);

  EXPECT_STREQ(fetch("msg").c_str(), "WORLD") << "chunked with extensions read WORLD";
}

TEST_F(BufferedStreamReaderTest, ChunkedTimeoutInChunkHeader) {
  MockDirectStream stream;
  stream.availableTimesOut = true;
  serveChunked(stream);

  EXPECT_STREQ(fetch("msg").c_str(), "") << "chunk timeout header returns empty";
}

TEST_F(BufferedStreamReaderTest, ChunkedTimeoutInChunkData) {
  MockDirectStream stream("5\r\n");
  serveChunked(stream);

  EXPECT_STREQ(fetch("msg").c_str(), "") << "chunk timeout data returns empty";
}

TEST_F(BufferedStreamReaderTest, LargeChunkSize) {
  // Chunk larger than BUF_SIZE (toRead > BUF_SIZE - _len): the refill loop must run to completion
  std::string payload = "{\"data\":\"" + std::string(600, 'Z') + "\"}";
  MockDirectStream stream(encodeChunked(payload));
  serveChunked(stream);

  EXPECT_EQ(static_cast<size_t>(fetch("").length()), payload.size()) << "big chunk read length";
}

TEST_F(BufferedStreamReaderTest, ChunkedWithZeroBytesRead) {
  MockDirectStream stream("10\r\n{\"data\":\"HI\"}\r\n0\r\n\r\n");
  stream.simulateZeroBytesRead = true;
  serveChunked(stream);

  EXPECT_STREQ(fetch("data").c_str(), "") << "zero bytes read causes eof";
}

TEST_F(BufferedStreamReaderTest, ChunkedWithPartialReads) {
  // readBytes only returns 2 bytes at a time (avail < toRead)
  MockDirectStream stream("10\r\n{\"data\":\"HI\"}\r\n0\r\n\r\n");
  stream.readBytesLimit = 2;
  serveChunked(stream);

  EXPECT_STREQ(fetch("data").c_str(), "HI") << "partial chunk read available";
}

TEST(BufferedStreamReaderStringTest, ReadsAndPeeksCharacters) {
  BufferedStreamReader reader("abc");

  EXPECT_TRUE(reader.available());
  EXPECT_EQ(reader.peek(), 'a');
  EXPECT_EQ(reader.read(), 'a');
  EXPECT_EQ(reader.read(), 'b');
  EXPECT_EQ(reader.read(), 'c');
  EXPECT_FALSE(reader.available()) << "no longer available after the last character";
  EXPECT_EQ(reader.read(), -1) << "read past end returns -1";
  EXPECT_EQ(reader.peek(), -1) << "peek past end returns -1";
}

TEST(BufferedStreamReaderStringTest, EmptyString) {
  BufferedStreamReader reader("");

  EXPECT_FALSE(reader.available());
  EXPECT_EQ(reader.read(), -1);
  EXPECT_EQ(reader.peek(), -1);
}

TEST(BufferedStreamReaderStreamTest, EmptyStreamReachesEof) {
  MockDirectStream stream("");
  BufferedStreamReader reader(&stream, false);

  EXPECT_FALSE(reader.available());
  EXPECT_EQ(reader.read(), -1);
  EXPECT_EQ(reader.peek(), -1);
}

TEST(BufferedStreamReaderStreamTest, ChunkedReaderTimeoutMeansNotAvailable) {
  MockDirectStream stream;
  stream.availableTimesOut = true;
  BufferedStreamReader reader(&stream, true);

  EXPECT_FALSE(reader.available());
}