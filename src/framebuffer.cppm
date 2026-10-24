module;

#include <array>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

export module dashboard.framebuffer;

using namespace std;

export namespace dashboard
{

// Panel format: one bit per pixel, most significant bit first; white = 1.
struct mono_image
{
  static constexpr int width = 800;
  static constexpr int height = 480;
  vector<uint8_t> pixels = vector<uint8_t>(width * height / 8, 0xff);

  void black_pixel(int x, int y)
  {
    if (x >= 0 && x < width && y >= 0 && y < height)
    {
      pixels[y * (width / 8) + x / 8] &= ~(0x80u >> (x % 8));
    }
  }

  void rectangle(int x, int y, int w, int h)
  {
    for (int row = y; row < y + h; ++row)
    {
      for (int col = x; col < x + w; ++col)
      {
        black_pixel(col, row);
      }
    }
  }

  void write_pbm(const string &path) const
  {
    ofstream output(path, ios::binary);
    output << "P4\n" << width << ' ' << height << '\n';
    // PBM uses the opposite polarity: black = 1.
    for (auto byte : pixels)
    {
      output.put(static_cast<char>(byte ^ 0xff));
    }
    output.flush();
    if (!output)
    {
      throw runtime_error("Cannot write preview: " + path);
    }
  }
};

} // namespace dashboard

