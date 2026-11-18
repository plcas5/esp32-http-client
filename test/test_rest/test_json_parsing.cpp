#include <cstring>

#include "test_support.h"

class JsonParsingTest : public RequestFixture {
 protected:
  // One user
  static constexpr const char* kUserJson = R"({
        "id": 1,
        "name": "Leanne Graham",
        "username": "Bret",
        "email": "Sincere@april.biz",
        "address": {
          "street": "Kulas Light",
          "suite": "Apt. 556",
          "city": "Gwenborough",
          "zipcode": "92998-3874",
          "geo": {
            "lat": "-37.3159",
            "lng": "81.1496"
          }
        },
        "phone": "1-770-736-8031 x56442",
        "website": "hildegard.org",
        "company": {
          "name": "Romaguera-Crona",
          "catchPhrase": "Multi-layered client-server neural-net",
          "bs": "harness real-time e-markets"
        }
      })";

  // Array of users
  static constexpr const char* kUsersJson = R"([
        {
          "name": "Leanne Graham",
          "address": { "city": "Gwenborough" }
        },
        {
          "name": "Ervin Howell",
          "address": {
            "city": "Wisokyburgh",
            "geo": { "lat": "-43.9509" }
          }
        }
      ])";

  // Parses json using provided req
  static void parse(RestRequest& req, const char* json) {
    StringStream stream(json);
    BufferedStreamReader reader(&stream);
    req.parseResponse(reader);
  }
};

TEST_F(JsonParsingTest, ParseResponseBindsTypes) {
  RestRequest& req = make("/x", HTTP_GET_METHOD);

  int count = 0;
  float temperature = 0.0f;
  double voltage = 0.0;
  bool active = false;
  long timestamp = 0;
  char label[16] = {0};

  req.getBody("count", &count)
      .getBody("temperature", &temperature)
      .getBody("voltage", &voltage)
      .getBody("active", &active)
      .getBody("timestamp", &timestamp)
      .getBody("label", label, sizeof(label));

  parse(
      req,
      R"({"count":7,"temperature":24.5,"voltage":3.3001,"active":true,"timestamp":1710000010,"label":"sensor\"A","ignore":{"k":1}})");

  EXPECT_EQ(count, 7) << "count should be parsed as int";
  EXPECT_NEAR(temperature, 24.5, 0.001) << "temperature should be parsed as float";
  EXPECT_NEAR(voltage, 3.3001, 0.00001) << "voltage should be parsed as double";
  EXPECT_TRUE(active) << "active should be parsed as bool";
  EXPECT_EQ(timestamp, 1710000010L) << "timestamp should be parsed as long";
  EXPECT_STREQ(label, "sensor\"A") << "escaped string should be unescaped and copied";
}

TEST_F(JsonParsingTest, ParsePrimitiveTypes) {
  RestRequest& req = make("/prim", HTTP_GET_METHOD);

  double dVal = 0.0;
  String bStr;

  req.getBody("dVal", &dVal).getBody("bStr", &bStr);

  parse(req, R"({"dVal":3.1415926535,"bStr":true})");

  EXPECT_NEAR(dVal, 3.1415926535, 0.000001) << "double precision parsing";
  EXPECT_EQ(bStr.str(), "true") << "boolean to String parsing";
}

TEST_F(JsonParsingTest, ParsePrimitiveEdgeCases) {
  HttpClientStub::setResponse(
      200,
      R"({"d_null": null, "l_null": null, "str_int": "1234", "num_str": 9876, "positive_exp": 6.02e+2, "negative_exp": -2.5E-3})");

  double dVal = 123.45;
  long lVal = 999L;
  int strIntVal = 0;
  String numStrVal;
  float positiveExp = 0.0f;
  double negativeExp = 0.0;

  client.get("/primitives")
      .getBody("d_null", &dVal)
      .getBody("l_null", &lVal)
      .getBody("str_int", &strIntVal)
      .getBody("num_str", &numStrVal)
      .getBody("positive_exp", &positiveExp)
      .getBody("negative_exp", &negativeExp);

  EXPECT_NEAR(dVal, 0.0, 0.001) << "null double set to 0.0";
  EXPECT_EQ(lVal, 0L) << "null long set to 0L";
  EXPECT_EQ(strIntVal, 1234) << "string coerced to int";
  EXPECT_EQ(numStrVal.str(), "9876") << "number coerced to Arduino String";
  EXPECT_NEAR(positiveExp, 602.0, 0.001) << "positive exponent parsed as float";
  EXPECT_NEAR(negativeExp, -0.0025, 0.000001) << "negative exponent parsed as double";
}

