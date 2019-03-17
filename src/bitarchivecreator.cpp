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

#include "../include/bitarchivecreator.hpp"

#include "../include/bitexception.hpp"
#include "../include/bitpropvariant.hpp"

#include <vector>

#include "7zip/Archive/IArchive.h"
#include "Common/MyCom.h"

using std::wstring;
using std::vector;
using namespace bit7z;

CMyComPtr< IOutArchive > initArchiveObject( const Bit7zLibrary& lib, const GUID* format_GUID ) {
    CMyComPtr< IOutArchive > arc_object;
    lib.createArchiveObject( format_GUID, &::IID_IOutArchive, reinterpret_cast< void** >( &arc_object ) );
    return arc_object;
}

bool isValidCompressionMethod( const BitInFormat& format, BitCompressionMethod method ) {
    switch ( method ) {
        case BitCompressionMethod::Copy:
            return format == BitFormat::SevenZip || format == BitFormat::Zip || format == BitFormat::Tar ||
                   format == BitFormat::Wim;
        case BitCompressionMethod::Ppmd:
        case BitCompressionMethod::Lzma:
            return format == BitFormat::SevenZip || format == BitFormat::Zip;
        case BitCompressionMethod::Lzma2:
            return format == BitFormat::SevenZip || format == BitFormat::Xz;
        case BitCompressionMethod::BZip2:
            return format == BitFormat::SevenZip || format == BitFormat::BZip2 || format == BitFormat::Zip;
        case BitCompressionMethod::Deflate:
            return format == BitFormat::GZip || format == BitFormat::Zip;
        case BitCompressionMethod::Deflate64:
            return format == BitFormat::Zip;
        default:
            return true;
    }
}

bool isValidDictionarySize( BitCompressionMethod method, uint32_t dictionary_size ) {
    switch ( method ) {
        case BitCompressionMethod::Lzma:
        case BitCompressionMethod::Lzma2:
            return dictionary_size <= 1536 * ( 1 << 20 ); // less than 1536 MiB
        case BitCompressionMethod::Ppmd:
            return dictionary_size <= ( 1 << 30 );        // less than 1 GiB, i.e. 2^30 bytes
        case BitCompressionMethod::BZip2:
            return dictionary_size <= 900 * ( 1 << 10 );  // less than 900 KiB
        case BitCompressionMethod::Deflate64:
            return dictionary_size == ( 1 << 16 );        // equal to 64 KiB, i.e. 2^16 bytes
        case BitCompressionMethod::Deflate:
            return dictionary_size == ( 1 << 15 );        // equal to 32 KiB, i.e. 2^15 bytes
        default:
            return true; //copy
    }
}

const wchar_t* methodName( BitCompressionMethod method ) {
    switch ( method ) {
        case BitCompressionMethod::Copy:
            return L"Copy";
        case BitCompressionMethod::Ppmd:
            return L"PPMd";
        case BitCompressionMethod::Lzma:
            return L"LZMA";
        case BitCompressionMethod::Lzma2:
            return L"LZMA2";
        case BitCompressionMethod::BZip2:
            return L"BZip2";
        case BitCompressionMethod::Deflate:
            return L"Deflate";
        case BitCompressionMethod::Deflate64:
            return L"Deflate64";
        default:
            return L""; //this should not happen!
    }
}

BitArchiveCreator::BitArchiveCreator( const Bit7zLibrary& lib, const BitInOutFormat& format ) :
    BitArchiveHandler( lib ),
    mFormat( format ),
    mCompressionLevel( BitCompressionLevel::NORMAL ),
    mCompressionMethod( format.defaultMethod() ),
    mDictionarySize( 0 ),
    mCryptHeaders( false ),
    mSolidMode( false ),
    mUpdateMode( false ),
    mVolumeSize( 0 ) {}


BitArchiveCreator::~BitArchiveCreator() {}

const BitInFormat& BitArchiveCreator::format() const {
    return mFormat;
}

const BitInOutFormat& BitArchiveCreator::compressionFormat() const {
    return mFormat;
}

bool BitArchiveCreator::cryptHeaders() const {
    return mCryptHeaders;
}

BitCompressionLevel BitArchiveCreator::compressionLevel() const {
    return mCompressionLevel;
}

BitCompressionMethod BitArchiveCreator::compressionMethod() const {
    return mCompressionMethod;
}

uint32_t BitArchiveCreator::dictionarySize() const {
    return mDictionarySize;
}

bool BitArchiveCreator::solidMode() const {
    return mSolidMode;
}

