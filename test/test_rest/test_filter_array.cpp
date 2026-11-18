#include "test_support.h"

namespace {
struct WeatherTestStruct {
  String city = "";
  String countryCode = "";
  int temperature = 0;
  float windSpeed = 0.0f;
  bool storm = false;

  REST_JSON_MAP(REST_FIELD_NAMED("location.city", city), REST_FIELD_NAMED("location.countryCode", countryCode),
                REST_FIELD(temperature), REST_FIELD(windSpeed), REST_FIELD(storm))
};

struct TempByCityTestStruct {
  String temperature;
  String city;
  REST_JSON_MAP(REST_FIELD(temperature), REST_FIELD(city))
};

constexpr const char* kForecasts = R"([
    {"location":{"city":"Toronto","countryCode":"CA"},"temperature":-2,"windSpeed":1.5,"storm":false},
    {"location":{"city":"Paris","countryCode":"FR"},"temperature":9,"windSpeed":3.0,"storm":true},
    {"location":{"city":"Montreal","countryCode":"CA"},"temperature":5,"windSpeed":2.0,"storm":false}
  ])";

constexpr const char* kRootObject = R"({
    "location":{"city":"Toronto","countryCode":"CA"},
    "temperature":-2,"windSpeed":1.5,"storm":false
  })";

constexpr const char* kIdCountry = R"([{"id":1,"c":"CA"},{"id":2,"c":"FR"},{"id":3,"c":"CA"}])";

constexpr const char* kMixedElements = R"([1,{"id":1,"c":"CA"},null,"x",[1,2],true,{"id":2,"c":"FR"}])";
}  // namespace

class FilterArrayTest : public RequestFixture {
 protected:
  std::vector<WeatherTestStruct> forecasts;
  String raw;

  static void respond(const char* json) {
    HttpClientStub::setResponse(200, json);
  }
};

TEST_F(FilterArrayTest, StringFilterOnNestedField) {
  respond(kForecasts);

  client.get("").filterArray("location.countryCode").equals("CA").collect(&forecasts);

  ASSERT_EQ(forecasts.size(), 2u) << "String filter must collect matching objects";
  EXPECT_STREQ(forecasts[0].city.c_str(), "Toronto") << "First matching city must be Toronto";
  EXPECT_STREQ(forecasts[1].city.c_str(), "Montreal") << "Second matching city must be Montreal";
}

TEST_F(FilterArrayTest, FloatFilter) {
  respond(kForecasts);

  client.get("").filterArray("windSpeed").equals(1.5f).collect(&forecasts);

  ASSERT_EQ(forecasts.size(), 1u) << "Float filter must collect one matching object";
  EXPECT_STREQ(forecasts[0].city.c_str(), "Toronto") << "Float filter must keep Toronto";
}

TEST_F(FilterArrayTest, DoubleFilter) {
  respond(kForecasts);

  client.get("").filterArray("windSpeed").equals(1.5).collect(&forecasts);

  ASSERT_EQ(forecasts.size(), 1u) << "Double filter must collect one matching object";
  EXPECT_STREQ(forecasts[0].city.c_str(), "Toronto") << "Double filter must keep Toronto";
}

TEST_F(FilterArrayTest, BooleanFilter) {
  respond(kForecasts);

  client.get("").filterArray("storm").equals(true).collect(&forecasts);

  ASSERT_EQ(forecasts.size(), 1u) << "Boolean filter must collect one matching object";
  EXPECT_STREQ(forecasts[0].city.c_str(), "Paris") << "Boolean filter must keep Paris";
}

TEST_F(FilterArrayTest, NegativeIntegerFilter) {
  respond(kForecasts);

  client.get("").filterArray("temperature").equals(-2).collect(&forecasts);

  ASSERT_EQ(forecasts.size(), 1u) << "Negative integer filter must collect one matching object";
  EXPECT_EQ(forecasts[0].temperature, -2) << "Filtered temperature must be -2";
}

TEST_F(FilterArrayTest, FilterWithoutConditionCollectsEveryElement) {
  respond(kForecasts);

  client.get("").filterArray("temperature").collect(&forecasts);

  EXPECT_EQ(forecasts.size(), 3u) << "Filter without comparison condition must collect every array element";
}

