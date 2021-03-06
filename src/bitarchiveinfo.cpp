// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

/*
 * bit7z - A C++ static library to interface with the 7-zip DLLs.
 * Copyright (c) 2014-2018  Riccardo Ostani - All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * Bit7z is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with bit7z; if not, see https://www.gnu.org/licenses/.
 */

#include "../include/bitarchiveinfo.hpp"

#include "7zip/PropID.h"

#include "../include/bitexception.hpp"
#include "../include/util.hpp"

using namespace bit7z;
using namespace bit7z::util;

BitArchiveInfo::BitArchiveInfo( const Bit7zLibrary& lib, const wstring& in_file, const BitInFormat& format )
    : BitArchiveOpener( lib, format ) {
    mInArchive = openArchive( mLibrary, mFormat, in_file, *this ).Detach();
}

BitArchiveInfo::~BitArchiveInfo() {
    if ( mInArchive ) {
        mInArchive->Release();
    }
}

BitPropVariant BitArchiveInfo::getArchiveProperty( BitProperty property ) const {
    BitPropVariant propvar;
    HRESULT res = mInArchive->GetArchiveProperty( static_cast<PROPID>( property ), &propvar );
    if ( res != S_OK ) {
        throw BitException( "Could not retrieve archive property" );
    }
    return propvar;
}

BitPropVariant BitArchiveInfo::getItemProperty( uint32_t index, BitProperty property ) const {
    BitPropVariant propvar;
    HRESULT res = mInArchive->GetProperty( index, static_cast<PROPID>( property ), &propvar );
    if ( res != S_OK ) {
        throw BitException( "Could not retrieve property for item at index " + std::to_string( index ) );
    }
    return propvar;
}

map<BitProperty, BitPropVariant> BitArchiveInfo::archiveProperties() const {
    map<BitProperty, BitPropVariant> result;
    for ( uint32_t i = kpidNoProperty; i <= kpidCopyLink; ++i ) {
        // Yeah, I know, I double cast property (here and in getArchiveProperty), but the code is easier to read!
        auto property = static_cast<BitProperty>( i );
        BitPropVariant property_value = getArchiveProperty( property );
        if ( !property_value.isEmpty() ) {
            result[ property ] = property_value;
        }
    }
    return result;
}

vector<BitArchiveItem> BitArchiveInfo::items() const {
    vector<BitArchiveItem> result;
    for ( uint32_t i = 0; i < itemsCount(); ++i ) {
        BitArchiveItem item( i );
        for ( uint32_t j = kpidNoProperty; j <= kpidCopyLink; ++j ) {
            // Yeah, I know, I double cast property (here and in getItemProperty), but the code is easier to read!
            auto property = static_cast<BitProperty>( j );
            BitPropVariant property_value = getItemProperty( i, property );
            if ( !property_value.isEmpty() ) {
                item.setProperty( property, property_value );
            }
        }
        result.push_back( item );
    }
    return result;
}

uint32_t BitArchiveInfo::itemsCount() const {
    uint32_t items_count;
    HRESULT result = mInArchive->GetNumberOfItems( &items_count );
    if ( result != S_OK ) {
        throw BitException( "Could not retrieve the number of items in the archive" );
    }
    return items_count;
}

uint32_t BitArchiveInfo::foldersCount() const {
    uint32_t result = 0;
    for ( uint32_t i = 0; i < itemsCount(); ++i ) {
        BitPropVariant prop = getItemProperty( i, BitProperty::IsDir );
        if ( !prop.isEmpty() && prop.getBool() ) {
            result += 1;
        }
    }
    return result;
}

uint32_t BitArchiveInfo::filesCount() const {
    return itemsCount() - foldersCount(); //I'm lazy :)
}

uint64_t BitArchiveInfo::size() const {
    uint64_t result = 0;
    for ( uint32_t i = 0; i < itemsCount(); ++i ) {
        BitPropVariant prop = getItemProperty( i, BitProperty::Size );
        if ( !prop.isEmpty() ) {
            result += prop.getUInt64();
        }
    }
    return result;
}

uint64_t BitArchiveInfo::packSize() const {
    uint64_t result = 0;
    for ( uint32_t i = 0; i < itemsCount(); ++i ) {
        BitPropVariant prop = getItemProperty( i, BitProperty::PackSize );
        if ( !prop.isEmpty() ) {
            result += prop.getUInt64();
        }
    }
    return result;
}