bool BitArchiveCreator::updateMode() const {
    return mUpdateMode;
}

uint64_t BitArchiveCreator::volumeSize() const {
    return mVolumeSize;
}

void BitArchiveCreator::setPassword( const wstring& password ) {
    setPassword( password, mCryptHeaders );
}

void BitArchiveCreator::setPassword( const wstring& password, bool crypt_headers ) {
    mPassword = password;
    mCryptHeaders = ( password.length() > 0 ) && crypt_headers;
}

void BitArchiveCreator::setCompressionLevel( BitCompressionLevel compression_level ) {
    mCompressionLevel = compression_level;
}

void BitArchiveCreator::setCompressionMethod( BitCompressionMethod compression_method ) {
    if ( !isValidCompressionMethod( mFormat, compression_method ) ) {
        throw BitException( "Invalid compression method for the chosen archive format" );
    }
    if ( mFormat.hasFeature( MULTIPLE_METHODS ) ) {
        /* even though the compression method is valid, we set it only if the format supports
         * different methods than the default one */
        mCompressionMethod = compression_method;
        mDictionarySize = 0; //reset dictionary size to default for the method
    }
}

void BitArchiveCreator::setDictionarySize( uint32_t dictionary_size ) {
    if ( !isValidDictionarySize( mCompressionMethod, dictionary_size ) ) {
        throw BitException( "Invalid dictionary size for the chosen compression method" );
    }
    if ( mCompressionMethod != BitCompressionMethod::Copy ||
            mCompressionMethod != BitCompressionMethod::Deflate ||
            mCompressionMethod != BitCompressionMethod::Deflate64 ) {
        //ignoring setting dictionary size for copy method and for methods having fixed dictionary size (deflate family)
        mDictionarySize = dictionary_size;
    }
}

void BitArchiveCreator::setSolidMode( bool solid_mode ) {
    mSolidMode = solid_mode;
}

void BitArchiveCreator::setUpdateMode( bool update_mode ) {
    mUpdateMode = update_mode;
}

void BitArchiveCreator::setVolumeSize( uint64_t size ) {
    mVolumeSize = size;
}

CMyComPtr<IOutArchive> BitArchiveCreator::initOutArchive() const {
    const GUID format_GUID = mFormat.guid();

    CMyComPtr< IOutArchive > out_archive = initArchiveObject( mLibrary, &format_GUID );
    setArchiveProperties( out_archive );
    return out_archive;
}

void BitArchiveCreator::setArchiveProperties( IOutArchive* out_archive ) const {
    vector< const wchar_t* > names;
    vector< BitPropVariant > values;
    if ( mCryptHeaders && mFormat.hasFeature( HEADER_ENCRYPTION ) ) {
        names.push_back( L"he" );
        values.emplace_back( true );
    }
    if ( mFormat.hasFeature( COMPRESSION_LEVEL ) ) {
        names.push_back( L"x" );
        values.emplace_back( static_cast< uint32_t >( mCompressionLevel ) );

        if ( mFormat.hasFeature( MULTIPLE_METHODS ) && mCompressionMethod != mFormat.defaultMethod() ) {
            names.push_back( mFormat == BitFormat::SevenZip ? L"0" : L"m" );
            values.emplace_back( methodName( mCompressionMethod ) );
        }
    }
    if ( mFormat.hasFeature( SOLID_ARCHIVE ) ) {
        names.push_back( L"s" );
        values.emplace_back( mSolidMode );
    }
    if ( mDictionarySize != 0 ) {
        const wchar_t* prop_name;
        //cannot optimize the following if-else, if we use wstring we have invalid pointers in names!
        if ( mFormat == BitFormat::SevenZip ) {
            prop_name = ( mCompressionMethod == BitCompressionMethod::Ppmd ? L"0mem" : L"0d" );
        } else {
            prop_name = ( mCompressionMethod == BitCompressionMethod::Ppmd ? L"mem" : L"d" );
        }
        names.push_back( prop_name );
        values.emplace_back( std::to_wstring( mDictionarySize ) + L"b" );
    }

    if ( !names.empty() ) {
        CMyComPtr< ISetProperties > set_properties;
        if ( out_archive->QueryInterface( ::IID_ISetProperties,
                                          reinterpret_cast< void** >( &set_properties ) ) != S_OK ) {
            throw BitException( "ISetProperties unsupported" );
        }
        if ( set_properties->SetProperties( names.data(), values.data(),
                                            static_cast< uint32_t >( names.size() ) ) != S_OK ) {
            throw BitException( "Cannot set properties of the archive" );
        }
    }
}
