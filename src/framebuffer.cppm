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

namespace
{

// Small hand-drawn 5x7 alphabet for the hardware smoke test.
array<unsigned char, 7> glyph(char letter)
{
  switch (letter)
  {
  case 'A':
    return {14, 17, 17, 31, 17, 17, 17};
  case 'B':
    return {30, 17, 17, 30, 17, 17, 30};
  case 'D':
    return {30, 17, 17, 17, 17, 17, 30};
  case 'E':
    return {31, 16, 16, 30, 16, 16, 31};
  case 'F':
    return {31, 16, 16, 30, 16, 16, 16};
  case 'H':
    return {17, 17, 17, 31, 17, 17, 17};
  case 'I':
    return {31, 4, 4, 4, 4, 4, 31};
  case 'K':
    return {17, 18, 20, 24, 20, 18, 17};
  case 'L':
    return {16, 16, 16, 16, 16, 16, 31};
  case 'M':
    return {17, 27, 21, 21, 17, 17, 17};
  case 'N':
    return {17, 25, 25, 21, 19, 19, 17};
  case 'O':
    return {14, 17, 17, 17, 17, 17, 14};
  case 'R':
    return {30, 17, 17, 30, 20, 18, 17};
  case 'S':
    return {15, 16, 16, 14, 1, 1, 30};
  case 'T':
    return {31, 4, 4, 4, 4, 4, 4};
  case 'W':
    return {17, 17, 17, 21, 21, 21, 10};
  case 'Y':
    return {17, 17, 10, 4, 4, 4, 4};
  case ' ':
    return {};
  default:
    throw invalid_argument("Unsupported test-font character");
  }
}

void centered_text(dashboard::mono_image &image, string_view text, int y,
                   int scale)
{
  int x = (image.width - (static_cast<int>(text.size()) * 6 - 1) * scale) / 2;
  for (char letter : text)
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

} // namespace

export namespace dashboard
{

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