export namespace dashboard
{

// Five-bit rows for printable ASCII, Swedish letters, and dashboard symbols.
// Unsupported Unicode characters and control characters use '?'.
constexpr array<unsigned char, 7> glyph(char32_t letter)
{
  switch (letter)
  {
  case U' ':
    return {0, 0, 0, 0, 0, 0, 0};
  case U'!':
    return {4, 4, 4, 4, 4, 0, 4};
  case U'"':
    return {10, 10, 10, 0, 0, 0, 0};
  case U'#':
    return {10, 10, 31, 10, 31, 10, 10};
  case U'$':
    return {4, 15, 20, 14, 5, 30, 4};
  case U'%':
    return {24, 25, 2, 4, 8, 19, 3};
  case U'&':
    return {12, 18, 20, 8, 21, 18, 13};
  case U'\'':
    return {4, 4, 8, 0, 0, 0, 0};
  case U'(':
    return {2, 4, 8, 8, 8, 4, 2};
  case U')':
    return {8, 4, 2, 2, 2, 4, 8};
  case U'*':
    return {0, 21, 14, 31, 14, 21, 0};
  case U'+':
    return {0, 4, 4, 31, 4, 4, 0};
  case U',':
    return {0, 0, 0, 0, 4, 4, 8};
  case U'-':
    return {0, 0, 0, 31, 0, 0, 0};
  case U'.':
    return {0, 0, 0, 0, 0, 0, 4};
  case U'/':
    return {1, 1, 2, 4, 8, 16, 16};
  case U'0':
    return {14, 17, 19, 21, 25, 17, 14};
  case U'1':
    return {4, 12, 4, 4, 4, 4, 14};
  case U'2':
    return {14, 17, 1, 2, 4, 8, 31};
  case U'3':
    return {30, 1, 1, 14, 1, 1, 30};
  case U'4':
    return {2, 6, 10, 18, 31, 2, 2};
  case U'5':
    return {31, 16, 16, 30, 1, 1, 30};
  case U'6':
    return {14, 16, 16, 30, 17, 17, 14};
  case U'7':
    return {31, 1, 2, 4, 8, 8, 8};
  case U'8':
    return {14, 17, 17, 14, 17, 17, 14};
  case U'9':
    return {14, 17, 17, 15, 1, 1, 14};
  case U':':
    return {0, 4, 4, 0, 4, 4, 0};
  case U';':
    return {0, 4, 4, 0, 4, 4, 8};
  case U'<':
    return {1, 2, 4, 8, 4, 2, 1};
  case U'=':
    return {0, 0, 31, 0, 31, 0, 0};
  case U'>':
    return {16, 8, 4, 2, 4, 8, 16};
  case U'?':
    return {14, 17, 1, 2, 4, 0, 4};
  case U'@':
    return {14, 17, 23, 21, 23, 16, 15};
  case U'A':
    return {14, 17, 17, 31, 17, 17, 17};
  case U'B':
    return {30, 17, 17, 30, 17, 17, 30};
  case U'C':
    return {14, 17, 16, 16, 16, 17, 14};
  case U'D':
    return {30, 17, 17, 17, 17, 17, 30};
  case U'E':
    return {31, 16, 16, 30, 16, 16, 31};
  case U'F':
    return {31, 16, 16, 30, 16, 16, 16};
  case U'G':
    return {14, 17, 16, 23, 17, 17, 15};
  case U'H':
    return {17, 17, 17, 31, 17, 17, 17};
  case U'I':
    return {31, 4, 4, 4, 4, 4, 31};
  case U'J':
    return {7, 2, 2, 2, 2, 18, 12};
  case U'K':
    return {17, 18, 20, 24, 20, 18, 17};
  case U'L':
    return {16, 16, 16, 16, 16, 16, 31};
  case U'M':
    return {17, 27, 21, 21, 17, 17, 17};
  case U'N':
    return {17, 25, 25, 21, 19, 19, 17};
  case U'O':
    return {14, 17, 17, 17, 17, 17, 14};
  case U'P':
    return {30, 17, 17, 30, 16, 16, 16};
  case U'Q':
    return {14, 17, 17, 17, 21, 18, 13};
  case U'R':
    return {30, 17, 17, 30, 20, 18, 17};
  case U'S':
    return {15, 16, 16, 14, 1, 1, 30};
  case U'T':
    return {31, 4, 4, 4, 4, 4, 4};
  case U'U':
    return {17, 17, 17, 17, 17, 17, 14};
  case U'V':
    return {17, 17, 17, 17, 17, 10, 4};
  case U'W':
    return {17, 17, 17, 21, 21, 21, 10};
  case U'X':
    return {17, 17, 10, 4, 10, 17, 17};
  case U'Y':
    return {17, 17, 10, 4, 4, 4, 4};
  case U'Z':
    return {31, 1, 2, 4, 8, 16, 31};
  case U'[':
    return {14, 8, 8, 8, 8, 8, 14};
  case U'\\':
    return {16, 16, 8, 4, 2, 1, 1};
  case U']':
    return {14, 2, 2, 2, 2, 2, 14};
  case U'^':
    return {4, 10, 17, 0, 0, 0, 0};
  case U'_':
    return {0, 0, 0, 0, 0, 0, 31};
  case U'`':
    return {8, 4, 2, 0, 0, 0, 0};
  case U'a':
    return {0, 0, 14, 1, 15, 17, 15};
  case U'b':
    return {16, 16, 30, 17, 17, 17, 30};
  case U'c':
    return {0, 0, 14, 17, 16, 17, 14};
  case U'd':
    return {1, 1, 15, 17, 17, 17, 15};
  case U'e':
    return {0, 0, 14, 17, 31, 16, 14};
  case U'f':
    return {6, 9, 8, 28, 8, 8, 8};
  case U'g':
    return {0, 15, 17, 17, 15, 1, 14};
  case U'h':
    return {16, 16, 30, 17, 17, 17, 17};
  case U'i':
    return {4, 0, 12, 4, 4, 4, 14};
  case U'j':
    return {2, 0, 6, 2, 2, 18, 12};
  case U'k':
    return {16, 16, 18, 20, 24, 20, 18};
  case U'l':
    return {12, 4, 4, 4, 4, 4, 14};
  case U'm':
    return {0, 0, 26, 21, 21, 21, 21};
  case U'n':
    return {0, 0, 30, 17, 17, 17, 17};
  case U'o':
    return {0, 0, 14, 17, 17, 17, 14};
  case U'p':
    return {0, 30, 17, 17, 30, 16, 16};
  case U'q':
    return {0, 15, 17, 17, 15, 1, 1};
  case U'r':
    return {0, 0, 22, 25, 16, 16, 16};
  case U's':
    return {0, 0, 15, 16, 14, 1, 30};
  case U't':
    return {8, 8, 28, 8, 8, 9, 6};
  case U'u':
    return {0, 0, 17, 17, 17, 19, 13};
  case U'v':
    return {0, 0, 17, 17, 17, 10, 4};
  case U'w':
    return {0, 0, 17, 17, 21, 21, 10};
  case U'x':
    return {0, 0, 17, 10, 4, 10, 17};
  case U'y':
    return {0, 17, 17, 17, 15, 1, 14};
  case U'z':
    return {0, 0, 31, 2, 4, 8, 31};
  case U'{':
    return {2, 4, 4, 8, 4, 4, 2};
  case U'|':
    return {4, 4, 4, 4, 4, 4, 4};
  case U'}':
    return {8, 4, 4, 2, 4, 4, 8};
  case U'~':
    return {0, 0, 9, 22, 0, 0, 0};
  case U'\u00a0':
    return {0, 0, 0, 0, 0, 0, 0};
  case U'°':
    return {6, 9, 9, 6, 0, 0, 0};
  case U'Ä':
    return {10, 0, 14, 17, 31, 17, 17};
  case U'Å':
    return {4, 10, 14, 17, 31, 17, 17};
  case U'É':
    return {2, 4, 31, 16, 30, 16, 31};
  case U'Ö':
    return {10, 0, 14, 17, 17, 17, 14};
  case U'ä':
    return {10, 0, 14, 1, 15, 17, 15};
  case U'å':
    return {4, 10, 14, 1, 15, 17, 15};
  case U'é':
    return {2, 4, 14, 17, 31, 16, 14};
  case U'ö':
    return {10, 0, 14, 17, 17, 17, 14};
  case U'–':
    return {0, 0, 0, 31, 0, 0, 0};
  case U'—':
    return {0, 0, 0, 31, 0, 0, 0};
  case U'‘':
    return {2, 4, 4, 0, 0, 0, 0};
  case U'’':
    return {4, 4, 8, 0, 0, 0, 0};
  case U'“':
    return {5, 10, 10, 0, 0, 0, 0};
  case U'”':
    return {10, 10, 20, 0, 0, 0, 0};
  case U'•':
    return {0, 0, 4, 14, 4, 0, 0};
  case U'…':
    return {0, 0, 0, 0, 0, 0, 21};
  case U'€':
    return {7, 8, 30, 8, 30, 8, 7};
  default:
    return {14, 17, 1, 2, 4, 0, 4};
  }
}

} // namespace dashboard