TEST_F(FilterArrayTest, NotEqualsString) {
  respond(kForecasts);

  client.get("").filterArray("location.countryCode").notEquals("CA").collect(&forecasts);

  ASSERT_EQ(forecasts.size(), 1u) << "notEquals must exclude objects matching the specified value";
  EXPECT_STREQ(forecasts[0].city.c_str(), "Paris") << "notEquals must keep Paris";
}

TEST_F(FilterArrayTest, NotEqualsBoolean) {
  respond(kForecasts);

  client.get("").filterArray("storm").notEquals(true).collect(&forecasts);

  ASSERT_EQ(forecasts.size(), 2u) << "Boolean notEquals must exclude objects matching the specified value";
  EXPECT_STREQ(forecasts[0].city.c_str(), "Toronto") << "First object without storm must be Toronto";
  EXPECT_STREQ(forecasts[1].city.c_str(), "Montreal") << "Second object without storm must be Montreal";
}

TEST_F(FilterArrayTest, NotEqualsInteger) {
  respond(kForecasts);

  client.get("").filterArray("temperature").notEquals(-2).collect(&forecasts);

  ASSERT_EQ(forecasts.size(), 2u) << "Integer notEquals must exclude objects matching the specified value";
  EXPECT_STREQ(forecasts[0].city.c_str(), "Paris") << "First remaining object must be Paris";
  EXPECT_STREQ(forecasts[1].city.c_str(), "Montreal") << "Second remaining object must be Montreal";
}

TEST_F(FilterArrayTest, MissingFieldDoesNotMatchNotEquals) {
  respond(kForecasts);

  client.get("").filterArray("inexistent").notEquals(0).collect(&forecasts);

  EXPECT_EQ(forecasts.size(), 0u) << "Objects with a missing filter field must not match notEquals";
}

TEST_F(FilterArrayTest, MissingFieldDoesNotMatchEquals) {
  respond(kForecasts);

  client.get("").filterArray("inexistent").equals(0).collect(&forecasts);

  EXPECT_EQ(forecasts.size(), 0u) << "Objects with a missing filter field must not match equals";
}

TEST_F(FilterArrayTest, NoMatchLeavesVectorEmpty) {
  respond(kForecasts);

  client.get("").filterArray("location.countryCode").equals("XX").collect(&forecasts);

  EXPECT_EQ(forecasts.size(), 0u) << "A filter matching no object must leave the target vector empty";
}

TEST_F(FilterArrayTest, CollectWithoutFilter) {
  respond(kForecasts);

  client.get("").collect(&forecasts);

  EXPECT_EQ(forecasts.size(), 3u) << "Collect without filter must collect every root array element";
}

class FilterArrayRootObjectTest : public FilterArrayTest {};

TEST_F(FilterArrayRootObjectTest, RootObjectTreatedAsSingleElement) {
  respond(kRootObject);

  client.get("").filterArray("location.countryCode").equals("CA").collect(&forecasts);

  ASSERT_EQ(forecasts.size(), 1u) << "A root object without arrayPath must be treated as a single element";
  EXPECT_STREQ(forecasts[0].city.c_str(), "Toronto") << "Root object must be collected";
}

TEST_F(FilterArrayRootObjectTest, NonMatchingRootObjectNotCollected) {
  respond(kRootObject);

  client.get("").filterArray("location.countryCode").equals("FR").collect(&forecasts);

  EXPECT_EQ(forecasts.size(), 0u) << "A non-matching root object must not be collected";
}

TEST_F(FilterArrayRootObjectTest, RootObjectWithoutFilter) {
  respond(kRootObject);

  client.get("").collect(&forecasts);

  ASSERT_EQ(forecasts.size(), 1u) << "A root object without filter must be collected as a single element";
  EXPECT_STREQ(forecasts[0].city.c_str(), "Toronto") << "Unfiltered root object must be collected";
}

TEST_F(FilterArrayRootObjectTest, RootObjectCollectedAsRawJsonArray) {
  respond(kRootObject);

  client.get("").filterArray("location.countryCode").equals("CA").collect(&raw);

  std::string expected = std::string("[") + kRootObject + "]";
  EXPECT_STREQ(raw.c_str(), expected.c_str()) << "A root object collected as String must be wrapped in a JSON array";
}

