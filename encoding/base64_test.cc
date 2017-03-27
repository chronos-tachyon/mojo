// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include "encoding/base64.h"
#include "gtest/gtest.h"

TEST(Base64, Encode) {
  EXPECT_EQ("", encode(encoding::BASE64, ""));
  EXPECT_EQ("VGhlIHF1aWNrIGJyb3duIGZveCBqdW1wcyBvdmVyIHRoZSBsYXp5IGRvZy4K",
            encode(encoding::BASE64,
                   "The quick brown fox jumps over the lazy dog.\n"));
  EXPECT_EQ("VGhlIHF1aWNrIGJyb3duIGZveAo=",
            encode(encoding::BASE64, "The quick brown fox\n"));
  EXPECT_EQ("VGhlIHF1aWNrIGJyb3duIGZvCg==",
            encode(encoding::BASE64, "The quick brown fo\n"));

  EXPECT_EQ("", encode(encoding::BASE64_NOPAD, ""));
  EXPECT_EQ("VGhlIHF1aWNrIGJyb3duIGZveCBqdW1wcyBvdmVyIHRoZSBsYXp5IGRvZy4K",
            encode(encoding::BASE64_NOPAD,
                   "The quick brown fox jumps over the lazy dog.\n"));
  EXPECT_EQ("VGhlIHF1aWNrIGJyb3duIGZveAo",
            encode(encoding::BASE64_NOPAD, "The quick brown fox\n"));
  EXPECT_EQ("VGhlIHF1aWNrIGJyb3duIGZvCg",
            encode(encoding::BASE64_NOPAD, "The quick brown fo\n"));
}

static std::pair<bool, std::string> boolstr(bool b, std::string s) {
  return std::make_pair(b, std::move(s));
}

TEST(Base64, Decode) {
  EXPECT_EQ(boolstr(true, ""), decode(encoding::BASE64, ""));

  EXPECT_EQ(
      boolstr(true, "The quick brown fox jumps over the lazy dog.\n"),
      decode(encoding::BASE64,
             "VGhlIHF1aWNrIGJyb3duIGZveCBqdW1wcyBvdmVyIHRoZSBsYXp5IGRvZy4K"));

  EXPECT_EQ(
      boolstr(true, "The quick brown fox jumps over the lazy dog.\n"),
      decode(encoding::BASE64,
             "VGhl IHF1 aWNr IGJy b3du IGZv eCBq dW1w cyBv dmVy IHRo ZSBs YXp5 IGRv Zy4K"));

  EXPECT_EQ(boolstr(true, "The quick brown fox\n"),
            decode(encoding::BASE64, "VGhlIHF1aWNrIGJyb3duIGZveAo="));
  EXPECT_EQ(boolstr(true, "The quick brown fox\n"),
            decode(encoding::BASE64, "VGhlIHF1aWNrIGJyb3duIGZveAo"));
  EXPECT_EQ(boolstr(true, "The quick brown fo\n"),
            decode(encoding::BASE64, "VGhlIHF1aWNrIGJyb3duIGZvCg=="));
  EXPECT_EQ(boolstr(true, "The quick brown fo\n"),
            decode(encoding::BASE64, "VGhlIHF1aWNrIGJyb3duIGZvCg"));
}
