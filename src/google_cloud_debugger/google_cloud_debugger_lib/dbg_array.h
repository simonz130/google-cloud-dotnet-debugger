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

#ifndef DBG_ARRAY_H_
#define DBG_ARRAY_H_

#include <memory>
#include <vector>

#include "dbg_reference_object.h"

namespace google_cloud_debugger {

class EvalCoordinator;

// This class represents a .NET array object.
// This includes multi-dimensional as well as jagged arrays.
class DbgArray : public DbgReferenceObject {
 public:
  // This constructor is used for testing.
  DbgArray(std::vector<ULONG32> dimensions)
      : DbgReferenceObject(nullptr, 0, std::shared_ptr<ICorDebugHelper>(),
                           std::shared_ptr<IDbgObjectFactory>()) {
    dimensions_ = dimensions;
    cor_element_type_ = CorElementType::ELEMENT_TYPE_ARRAY;

  }
   
  DbgArray(ICorDebugType *debug_type, int depth,
           std::shared_ptr<ICorDebugHelper> debug_helper,
           std::shared_ptr<IDbgObjectFactory> object_factory)
      : DbgReferenceObject(debug_type, depth, debug_helper,
                           object_factory) {}

  // Retrieves information about array rank, array dimensions, array type and
  // creates a strong handle to the array.
  void Initialize(ICorDebugValue *debug_value, BOOL is_null) override;

  // Gets the object at a given position in the array.
  // Note that we treat multi-dimensional array as zero-based,
  // single-dimensional array. The layout of this array follows C++ style of
  // array layout.
  // In the case of an array of array, this simply gets the
  // inner array at position i.
  //
  // For example, if the array is an array of array like
  // double[][] jagged = new double[10][10],
  // then GetArrayItem(1, array_item) will return the inner item
  // jagged[1].
  // If the array is a jagged array like
  // double[,] multi = new double[10, 10],
  // then GetArrayItem(1, array_item) will return multi[0, 1]
  // and GetArrayItem(10, array_item) will return multi[1, 0].
  HRESULT GetArrayItem(int position, ICorDebugValue **array_item);

  // Populate members vector with items in the array.
  // Variable_proto will be used to create protos that represent
  // items in the array. These protos, together with the DbgObject
  // (which represents the underlying objects in the array) will
  // be used to populate the members vector.
  HRESULT PopulateMembers(
      google::cloud::diagnostics::debug::Variable *variable_proto,
      std::vector<VariableWrapper> *members,
      IEvalCoordinator *eval_coordinator) override;

  // Gets the type of the array.
  // For example, if this array represents an int array,
  // type_string will be set to int[].
  HRESULT GetTypeString(std::string *type_string) override;

  // Sets the maximum amount of items that the array will retrieve
  // when PopulateMembers is called.
  void SetMaxArrayItemsToRetrieve(std::uint32_t target) {
    max_items_to_retrieved_ = target;
  }

  // Returns the size of the array.
  // This is calculated as the product of all the array dimensions.
  // For example:
  //   1D array of dimensions 3 will have a size of 3.
  //   2D array of 3x3 would have a size of 9.
  //   3D array of 3x3x3 would have a size of 27.
  int GetArraySize();

  // Returns TypeSignature of this array.
  HRESULT GetTypeSignature(TypeSignature *type_signature) override;

 private:
  // The type of the array.
  CComPtr<ICorDebugType> array_type_;

  // This empty_object_ is an object of the type array_type_.
  // We use this object to help us determine the array type.
  std::unique_ptr<DbgObject> empty_object_;

  // An array that stores the dimensions of the array.
  // Each value in this array specifies the number of elements in
  // a dimension in this array.
  std::vector<ULONG32> dimensions_;

  // The maximum amount of items to retrieve in an array.
  std::uint32_t max_items_to_retrieved_ = 0;
};

}  //  namespace google_cloud_debugger

#endif  //  DBG_ARRAY_H_
