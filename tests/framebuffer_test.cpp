#include <algorithm>
#include <array>
#include <cstdio>
#include <exception>
#include <print>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

import dashboard.framebuffer;

using namespace std;

namespace
{

void require(bool condition, string_view message)
{
  if (!condition)
  {
    throw runtime_error(string(message));
  }
}

bool black(const dashboard::mono_image &image, int x, int y)
{
  return !(image.pixels[y * 100 + x / 8] & (0x80 >> (x % 8)));
}

void test_glyphs()
{
  for (char32_t character = U'!'; character <= U'~'; ++character)
  {
    const auto rows = dashboard::glyph(character);
    require(any_of(rows.begin(), rows.end(), [](auto row) { return row != 0; }),
            "A printable ASCII character is blank");
    require(all_of(rows.begin(), rows.end(), [](auto row) { return row < 32; }),
            "A glyph extends beyond its five-pixel cell");
    if (character != U'?')
    {
      require(rows != dashboard::glyph(U'?'),
              "An ASCII character uses the fallback");
    }
  }
  const auto space = dashboard::glyph(U' ');
  require(all_of(space.begin(), space.end(), [](auto row) { return row == 0; }),
          "Space must be blank");
  for (char32_t character : U"ÅÄÖåäöÉé°€–—‘’“”…•")
  {
    if (character == U'\0')
    {
      continue;
    }
    require(dashboard::glyph(character) != dashboard::glyph(U'?'),
            "A dashboard character uses the fallback");
  }
  require(dashboard::glyph(U'a') != dashboard::glyph(U'A'),
          "Lowercase must have its own glyphs");
  require(dashboard::glyph(U'\U0001f600') == dashboard::glyph(U'?'),
          "Unsupported Unicode must use the question-mark fallback");
}

void test_utf8()
{
  dashboard::mono_image image;
  dashboard::centered_text(image, "Å", 0, 1);
  // A single five-pixel glyph starts at x=397. Its ring's top pixel is x=399.
  for (int x = 0; x < image.width; ++x)
  {
    require(black(image, x, 0) == (x == 399),
            "UTF-8 centering must count characters instead of bytes");
  }

  dashboard::mono_image expected;
  dashboard::mono_image actual;
  dashboard::centered_text(expected, "?", 0, 1);
  dashboard::centered_text(actual, "😀", 0, 1);
  require(actual.pixels == expected.pixels,
          "A four-byte Unicode character should produce one fallback");

  dashboard::mono_image malformed;
  dashboard::mono_image two_fallbacks;
  dashboard::centered_text(malformed, "\xc0\xaf", 0, 1);
  dashboard::centered_text(two_fallbacks, "??", 0, 1);
  require(malformed.pixels == two_fallbacks.pixels,
          "Malformed UTF-8 must not decode an overlong slash");

  dashboard::mono_image truncated;
  dashboard::centered_text(truncated, "\xc3", 0, 1);
  require(truncated.pixels == expected.pixels,
          "Truncated UTF-8 must use a fallback without reading past the input");
}

} // namespace

int main()
{
  try
  {
    test_glyphs();
    test_utf8();
    return 0;
  }
  catch (const exception &error)
  {
    println(stderr, "{}", error.what());
    return 1;
  }
}