TEST_F(FilterArrayRootObjectTest, ObjectAtArrayPathTreatedAsSingleElement) {
  respond(R"({
    "data":{"location":{"city":"Toronto","countryCode":"CA"},"temperature":-2,"windSpeed":1.5,"storm":false,"kind":"a"}
  })");

  client.get("").filterArray("location.countryCode", "data").equals("CA").collect(&forecasts);

  ASSERT_EQ(forecasts.size(), 1u) << "An object at arrayPath must be treated as a single element";
  EXPECT_STREQ(forecasts[0].city.c_str(), "Toronto") << "Object at arrayPath must be collected";
}

class FilterArrayPathTest : public FilterArrayTest {};

TEST_F(FilterArrayPathTest, ArrayInsideObject) {
  respond(R"({
    "apiType":"rest",
    "data":[
      {"location":{"city":"Toronto","countryCode":"CA"},"temperature":-2,"windSpeed":1.5,"storm":false},
      {"location":{"city":"Paris","countryCode":"FR"},"temperature":9,"windSpeed":3.0,"storm":true}
    ]
  })");

  client.get("").filterArray("location.countryCode", "data").equals("CA").collect(&forecasts);

  ASSERT_EQ(forecasts.size(), 1u) << "Filtering an array at arrayPath must collect matching objects";
  EXPECT_STREQ(forecasts[0].city.c_str(), "Toronto") << "Array path filter must keep Toronto";
}

TEST_F(FilterArrayPathTest, NestedArrayPathWithIndices) {
  respond(R"({"r":[{"items":[{"id":1,"c":"CA"}]}]})");

  client.get("").filterArray("c", "r.0.items").equals("CA").collect(&raw);

  EXPECT_STREQ(raw.c_str(), R"([{"id":1,"c":"CA"}])") << "arrayPath must support nested array indices";
}

TEST_F(FilterArrayPathTest, FilterKeyThroughArrayIndex) {
  respond(R"([{"id":1,"tags":["hot","dry"]},{"id":2,"tags":["cold"]}])");

  client.get("").filterArray("tags.0").equals("hot").collect(&raw);

  EXPECT_STREQ(raw.c_str(), R"([{"id":1,"tags":["hot","dry"]}])") << "Filter keys must support array indices";
}

TEST_F(FilterArrayPathTest, EmptyArrayAtArrayPath) {
  respond(R"({"data":[]})");

  client.get("").filterArray("c", "data").equals("CA").collect(&raw);

  EXPECT_STREQ(raw.c_str(), "") << "An empty array at arrayPath must leave the target String unchanged";
}

TEST_F(FilterArrayPathTest, MissingArrayPathLeavesTargetUnchanged) {
  respond(R"({"data":[{"id":1,"c":"CA"}]})");
  raw = "untouched";

  client.get("").filterArray("c", "missing").equals("CA").collect(&raw);

  EXPECT_STREQ(raw.c_str(), "untouched") << "A missing arrayPath must leave the target String unchanged";
}

TEST_F(FilterArrayPathTest, FilterCombinedWithGetBody) {
  respond(R"({"count":3,"data":[{"id":1,"c":"CA"},{"id":2,"c":"FR"},{"id":3,"c":"CA"}]})");
  int count = 0;

  client.get("").getBody("count", &count).filterArray("c", "data").equals("CA").collect(&raw);

  EXPECT_EQ(count, 3) << "getBody must extract the requested field";
  EXPECT_STREQ(raw.c_str(), R"([{"id":1,"c":"CA"},{"id":3,"c":"CA"}])")
      << "Filtering and collect must work after getBody on the same request";
}

class FilterArrayStringCollectTest : public FilterArrayTest {};

TEST_F(FilterArrayStringCollectTest, ReturnsJsonArrayOfMatchingObjects) {
  respond(kIdCountry);

  client.get("").filterArray("c").equals("CA").collect(&raw);

  EXPECT_STREQ(raw.c_str(), R"([{"id":1,"c":"CA"},{"id":3,"c":"CA"}])")
      << "String collect must return a JSON array containing all matching objects";
}