namespace
{

u32string decode_utf8(string_view text)
{
  u32string result;
  for (size_t i = 0; i < text.size();)
  {
    const auto lead = static_cast<unsigned char>(text[i]);
    if (lead < 0x80)
    {
      result.push_back(lead);
      ++i;
      continue;
    }

    const size_t count = lead >= 0xc2 && lead <= 0xdf   ? 2
                         : lead >= 0xe0 && lead <= 0xef ? 3
                         : lead >= 0xf0 && lead <= 0xf4 ? 4
                                                        : 0;
    char32_t value = count ? lead & (0x7f >> count) : 0;
    bool valid = count && count <= text.size() - i;
    for (size_t j = 1; valid && j < count; ++j)
    {
      const auto next = static_cast<unsigned char>(text[i + j]);
      valid = (next & 0xc0) == 0x80;
      value = (value << 6) | (next & 0x3f);
    }
    const char32_t minimum = count == 2 ? 0x80 : count == 3 ? 0x800 : 0x10000;
    valid = valid && value >= minimum && value <= 0x10ffff &&
            !(value >= 0xd800 && value <= 0xdfff);
    result.push_back(valid ? value : U'\ufffd');
    i += valid ? count : 1;
  }
  return result;
}

} // namespace

export namespace dashboard
{

void centered_text(dashboard::mono_image &image, string_view text, int y,
                   int scale)
{
  const auto letters = decode_utf8(text);
  int x =
      (image.width - (static_cast<int>(letters.size()) * 6 - 1) * scale) / 2;
  for (char32_t letter : letters)
  {
    const auto rows = glyph(letter);
    for (int row = 0; row < 7; ++row)
    {
      for (int col = 0; col < 5; ++col)
      {
        if (rows[row] & (1u << (4 - col)))
        {
          image.rectangle(x + col * scale, y + row * scale, scale, scale);
        }
      }
    }
    x += 6 * scale;
  }
}

mono_image test_image()
{
  mono_image image;
  image.rectangle(16, 16, 768, 3);
  image.rectangle(16, 461, 768, 3);
  image.rectangle(16, 16, 3, 448);
  image.rectangle(781, 16, 3, 448);
  centered_text(image, "FAMILY DASHBOARD", 70, 4);
  image.rectangle(64, 146, 672, 2);
  centered_text(image, "HELLO KENT", 218, 8);
  for (int i = 0; i < 16; ++i)
  {
    if (i % 2 == 0)
    {
      image.rectangle(240 + 20 * i, 368, 20, 20);
    }
  }
  return image;
}

} // namespace dashboard
