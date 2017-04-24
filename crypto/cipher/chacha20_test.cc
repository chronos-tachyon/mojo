// Copyright © 2017 by Donald King <chronos@chronos-tachyon.net>
// Available under the MIT License. See LICENSE for details.

#include <string>
#include <vector>

#include "base/result_testing.h"
#include "crypto/cipher/chacha20.h"
#include "crypto/crypto.h"
#include "encoding/hex.h"
#include "gtest/gtest.h"

TEST(ChaCha20, Encrypt) {
  std::vector<uint8_t> key;
  std::vector<uint8_t> nonce;
  std::vector<uint8_t> vec;
  std::string out;
  std::string tmp;
  std::unique_ptr<crypto::Crypter> crypter;

  // https://tools.ietf.org/html/rfc7539#appendix-A.1
  //
  // Test Vector #1
  // Natural Flow
  key.resize(32);
  nonce.resize(12);
  crypter = crypto::cipher::new_chacha20(key, nonce);

  vec.resize(64);
  crypter->encrypt(vec, vec);
  out = encode(encoding::HEX, vec);
  EXPECT_EQ(
      "76b8e0ada0f13d90405d6ae55386bd28"
      "bdd219b8a08ded1aa836efcc8b770dc7"
      "da41597c5157488d7724e03fb8d84a37"
      "6a43b8f41518a11cc387b669b2ee6586",
      out);

  // Test Vector #2
  // Natural Flow
  ::bzero(vec.data(), vec.size());
  crypter->encrypt(vec, vec);
  out = encode(encoding::HEX, vec);
  EXPECT_EQ(
      "9f07e7be5551387a98ba977c732d080d"
      "cb0f29a048e3656912c6533e32ee7aed"
      "29b721769ce64e43d57133b074d839d5"
      "31ed1f28510afb45ace10a1f4b794d6f",
      out);

  // Test Vector #1
  // Seek Flow
  EXPECT_OK(crypter->seek(0, SEEK_SET));
  ::bzero(vec.data(), vec.size());
  crypter->encrypt(vec, vec);
  out = encode(encoding::HEX, vec);
  EXPECT_EQ(
      "76b8e0ada0f13d90405d6ae55386bd28"
      "bdd219b8a08ded1aa836efcc8b770dc7"
      "da41597c5157488d7724e03fb8d84a37"
      "6a43b8f41518a11cc387b669b2ee6586",
      out);

  // Test Vector #2
  // Seek Flow
  EXPECT_OK(crypter->seek(64, SEEK_SET));
  ::bzero(vec.data(), vec.size());
  crypter->encrypt(vec, vec);
  out = encode(encoding::HEX, vec);
  EXPECT_EQ(
      "9f07e7be5551387a98ba977c732d080d"
      "cb0f29a048e3656912c6533e32ee7aed"
      "29b721769ce64e43d57133b074d839d5"
      "31ed1f28510afb45ace10a1f4b794d6f",
      out);

  // Test Vector #3
  key[31] = 0x01;
  crypter = crypto::cipher::new_chacha20(key, nonce);
  EXPECT_OK(crypter->seek(64, SEEK_SET));

  ::bzero(vec.data(), vec.size());
  crypter->encrypt(vec, vec);
  out = encode(encoding::HEX, vec);
  EXPECT_EQ(
      "3aeb5224ecf849929b9d828db1ced4dd"
      "832025e8018b8160b82284f3c949aa5a"
      "8eca00bbb4a73bdad192b5c42f73f2fd"
      "4e273644c8b36125a64addeb006c13a0",
      out);

  // Test Vector #4
  key[1] = 0xff;
  key[31] = 0x00;
  crypter = crypto::cipher::new_chacha20(key, nonce);
  EXPECT_OK(crypter->seek(128, SEEK_SET));

  ::bzero(vec.data(), vec.size());
  crypter->encrypt(vec, vec);
  out = encode(encoding::HEX, vec);
  EXPECT_EQ(
      "72d54dfbf12ec44b362692df94137f32"
      "8fea8da73990265ec1bbbea1ae9af0ca"
      "13b25aa26cb4a648cb9b9d1be65b2c09"
      "24a66c54d545ec1b7374f4872e99f096",
      out);

  // Test Vector #5
  key[1] = 0x00;
  nonce[11] = 0x02;
  crypter = crypto::cipher::new_chacha20(key, nonce);

  ::bzero(vec.data(), vec.size());
  crypter->encrypt(vec, vec);
  out = encode(encoding::HEX, vec);
  EXPECT_EQ(
      "c2c64d378cd536374ae204b9ef933fcd"
      "1a8b2288b3dfa49672ab765b54ee27c7"
      "8a970e0e955c14f3a88e741b97c286f7"
      "5f8fc299e8148362fa198a39531bed6d",
      out);

  // https://tools.ietf.org/html/rfc7539#section-2.3.2
  for (std::size_t i = 0; i < 32; ++i) {
    key[i] = i;
  }
  ::bzero(nonce.data(), 12);
  nonce[3] = 0x09;
  nonce[7] = 0x4a;
  crypter = crypto::cipher::new_chacha20(key, nonce);
  EXPECT_OK(crypter->seek(64, SEEK_SET));

  vec.resize(64);
  ::bzero(vec.data(), vec.size());
  crypter->encrypt(vec, vec);
  out = encode(encoding::HEX, vec);
  EXPECT_EQ(
      "10f1e7e4d13b5915500fdd1fa32071c4"
      "c7d1f4c733c068030422aa9ac3d46c4e"
      "d2826446079faa0914c2d705d98b02a2"
      "b5129cd1de164eb9cbd083e8a2503c4e",
      out);

  // https://tools.ietf.org/html/rfc7539#section-2.4.2
  nonce[3] = 0;
  crypter = crypto::cipher::new_chacha20(key, nonce);
  EXPECT_OK(crypter->seek(64, SEEK_SET));

  tmp =
      "Ladies and Gentl"
      "emen of the clas"
      "s of '99: If I c"
      "ould offer you o"
      "nly one tip for "
      "the future, suns"
      "creen would be i"
      "t.";
  vec.resize(tmp.size());
  ::memcpy(vec.data(), tmp.data(), tmp.size());
  crypter->encrypt(vec, vec);

  out = encode(encoding::HEX, vec);
  EXPECT_EQ(
      "6e2e359a2568f98041ba0728dd0d6981"
      "e97e7aec1d4360c20a27afccfd9fae0b"
      "f91b65c5524733ab8f593dabcd62b357"
      "1639d624e65152ab8f530c359f0861d8"
      "07ca0dbf500d6a6156a38e088a22b65e"
      "52bc514d16ccf806818ce91ab7793736"
      "5af90bbf74a35be6b40b8eedf2785e42"
      "874d",
      out);

  // https://tools.ietf.org/html/rfc7539#appendix-A.2
  //
  // Test Vector #2
  ::bzero(key.data(), 32);
  ::bzero(nonce.data(), 12);
  key[31] = 0x01;
  nonce[11] = 0x02;
  crypter = crypto::cipher::new_chacha20(key, nonce);
  EXPECT_OK(crypter->seek(64, SEEK_SET));

  tmp =
      "Any submission t"
      "o the IETF inten"
      "ded by the Contr"
      "ibutor for publi"
      "cation as all or"
      " part of an IETF"
      " Internet-Draft "
      "or RFC and any s"
      "tatement made wi"
      "thin the context"
      " of an IETF acti"
      "vity is consider"
      "ed an \"IETF Cont"
      "ribution\". Such "
      "statements inclu"
      "de oral statemen"
      "ts in IETF sessi"
      "ons, as well as "
      "written and elec"
      "tronic communica"
      "tions made at an"
      "y time or place,"
      " which are addre"
      "ssed to";
  vec.resize(tmp.size());
  ::memcpy(vec.data(), tmp.data(), tmp.size());
  crypter->encrypt(vec, vec);
  out = encode(encoding::HEX, vec);
  EXPECT_EQ(
      "a3fbf07df3fa2fde4f376ca23e827370"
      "41605d9f4f4f57bd8cff2c1d4b7955ec"
      "2a97948bd3722915c8f3d337f7d37005"
      "0e9e96d647b7c39f56e031ca5eb6250d"
      "4042e02785ececfa4b4bb5e8ead0440e"
      "20b6e8db09d881a7c6132f420e527950"
      "42bdfa7773d8a9051447b3291ce1411c"
      "680465552aa6c405b7764d5e87bea85a"
      "d00f8449ed8f72d0d662ab052691ca66"
      "424bc86d2df80ea41f43abf937d3259d"
      "c4b2d0dfb48a6c9139ddd7f76966e928"
      "e635553ba76c5c879d7b35d49eb2e62b"
      "0871cdac638939e25e8a1e0ef9d5280f"
      "a8ca328b351c3c765989cbcf3daa8b6c"
      "cc3aaf9f3979c92b3720fc88dc95ed84"
      "a1be059c6499b9fda236e7e818b04b0b"
      "c39c1e876b193bfe5569753f88128cc0"
      "8aaa9b63d1a16f80ef2554d7189c411f"
      "5869ca52c5b83fa36ff216b9c1d30062"
      "bebcfd2dc5bce0911934fda79a86f6e6"
      "98ced759c3ff9b6477338f3da4f9cd85"
      "14ea9982ccafb341b2384dd902f3d1ab"
      "7ac61dd29c6f21ba5b862f3730e37cfd"
      "c4fd806c22f221",
      out);

  // Test Vector #3
  tmp =
    "1c9240a5eb55d38af333888604f6b5f0"
    "473917c1402b80099dca5cbc207075c0";
  decode_to(encoding::HEX, key, tmp);
  ::bzero(nonce.data(), 12);
  nonce[11] = 0x02;
  crypter = crypto::cipher::new_chacha20(key, nonce);
  EXPECT_OK(crypter->seek(64 * 42, SEEK_SET));

  tmp =
    "'Twas brillig, a"
    "nd the slithy to"
    "ves\nDid gyre and"
    " gimble in the w"
    "abe:\nAll mimsy w"
    "ere the borogove"
    "s,\nAnd the mome "
    "raths outgrabe.";
  vec.resize(tmp.size());
  ::memcpy(vec.data(), tmp.data(), tmp.size());
  crypter->encrypt(vec, vec);
  out = encode(encoding::HEX, vec);
  EXPECT_EQ(
      "62e6347f95ed87a45ffae7426f27a1df"
      "5fb69110044c0d73118effa95b01e5cf"
      "166d3df2d721caf9b21e5fb14c616871"
      "fd84c54f9d65b283196c7fe4f60553eb"
      "f39c6402c42234e32a356b3e764312a6"
      "1a5532055716ead6962568f87d3f3f77"
      "04c6a8d1bcd1bf4d50d6154b6da731b1"
      "87b58dfd728afa36757a797ac188d1",
      out);
}