TEST_F(FilterArrayStringCollectTest, NoMatchLeavesTargetUnchanged) {
  respond(kIdCountry);

  client.get("").filterArray("c").equals("XX").collect(&raw);

  EXPECT_STREQ(raw.c_str(), "") << "String collect must leave the target unchanged when no objects match";
}

TEST_F(FilterArrayStringCollectTest, RejectedFirstElementAddsNoLeadingComma) {
  respond(R"([{"id":1,"c":"FR"},{"id":2,"c":"CA"},{"id":3,"c":"CA"}])");

  client.get("").filterArray("c").equals("CA").collect(&raw);

  EXPECT_STREQ(raw.c_str(), R"([{"id":2,"c":"CA"},{"id":3,"c":"CA"}])")
      << "String collect must not add a leading comma when the first element is rejected";
}

TEST_F(FilterArrayStringCollectTest, NonObjectElementsAreSkipped) {
  respond(kMixedElements);

  client.get("").filterArray("c").equals("CA").collect(&raw);

  EXPECT_STREQ(raw.c_str(), R"([{"id":1,"c":"CA"}])")
      << "Filtering must skip non-object elements and collect matching objects";
}

class FilterArrayEdgeCaseTest : public FilterArrayTest {};

TEST_F(FilterArrayEdgeCaseTest, UnmappedFieldFilter) {
  respond(R"([
    {"location":{"city":"Toronto","countryCode":"CA"},"temperature":-2,"windSpeed":1.5,"storm":false,"kind":"a"},
    {"location":{"city":"Paris","countryCode":"FR"},"temperature":9,"windSpeed":3.0,"storm":true,"kind":"b"},
    {"location":{"city":"Montreal","countryCode":"CA"},"temperature":5,"windSpeed":2.0,"storm":false,"kind":"a"}
  ])");

  client.get("").filterArray("kind").equals("a").collect(&forecasts);

  ASSERT_EQ(forecasts.size(), 2u) << "Filtering on an unmapped field must collect matching objects";
  EXPECT_STREQ(forecasts[0].city.c_str(), "Toronto") << "First matching object must be Toronto";
  EXPECT_STREQ(forecasts[1].city.c_str(), "Montreal") << "Second matching object must be Montreal";
}

TEST_F(FilterArrayEdgeCaseTest, EscapedCharactersInStrings) {
  respond(R"([
    {"location":{"city":"Bad,}\"q","countryCode":"CA"},"temperature":1,"windSpeed":0.5,"storm":false},
    {"location":{"city":"Paris","countryCode":"FR"},"temperature":9,"windSpeed":3.0,"storm":true},
    {"location":{"city":"Montreal","countryCode":"CA"},"temperature":5,"windSpeed":2.0,"storm":false}
  ])");

  client.get("").filterArray("location.countryCode").equals("CA").collect(&forecasts);

  ASSERT_EQ(forecasts.size(), 2u) << "Escaped characters must not interrupt JSON filtering";
  EXPECT_STREQ(forecasts[0].city.c_str(), "Bad,}\"q") << "Escaped characters must be correctly parsed";
  EXPECT_STREQ(forecasts[1].city.c_str(), "Montreal") << "Parsing must continue after an escaped string";
}

TEST_F(FilterArrayEdgeCaseTest, FilterWithoutConditionKeepsEmptyValues) {
  respond(R"([
    {"location":{"city":"Toronto","countryCode":"CA"},"temperature":-2,"windSpeed":1.5,"storm":false},
    {"location":{"city":"Paris","countryCode":""},"temperature":9,"windSpeed":3.0,"storm":true}
  ])");

  client.get("").filterArray("location.countryCode").collect(&forecasts);

  EXPECT_EQ(forecasts.size(), 2u) << "A filter without a comparison condition must not exclude objects";
}

TEST_F(FilterArrayEdgeCaseTest, FailedRequestLeavesStringUnchanged) {
  raw = "untouched";
  HttpClientStub::setResponse(500, R"(error)");

  client.get("").filterArray("countryCode").equals("CA").collect(&raw);

  EXPECT_STREQ(raw.c_str(), "untouched") << "Failed requests must not modify the target String";
}

