// Copyright 2017 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef CUSTOM_BINARY_READER_H_
#define CUSTOM_BINARY_READER_H_

#include <cstdint>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "cor.h"

#include "metadata_tables.h"

// typedef std::vector<uint8_t>::const_iterator binary_stream_iter;

namespace google_cloud_debugger_portable_pdb {
struct CompressedMetadataTableHeader;

enum Heap : std::uint8_t {
  StringsHeap = 0x01,
  GuidsHeap = 0x02,
  BlobsHeap = 0x04
};

// Class that consumes a file or a uint8_t vector and produces a
// binary stream. This stream is used to read byte, integers,
// compressed integers and table index.
class CustomBinaryStream {
 public:
  // Consumes a binary stream pointer, takes ownership
  // of the underlying stream and makes it the underlying
  // stream of this class.
  bool ConsumeStream(std::istream *stream);

  // Consumes a file and exposes the file content as a binary stream.
  bool ConsumeFile(const std::string &file);

  // Returns true if there is a next byte in the stream.
  bool HasNext() const;

  // Returns the next byte without advancing the stream.
  bool Peek(std::uint8_t *result) const;

  // Sets the stream position to position from the current position.
  bool SeekFromCurrent(std::uint32_t position);

  // Sets the stream position to position from the original position.
  // This function ignores the length of the stream set by SetStreamLength.
  bool SeekFromOrigin(std::uint32_t position);

  // Sets where the stream will end. This should be less than the current end_.
  bool SetStreamLength(std::uint32_t length);

  // Resets the stream length to the original length of the file.
  // This function is meant to be used to reset the stream after
  // SetStreamLength has been used.
  void ResetStreamLength();

  // Gets a string starting from the offset to a null terminating character or the end of the stream.
  // This function does not change the stream pointer.
  bool GetString(std::string *result, std::uint32_t offset);

  // Gets blob bytes starting from offset in the stream.
  // The first byte will tell us the length of the blob.
  // This function does not change the stream pointer.
  bool GetBlobBytes(std::uint32_t offset, std::vector<uint8_t> *result);

  // Reads the next byte in the stream. Returns false if the byte
  // cannot be read.
  bool ReadByte(std::uint8_t *result);

  // Tries to read bytes_to_read in the stream and stores result in result.
  // Returns false if the number of bytes read are less than that.
  bool ReadBytes(std::uint8_t *result, std::uint32_t bytes_to_read,
                 std::uint32_t *bytes_read);

  // Reads the next UInt16 from the stream. Returns false if the UInt16
  // cannot be read.
  bool ReadUInt16(std::uint16_t *result);

  // Reads the next UInt32 from the stream. Returns false if the UInt32
  // cannot be read.
  bool ReadUInt32(std::uint32_t *result);

  // Reads an unsigned integer using the encoding mechanism described in the
  // ECMA spec. See II.23.2 "Blobs and signatures". Returns false if the
  // Compressed UInt32 cannot be read.
  bool ReadCompressedUInt32(std::uint32_t *result);

  // Reads a signed int using the encoding mechanism described in the ECMA spec.
  // See II.23.2 "Blobs and signatures".
  //
  //
  // Why the long SignedInt name, rather than "CompressedUInt" and
  // "CompressedInt"? Because 90% of all reads from compressed values are for
  // UInts and it is too easy to confuse the two. Only call this method if you
  // are positive you are reading a signed value.
  bool ReadCompressSignedInt32(std::int32_t *result);

  // Reads a heap table index according to II.24.2.6 "#~ stream" under schema.
  // Returns false if it cannot be read.
  bool ReadTableIndex(Heap heap, std::uint8_t heap_size,
                      std::uint32_t *table_index);

  // Reads the size of a metadata table index according to II.24.2.6 "#~ stream"
  // under schema. Returns false if it cannot be read.
  bool ReadTableIndex(MetadataTable table,
                      const CompressedMetadataTableHeader &metadata_header,
                      std::uint32_t *table_index);

  // Returns the current position of the stream.
  std::streampos Current() { return stream_->tellg(); }

 private:
  // The underlying binary stream.
  std::unique_ptr<std::istream> stream_;

  // The begin position of the stream.
  std::streampos begin_;

  // The absolute end position of the stream.
  std::streampos absolute_end_;

  // The relative end position of the stream (sets by SetStreamLength), which
  // is as far in a PDB file as we need to read.
  std::streampos relative_end_;
};

}  // namespace google_cloud_debugger_portable_pdb

#endif