TEST_F(JsonParsingTest, MoreParseEdgeCases) {
  RestRequest& req = make("/edge", HTTP_GET_METHOD);

  bool active = true;
  String strVal;
  long lval = 0;
  char charbuf[10] = {0};

  req.getBody("active", &active)
      .getBody("strVal", &strVal)
      .getBody("lval", &lval)
      .getBody("charbuf", charbuf, sizeof(charbuf));

  parse(
      req,
      R"({"active":false,"strVal":"hello","lval":-12345,"charbuf":"1234567890123","ignore_arr":[1,2,3],"ignore_obj":{"a":1}})");

  EXPECT_FALSE(active) << "boolean false should be parsed";
  EXPECT_EQ(strVal.str(), "hello") << "arduino string should be parsed";
  EXPECT_EQ(lval, -12345) << "negative long should be parsed";
  EXPECT_STREQ(charbuf, "123456789") << "char buffer should be truncated to max length";
}

TEST_F(JsonParsingTest, ParseEscapedStringsSimple) {
  RestRequest& req = make("/edge", HTTP_GET_METHOD);
  char buf[32] = {0};
  req.getBody("escaped", buf, sizeof(buf));

  parse(req, R"({"escaped":"a\"b\\c"})");

  EXPECT_STREQ(buf, "a\"b\\c") << "escaped char buffer";
}

TEST_F(JsonParsingTest, ParseNestedJSON) {
  RestRequest& req = make("/users/1", HTTP_GET_METHOD);

  char street[64] = {0};
  float lat = 0.0f;

  req.getBody("address.street", street, sizeof(street)).getBody("address.geo.lat", &lat);

  parse(req, kUserJson);

  EXPECT_GT(strlen(street), 0u) << "street should be populated";
  EXPECT_NE(lat, 0.0f) << "lat should be populated and parsed as float";
}

TEST_F(JsonParsingTest, ParseNestedJSONMissingFields) {
  RestRequest& req = make("/users/1", HTTP_GET_METHOD);

  char invalidStreet[64] = {0};
  float invalidLat = 0.0f;

  req.getBody("address.unknown_street", invalidStreet, sizeof(invalidStreet))
      .getBody("address.not_found.deep", &invalidLat);

  parse(req, R"({
        "address": {
          "street": "Kulas Light",
          "geo": {
            "lat": "-37.3159"
          }
        }
      })");

  EXPECT_EQ(strlen(invalidStreet), 0u) << "missing string field should remain empty";
  EXPECT_EQ(invalidLat, 0.0f) << "missing float field should remain unchanged (0.0f)";
}

TEST_F(JsonParsingTest, ObjectWithNestedArrayChildBinding) {
  HttpClientStub::setResponse(200, R"({"items": [100, 200], "flag": true})");

  int item0 = 0;
  bool flag = false;
  client.get("/nested_arr_in_obj").getBody("items.0", &item0).getBody("flag", &flag);

  EXPECT_EQ(item0, 100) << "items.0 extracted from object";
  EXPECT_TRUE(flag) << "flag extracted";
}

TEST_F(JsonParsingTest, RawJsonRootObjectAndEscapedStrings) {
  const char* json = R"({"msg": "hello \"world\" \\ test", "num": 123})";
  HttpClientStub::setResponse(200, json);

  String rawJson;
  client.get("/data").getBody("", &rawJson);

  EXPECT_STREQ(rawJson.c_str(), json) << "raw JSON root object captured with escaped chars";
}

TEST_F(JsonParsingTest, RawRootArray) {
  String entireArray;
  RestRequest& req = make("/users", HTTP_GET_METHOD);
  req.getBody("", &entireArray);

  parse(req, kUsersJson);

  EXPECT_GT(entireArray.str().length(), 50u) << "should contain the raw JSON of the root array";
  EXPECT_EQ(entireArray.str().substr(0, 1), "[") << "should start with [";
}

TEST_F(JsonParsingTest, RawNestedObject) {
  String specificObject;
  RestRequest& req = make("/users", HTTP_GET_METHOD);
  req.getBody("1.address", &specificObject);

  parse(req, kUsersJson);

  EXPECT_GT(specificObject.str().length(), 20u) << "should contain the raw JSON of the nested object";
  EXPECT_EQ(specificObject.str().substr(0, 1), "{") << "should start with {";
}

