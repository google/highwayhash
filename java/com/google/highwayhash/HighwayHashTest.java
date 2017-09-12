package com.google.highwayhash;

public class HighwayHashTest {
  private static void testHash64(long expected, byte[] data, int length, long[] key) {
    long hash = HighwayHash.hash64(data, 0, length, key);
    if (expected != hash) {
      System.out.println("Test failed: expected " + expected
          + ", got "  + hash + " size: " + length);
      throw new IllegalStateException("Test failed");
    }
  }

  private static  void testKnownValuesWithKey1234() {
    long[] key = {1, 2, 3, 4};

    byte[] b = new byte[33];
    for (int i = 0; i < b.length; i++) {
      b[i] = (byte) (128 + i);
    }

    testHash64(0x53c516cce478cad7L, b, 33, key);
    testHash64(0x7858f24d2d79b2b2L, new byte[] {-1}, 1, key);
  }

  private static void testArrays() {
    // HighwayHash results for byte strings [], [0], [0,1], [0,1,2], etc...,
    // these match the values in the C++ test of HighwayHash at
    // https://github.com/google/highwayhash/blob/master/highwayhash/highwayhash_test.cc
    long[] expected64 = {
      0x907A56DE22C26E53L, 0x7EAB43AAC7CDDD78L, 0xB8D0569AB0B53D62L,
      0x5C6BEFAB8A463D80L, 0xF205A46893007EDAL, 0x2B8A1668E4A94541L,
      0xBD4CCC325BEFCA6FL, 0x4D02AE1738F59482L, 0xE1205108E55F3171L,
      0x32D2644EC77A1584L, 0xF6E10ACDB103A90BL, 0xC3BBF4615B415C15L,
      0x243CC2040063FA9CL, 0xA89A58CE65E641FFL, 0x24B031A348455A23L,
      0x40793F86A449F33BL, 0xCFAB3489F97EB832L, 0x19FE67D2C8C5C0E2L,
      0x04DD90A69C565CC2L, 0x75D9518E2371C504L, 0x38AD9B1141D3DD16L,
      0x0264432CCD8A70E0L, 0xA9DB5A6288683390L, 0xD7B05492003F028CL,
      0x205F615AEA59E51EL, 0xEEE0C89621052884L, 0x1BFC1A93A7284F4FL,
      0x512175B5B70DA91DL, 0xF71F8976A0A2C639L, 0xAE093FEF1F84E3E7L,
      0x22CA92B01161860FL, 0x9FC7007CCF035A68L, 0xA0C964D9ECD580FCL,
      0x2C90F73CA03181FCL, 0x185CF84E5691EB9EL, 0x4FC1F5EF2752AA9BL,
      0xF5B7391A5E0A33EBL, 0xB9B84B83B4E96C9CL, 0x5E42FE712A5CD9B4L,
      0xA150F2F90C3F97DCL, 0x7FA522D75E2D637DL, 0x181AD0CC0DFFD32BL,
      0x3889ED981E854028L, 0xFB4297E8C586EE2DL, 0x6D064A45BB28059CL,
      0x90563609B3EC860CL, 0x7AA4FCE94097C666L, 0x1326BAC06B911E08L,
      0xB926168D2B154F34L, 0x9919848945B1948DL, 0xA2A98FC534825EBEL,
      0xE9809095213EF0B6L, 0x582E5483707BC0E9L, 0x086E9414A88A6AF5L,
      0xEE86B98D20F6743DL, 0xF89B7FF609B1C0A7L, 0x4C7D9CC19E22C3E8L,
      0x9A97005024562A6FL, 0x5DD41CF423E6EBEFL, 0xDF13609C0468E227L,
      0x6E0DA4F64188155AL, 0xB755BA4B50D7D4A1L, 0x887A3484647479BDL,
      0xAB8EEBE9BF2139A0L, 0x75542C5D4CD2A6FFL
    };

    byte[] data = new byte[65];
    long[] key = {0x0706050403020100L, 0x0F0E0D0C0B0A0908L,
        0x1716151413121110L, 0x1F1E1D1C1B1A1918L};

    for (int i = 0; i <= 64; i++) {
      data[i] = (byte) i;
    }

    for (int i = 0; i <= 64; i++) {
      testHash64(expected64[i], data, i, key);
    }
  }

  public static void main(String[] args) {

    // 128-bit and 256-bit tests to be added when they are declared frozen in the C++ version

    testKnownValuesWithKey1234();
    testArrays();

    System.out.println("Test success");
  }
}