TEST_F(FilterArrayEdgeCaseTest, FailedRequestLeavesVectorUnchanged) {
  HttpClientStub::setResponse(500, R"(error)");

  client.get("").filterArray("location.countryCode").equals("CA").collect(&forecasts);

  EXPECT_EQ(forecasts.size(), 0u) << "Failed requests must not modify the target vector";
}

TEST_F(FilterArrayEdgeCaseTest, StringAtRootLeavesTargetUnchanged) {
  raw = "untouched";
  respond(R"("hello")");

  client.get("").filterArray("c").equals("CA").collect(&raw);

  EXPECT_STREQ(raw.c_str(), "untouched") << "A string at the root must leave the target unchanged";
}

TEST_F(FilterArrayEdgeCaseTest, NumberAtRootLeavesTargetUnchanged) {
  raw = "untouched";
  respond("42");

  client.get("").filterArray("c").equals("CA").collect(&raw);

  EXPECT_STREQ(raw.c_str(), "untouched") << "A number at the root must leave the target unchanged";
}

TEST_F(FilterArrayEdgeCaseTest, PrimitiveAtRootLeavesVectorEmpty) {
  respond("42");

  client.get("").filterArray("c").equals("CA").collect(&forecasts);

  EXPECT_EQ(forecasts.size(), 0u) << "A number at the root must leave the target vector empty";
}

TEST_F(FilterArrayEdgeCaseTest, EmptyArrayWithFilterLeavesVectorEmpty) {
  respond(R"([])");

  client.get("").filterArray("property").equals("value").collect(&forecasts);

  EXPECT_EQ(forecasts.size(), 0u) << "An empty array must leave the target vector unchanged";
}

TEST_F(FilterArrayEdgeCaseTest, EmptyArrayWithoutFilterLeavesVectorEmpty) {
  respond(R"([])");

  client.get("").collect(&forecasts);

  EXPECT_EQ(forecasts.size(), 0u) << "An empty array without filter must leave the target vector unchanged";
}

TEST_F(FilterArrayEdgeCaseTest, EmptyArrayLeavesStringUnchanged) {
  respond(R"([])");

  client.get("").filterArray("c").equals("CA").collect(&raw);

  EXPECT_STREQ(raw.c_str(), "") << "An empty array must leave the target String unchanged";
}

TEST_F(FilterArrayEdgeCaseTest, NumericJsonValueMappedToString) {
  std::vector<TempByCityTestStruct> temps;
  respond(R"([{"temperature":5,"city":"A"},{"temperature":9,"city":"B"}])");

  client.get("").filterArray("temperature").equals(5).collect(&temps);

  ASSERT_EQ(temps.size(), 1u) << "Filtering must work when the mapped field type differs from the filter type";
  EXPECT_STREQ(temps[0].temperature.c_str(), "5") << "Numeric JSON value must be converted to the mapped String field";
}

class FilterArrayStreamTest : public FilterArrayTest {};

TEST_F(FilterArrayStreamTest, LargeNonChunkedResponse) {
  std::string largePayload(700, 'X');
  std::string largeJson = "[{\"id\":1,\"c\":\"FR\",\"large\":\"" + largePayload + "\"},{\"id\":2,\"c\":\"CA\"}]";

  MockDirectStream stream(largeJson);
  HttpClientStub::customStream = &stream;
  HttpClientStub::customSize = static_cast<int>(largeJson.size());

  client.get("").filterArray("c").equals("CA").collect(&raw);

  EXPECT_STREQ(raw.c_str(), R"([{"id":2,"c":"CA"}])") << "Filtering must work when the response exceeds BUF_SIZE";
}

TEST_F(FilterArrayStreamTest, ChunkedTransferEncoding) {
  std::string payload = R"([{"id":1,"c":"FR"},{"id":2,"c":"CA"}])";
  std::string chunked = "\r\n25;ext=test\r\n" + payload + "\r\n0\r\n\r\n";

  MockDirectStream stream(chunked);
  HttpClientStub::customStream = &stream;
  HttpClientStub::customSize = -1;

  client.get("").filterArray("c").equals("CA").collect(&raw);

  EXPECT_STREQ(raw.c_str(), R"([{"id":2,"c":"CA"}])") << "Filtering must work with chunked transfer encoding";
}