TEST_F(JsonParsingTest, MissingPathLeavesTargetUnchanged) {
  String notFound = "original";
  RestRequest& req = make("/users", HTTP_GET_METHOD);
  req.getBody("5.unknown", &notFound);

  parse(req, kUsersJson);

  EXPECT_EQ(notFound.str(), "original");
}

TEST_F(JsonParsingTest, ArrayPrimitiveElements) {
  HttpClientStub::setResponse(200, "[10, 20, 30]");

  int item0 = 0, item1 = 0, item2 = 0;
  client.get("/array").getBody("0", &item0).getBody("1", &item1).getBody("2", &item2);

  EXPECT_EQ(item0, 10) << "array[0] primitive";
  EXPECT_EQ(item1, 20) << "array[1] primitive";
  EXPECT_EQ(item2, 30) << "array[2] primitive";
}

TEST_F(JsonParsingTest, ArrayRawElement) {
  HttpClientStub::setResponse(200, R"([{"a":1}, {"b":2}])");

  String elem0;
  client.get("/raw_array_elem").getBody("0", &elem0);

  EXPECT_EQ(elem0.str(), R"({"a":1})") << "array element raw json string";
}

TEST_F(JsonParsingTest, NestedArrayElement) {
  HttpClientStub::setResponse(200, "[[100, 200], [300, 400]]");

  int nestedVal = 0;
  client.get("/nested_array").getBody("1.0", &nestedVal);

  EXPECT_EQ(nestedVal, 300) << "nested array element 1.0";
}

TEST_F(JsonParsingTest, EmptyObjectDoesNotCrash) {
  RestRequest& req = make("/empty", HTTP_GET_METHOD);
  int x = 10;
  req.getBody("x", &x);

  parse(req, "{}");

  EXPECT_EQ(x, 10);
}

TEST_F(JsonParsingTest, EmptyArrayDoesNotCrash) {
  RestRequest& req = make("/empty", HTTP_GET_METHOD);
  int x = 10;
  req.getBody("x", &x);

  parse(req, "[]");

  EXPECT_EQ(x, 10);
}

TEST_F(JsonParsingTest, SkipValueObjectAndArray) {
  HttpClientStub::setResponse(
      200,
      R"({"skip_obj": {"a": 1, "b": "str with \"quote\" and \\ slash", "nested": [10, 20]}, "skip_arr": [{"x": true}, [1, 2]], "keep": 42})");

  int keep = 0;
  client.get("/skip").getBody("keep", &keep);

  EXPECT_EQ(keep, 42) << "unmapped complex object and array skipped properly";
}

TEST_F(JsonParsingTest, SkipValueEscapedString) {
  HttpClientStub::setResponse(200, R"({"skip_str": "hello \"world\" \\ test", "keep": 123})");

  int keep = 0;
  client.get("/skip_esc").getBody("keep", &keep);

  EXPECT_EQ(keep, 123) << "skipValue skips escaped string";
}

TEST_F(JsonParsingTest, SkipValueAdvanced) {
  RestRequest& req = make("/skip", HTTP_GET_METHOD);

  int x = 0;
  req.getBody("x", &x);

  parse(req, R"({"skip_obj":{"a":"b\"c", "d":[1,2,{"e":"f"}]},"skip_arr":[1,"[","]",{"x":"y"}],"x":42})");

  EXPECT_EQ(x, 42) << "x should be parsed after skipping complex structures";
}

TEST_F(JsonParsingTest, MalformedKeyIsSkipped) {
  HttpClientStub::setResponse(200, R"({"badkey" 123, "goodkey": 456})");

  int good = 0;
  client.get("/malformed").getBody("goodkey", &good);

  EXPECT_EQ(good, 456) << "malformed key missing colon is skipped";
}

TEST_F(JsonParsingTest, UnixTimestampFetch) {
  HttpClientStub::setResponse(200, R"({"unix_timestamp":1781301337})");

  ESP32HTTPClient timeApi("https://timeapi.io");
  long ts = 0;
  timeApi.get("/api/v1/time/current/unix").getBody("unix_timestamp", &ts);

  EXPECT_EQ(HttpClientStub::lastMethod, "GET") << "should use GET method";
  EXPECT_EQ(HttpClientStub::lastUrl, "https://timeapi.io/api/v1/time/current/unix") << "url should be correct";
  EXPECT_EQ(ts, 1781301337L) << "unix_timestamp should be successfully parsed";
}