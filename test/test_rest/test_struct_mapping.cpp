#include "test_support.h"

namespace {

struct UserTestStruct {
  int id = 0;
  char name[32] = {0};
  float score = 0.0f;
  bool active = false;
  String email = "";

  REST_JSON_MAP(REST_FIELD(id), REST_FIELD(name), REST_FIELD(score), REST_FIELD(active), REST_FIELD(email))
};

struct CustomKeyStruct {
  int userId = 0;
  char fullName[32] = {0};

  REST_JSON_MAP(REST_FIELD_NAMED("user_id", userId), REST_FIELD_NAMED("full_name", fullName))
};

struct ExternalUserStruct {
  int id = 0;
  String role = "";
};

REST_JSON_MAP_EXT(ExternalUserStruct, REST_FIELD_EXT(id), REST_FIELD_EXT(role))

struct NullStrStruct {
  const char* text = nullptr;

  REST_JSON_MAP(REST_FIELD(text))
};

}  // namespace

class StructMappingTest : public RequestFixture {};

TEST_F(StructMappingTest, SerializationToJson) {
  UserTestStruct user;
  user.id = 15;
  strncpy(user.name, "Pedro", sizeof(user.name));
  user.score = 9.5f;
  user.active = true;
  user.email = "pedro@test.com";

  String json = ESP32HTTPClient::toJson(user);

  EXPECT_STREQ(json.c_str(), R"({"id":15,"name":"Pedro","score":9.5,"active":true,"email":"pedro@test.com"})");
  EXPECT_STREQ(RestJson::toJson(user).c_str(), json.c_str()) << "RestJson::toJson matches ESP32HTTPClient::toJson";
}

TEST_F(StructMappingTest, NullStringFieldSerializesToNull) {
  NullStrStruct s;

  String json = ESP32HTTPClient::toJson(s);

  EXPECT_STREQ(json.c_str(), R"({"text":null})");
}

TEST_F(StructMappingTest, CustomKeysSerialization) {
  CustomKeyStruct custom;
  custom.userId = 55;
  strncpy(custom.fullName, "Custom Name", sizeof(custom.fullName));

  EXPECT_STREQ(ESP32HTTPClient::toJson(custom).c_str(), R"({"user_id":55,"full_name":"Custom Name"})");
}

TEST_F(StructMappingTest, ExternalMappingSerialization) {
  ExternalUserStruct ext;
  ext.id = 12;
  ext.role = "admin";

  EXPECT_STREQ(ESP32HTTPClient::toJson(ext).c_str(), R"({"id":12,"role":"admin"})");
}

TEST_F(StructMappingTest, DeserializationFromJson) {
  UserTestStruct user;

  bool ok =
      ESP32HTTPClient::fromJson(R"({"id":42,"name":"Ana","score":8.75,"active":true,"email":"ana@domain.com"})", &user);

  EXPECT_TRUE(ok);
  EXPECT_EQ(user.id, 42);
  EXPECT_STREQ(user.name, "Ana");
  EXPECT_NEAR(user.score, 8.75f, 0.001f);
  EXPECT_TRUE(user.active);
  EXPECT_STREQ(user.email.c_str(), "ana@domain.com");
}

TEST_F(StructMappingTest, MissingFieldsKeepDefaults) {
  UserTestStruct user;
  user.id = 0;
  strncpy(user.name, "DefaultName", sizeof(user.name));
  user.score = 5.0f;
  user.active = true;
  user.email = "default@email.com";

  bool ok = ESP32HTTPClient::fromJson(R"({"id":100,"extra_unmapped":"ignored","tags":[1,2,3]})", &user);

  EXPECT_TRUE(ok) << "partially matching json must still parse";
  EXPECT_EQ(user.id, 100) << "present field is updated";
  EXPECT_STREQ(user.name, "DefaultName") << "missing name preserved";
  EXPECT_NEAR(user.score, 5.0f, 0.001f) << "missing score preserved";
  EXPECT_TRUE(user.active) << "missing active preserved";
  EXPECT_STREQ(user.email.c_str(), "default@email.com") << "missing email preserved";
}

TEST_F(StructMappingTest, NullFieldsResetToZeroValues) {
  UserTestStruct user;
  user.id = 99;
  strncpy(user.name, "Old", sizeof(user.name));
  user.score = 7.0f;
  user.active = true;
  user.email = "old@test.com";

  bool ok = ESP32HTTPClient::fromJson(R"({"id":null,"name":null,"score":null,"active":null,"email":null})", &user);

  EXPECT_TRUE(ok) << "null values must parse";
  EXPECT_EQ(user.id, 0) << "null id set to 0";
  EXPECT_STREQ(user.name, "") << "null char array set to empty string";
  EXPECT_NEAR(user.score, 0.0f, 0.001f) << "null score set to 0";
  EXPECT_FALSE(user.active) << "null active set to false";
  EXPECT_STREQ(user.email.c_str(), "") << "null String set to empty";
}

TEST_F(StructMappingTest, CustomKeysDeserialization) {
  CustomKeyStruct customIn;

  ESP32HTTPClient::fromJson(R"({"user_id":77,"full_name":"Another Name"})", &customIn);

  EXPECT_EQ(customIn.userId, 77);
  EXPECT_STREQ(customIn.fullName, "Another Name");
}

TEST_F(StructMappingTest, ExternalMappingDeserialization) {
  ExternalUserStruct extIn;

  ESP32HTTPClient::fromJson(R"({"id":34,"role":"guest"})", &extIn);

  EXPECT_EQ(extIn.id, 34);
  EXPECT_STREQ(extIn.role.c_str(), "guest");
}

TEST_F(StructMappingTest, StructAsRequestBody) {
  UserTestStruct user;
  user.id = 7;
  strncpy(user.name, "Lucas", sizeof(user.name));
  user.score = 10.0f;
  user.active = false;
  user.email = "lucas@test.com";

  int resId = 0;
  HttpClientStub::setResponse(201, R"({"id":7})");
  client.post("/users").body(user).getBody("id", &resId);

  EXPECT_EQ(HttpClientStub::lastUrl, url("/users"));
  EXPECT_EQ(HttpClientStub::lastPayload,
            R"({"id":7,"name":"Lucas","score":10,"active":false,"email":"lucas@test.com"})")
      << "struct serialized as post payload";
  EXPECT_EQ(resId, 7) << "response bound alongside the struct body";
}

TEST_F(StructMappingTest, GetBodyBindsWholeStruct) {
  HttpClientStub::setResponse(200, R"({"id":88,"name":"Julia","score":9.9,"active":true,"email":"j@test.com"})");

  UserTestStruct user;
  client.get("/users/88").getBody(&user);

  EXPECT_EQ(user.id, 88);
  EXPECT_STREQ(user.name, "Julia");
  EXPECT_TRUE(user.active);
  EXPECT_STREQ(user.email.c_str(), "j@test.com");
}

TEST_F(StructMappingTest, GetBodyBindsStructAtNestedPath) {
  HttpClientStub::setResponse(
      200,
      R"({"status":"success","data":{"user":{"id":99,"name":"Carlos","score":8.0,"active":false,"email":"c@test.com"}}})");

  UserTestStruct nestedUser;
  client.get("/profile").getBody("data.user", &nestedUser);

  EXPECT_EQ(nestedUser.id, 99);
  EXPECT_STREQ(nestedUser.name, "Carlos");
  EXPECT_FALSE(nestedUser.active);
  EXPECT_STREQ(nestedUser.email.c_str(), "c@test.com");
}