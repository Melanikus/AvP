/*
 * libbinkdec - Bink video decoder
 * Copyright (C) 2011 Barry Duncan
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

// Modified 2026-11-05 by Melanikus for the AvP port:
//   BK-P23: errors aren't kept in a string that several threads wrote to and nothing read
#include "LogError.h"

namespace BinkCommon {

// BK-P23: this used to keep the message in one string, which every film's thread wrote to with nothing to stop two doing it at once, and which nothing ever read. It says nothing now, and it's
// kept so that the decoder can go on saying what went wrong in one place, for anyone who wants to look.
void LogError(const std::string &)
{
}

} // close namespace BinkCommon
