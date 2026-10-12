/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <string>

namespace deskflow::clipboard_image {

//! Compress a kBitmap payload (a DIB: BITMAPINFOHEADER-or-later followed by
//! pixel data, with no 14 byte file header) to PNG for the wire. Returns
//! \p dib unchanged if it can't be decoded as an image, so the caller always
//! has something to send.
std::string compress(const std::string &dib);

//! Reverse of compress(): decode PNG back to DIB form. Returns \p data
//! unchanged if it isn't PNG, so a peer that still sends raw DIB bytes
//! continues to work.
std::string decompress(const std::string &data);

} // namespace deskflow::clipboard_image
