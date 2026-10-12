/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/ClipboardImageCodec.h"

#include "base/Log.h"

#include <QBuffer>
#include <QByteArray>
#include <QImage>
#include <QtEndian>

#include <cstring>

namespace {

constexpr quint32 kBmpFileHeaderSize = 14;
constexpr quint32 kStandardDibHeaderSize = 40; // sizeof(BITMAPINFOHEADER)
constexpr quint32 kBiBitfields = 3;
constexpr quint32 kBiAlphabitfields = 6;
constexpr unsigned char kPngSignature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};

// The clipboard's generic Bitmap payload is a DIB (BITMAPINFOHEADER-or-later
// immediately followed by pixel data, no file header). Newer DIB header
// variants (BITMAPV4/V5) fold colour-mask/profile fields into the header
// itself, while a plain BITMAPINFOHEADER using BI_BITFIELDS appends them
// after it instead, so the pixel offset has to be computed from the header
// rather than assumed to be a fixed size.
quint32 dibPixelOffset(const quint8 *dib, qsizetype dibSize)
{
  quint32 pixelOffset = 0;
  if (dibSize >= 16) {
    const auto biSize = qFromLittleEndian<quint32>(dib);
    if (biSize >= 12 && biSize <= dibSize) {
      pixelOffset = biSize;
      if (biSize >= kStandardDibHeaderSize) {
        const auto biBitCount = qFromLittleEndian<quint16>(dib + 14);
        const auto biCompression = qFromLittleEndian<quint32>(dib + 16);
        if (biSize == kStandardDibHeaderSize) {
          if (biCompression == kBiBitfields) {
            pixelOffset += 12;
          } else if (biCompression == kBiAlphabitfields) {
            pixelOffset += 16;
          }
        }
        if (biBitCount > 0 && biBitCount <= 8) {
          const auto biClrUsed = qFromLittleEndian<quint32>(dib + 32);
          const auto numColors = biClrUsed != 0 ? biClrUsed : (1u << biBitCount);
          pixelOffset += numColors * 4;
        }
      }
    }
  }
  return pixelOffset;
}

// Wrap a generic-cache DIB with a standard 14 byte BITMAPFILEHEADER so Qt
// can decode it as a standalone BMP file.
QByteArray wrapDibAsBmp(const std::string &dib)
{
  if (dib.size() < 4) {
    return {};
  }

  auto pixelOffset = dibPixelOffset(reinterpret_cast<const quint8 *>(dib.data()), static_cast<qsizetype>(dib.size()));
  if (pixelOffset == 0) {
    pixelOffset = kStandardDibHeaderSize;
  }

  quint8 header[kBmpFileHeaderSize];
  header[0] = 'B';
  header[1] = 'M';
  qToLittleEndian<quint32>(static_cast<quint32>(kBmpFileHeaderSize + dib.size()), header + 2);
  qToLittleEndian<quint16>(0, header + 6);
  qToLittleEndian<quint16>(0, header + 8);
  qToLittleEndian<quint32>(kBmpFileHeaderSize + pixelOffset, header + 10);

  QByteArray bmp(reinterpret_cast<const char *>(header), static_cast<qsizetype>(kBmpFileHeaderSize));
  bmp += QByteArray(dib.data(), static_cast<qsizetype>(dib.size()));
  return bmp;
}

// Strip a standalone BMP file's 14 byte file header, back to the generic
// cache's DIB-only convention.
std::string unwrapBmpToDib(const QByteArray &bmp)
{
  if (bmp.size() <= static_cast<qsizetype>(kBmpFileHeaderSize)) {
    return {};
  }
  return std::string(bmp.constData() + kBmpFileHeaderSize, static_cast<size_t>(bmp.size()) - kBmpFileHeaderSize);
}

} // namespace

namespace deskflow::clipboard_image {

std::string compress(const std::string &dib)
{
  if (dib.empty()) {
    return dib;
  }

  const auto bmp = wrapDibAsBmp(dib);
  QImage image;
  if (bmp.isEmpty() || !image.loadFromData(bmp, "BMP")) {
    LOG_DEBUG("clipboard image: failed to decode dib, sending uncompressed (%zu bytes)", dib.size());
    return dib;
  }

  QByteArray png;
  QBuffer buffer(&png);
  buffer.open(QIODevice::WriteOnly);
  if (!image.save(&buffer, "PNG")) {
    LOG_DEBUG("clipboard image: failed to encode png, sending uncompressed (%zu bytes)", dib.size());
    return dib;
  }

  LOG_DEBUG("clipboard image: compressed %zu byte dib to %lld byte png", dib.size(), static_cast<long long>(png.size()));
  return std::string(png.constData(), static_cast<size_t>(png.size()));
}

std::string decompress(const std::string &data)
{
  if (data.size() < sizeof(kPngSignature) ||
      std::memcmp(data.data(), kPngSignature, sizeof(kPngSignature)) != 0) {
    // not PNG -- assume it's already a raw DIB, e.g. from a peer that
    // doesn't compress this payload.
    return data;
  }

  QImage image;
  if (!image.loadFromData(QByteArray::fromRawData(data.data(), static_cast<qsizetype>(data.size())), "PNG")) {
    LOG_WARN("clipboard image: failed to decode png clipboard data");
    return data;
  }

  QByteArray bmp;
  QBuffer buffer(&bmp);
  buffer.open(QIODevice::WriteOnly);
  if (!image.save(&buffer, "BMP")) {
    LOG_WARN("clipboard image: failed to re-encode clipboard image as bmp");
    return data;
  }

  auto dib = unwrapBmpToDib(bmp);
  if (dib.empty()) {
    return data;
  }
  return dib;
}

} // namespace deskflow::clipboard_image
