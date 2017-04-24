void AESState::expand_generic(const uint8_t* key, std::size_t len) {
  uint32_t nk = (len / 4);
  uint32_t n = num_rounds * 4;
  for (uint32_t i = 0; i < nk; ++i) {
    enc.u32[i] = RBE32(key, i);
  }
  for (uint32_t i = nk; i < n; ++i) {
    uint32_t temp = enc.u32[i - 1];
    uint32_t p = (i / nk);
    uint32_t q = (i % nk);
    if (q == 0) {
      temp = S0(ROL32(temp, 8)) ^ (uint32_t(POW_X[p - 1]) << 24);
    } else if (nk == 8 && q == 4) {
      temp = S0(temp);
    }
    enc.u32[i] = enc.u32[i - nk] ^ temp;
  }

  for (uint32_t i = 0; i < n; i += 4) {
    uint32_t ei = n - (i + 4);
    for (uint32_t j = 0; j < 4; ++j) {
      uint32_t x = enc.u32[ei + j];
      if (i > 0 && (i + 4) < n) {
        x = TD(S0(x));
      }
      dec.u32[i + j] = x;
    }
  }
}

void AESState::encrypt_generic(uint8_t* dst, const uint8_t* src,
                               std::size_t len) const {
  uint32_t s0, s1, s2, s3;
  uint32_t t0, t1, t2, t3;
  uint32_t index;

  while (len >= 16) {
    // Round 1: just XOR
    s0 = enc.u32[0] ^ RBE32(src, 0);
    s1 = enc.u32[1] ^ RBE32(src, 1);
    s2 = enc.u32[2] ^ RBE32(src, 2);
    s3 = enc.u32[3] ^ RBE32(src, 3);

    // Rounds 2 .. N - 1: shuffle and XOR
    index = 4;
    for (uint32_t i = 2; i < num_rounds; ++i) {
      t0 = s0;
      t1 = s1;
      t2 = s2;
      t3 = s3;
      s0 = enc.u32[index + 0] ^ TE(t0, t1, t2, t3);
      s1 = enc.u32[index + 1] ^ TE(t1, t2, t3, t0);
      s2 = enc.u32[index + 2] ^ TE(t2, t3, t0, t1);
      s3 = enc.u32[index + 3] ^ TE(t3, t0, t1, t2);
      index += 4;
    }

    // Round N: S-box and XOR
    t0 = s0;
    t1 = s1;
    t2 = s2;
    t3 = s3;
    s0 = enc.u32[index + 0] ^ S0(t0, t1, t2, t3);
    s1 = enc.u32[index + 1] ^ S0(t1, t2, t3, t0);
    s2 = enc.u32[index + 2] ^ S0(t2, t3, t0, t1);
    s3 = enc.u32[index + 3] ^ S0(t3, t0, t1, t2);

    WBE32(dst, 0, s0);
    WBE32(dst, 1, s1);
    WBE32(dst, 2, s2);
    WBE32(dst, 3, s3);

    src += 16;
    dst += 16;
    len -= 16;
  }
  DCHECK_EQ(len, 0U);
}

void AESState::decrypt_generic(uint8_t* dst, const uint8_t* src,
                               std::size_t len) const {
  while (len >= 16) {
    // Round 1: just XOR
    uint32_t s0 = dec.u32[0] ^ RBE32(src, 0);
    uint32_t s1 = dec.u32[1] ^ RBE32(src, 1);
    uint32_t s2 = dec.u32[2] ^ RBE32(src, 2);
    uint32_t s3 = dec.u32[3] ^ RBE32(src, 3);

    uint32_t t0, t1, t2, t3;

    // Rounds 2 .. N - 1: shuffle and XOR
    uint32_t i = 4;
    for (uint32_t round = 2; round < num_rounds; ++round) {
      t0 = s0;
      t1 = s1;
      t2 = s2;
      t3 = s3;
      s0 = dec.u32[i + 0] ^ TD(t0, t3, t2, t1);
      s1 = dec.u32[i + 1] ^ TD(t1, t0, t3, t2);
      s2 = dec.u32[i + 2] ^ TD(t2, t1, t0, t3);
      s3 = dec.u32[i + 3] ^ TD(t3, t2, t1, t0);
      i += 4;
    }

    // Round N: S-box and XOR
    t0 = s0;
    t1 = s1;
    t2 = s2;
    t3 = s3;
    s0 = dec.u32[i + 0] ^ S1(t0, t3, t2, t1);
    s1 = dec.u32[i + 1] ^ S1(t1, t0, t3, t2);
    s2 = dec.u32[i + 2] ^ S1(t2, t1, t0, t3);
    s3 = dec.u32[i + 3] ^ S1(t3, t2, t1, t0);

    WBE32(dst, 0, s0);
    WBE32(dst, 1, s1);
    WBE32(dst, 2, s2);
    WBE32(dst, 3, s3);

    src += 16;
    dst += 16;
    len -= 16;
  }
  DCHECK_EQ(len, 0U);
}
