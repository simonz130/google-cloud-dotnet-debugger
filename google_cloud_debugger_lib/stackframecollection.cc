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

#include "dbgbreakpoint.h"
#include "evalcoordinator.h"
#include "stackframecollection.h"

#include <iostream>

using google::cloud::diagnostics::debug::Breakpoint;
using google::cloud::diagnostics::debug::SourceLocation;
using google::cloud::diagnostics::debug::StackFrame;
using google_cloud_debugger_portable_pdb::LocalConstantInfo;
using google_cloud_debugger_portable_pdb::LocalVariableInfo;
using google_cloud_debugger_portable_pdb::SequencePoint;
using std::cerr;
using std::vector;

namespace google_cloud_debugger {

HRESULT StackFrameCollection::Initialize(
    ICorDebugStackWalk *debug_stack_walk,
    const std::vector<google_cloud_debugger_portable_pdb::PortablePdbFile>
        &pdb_files) {
  if (!debug_stack_walk) {
    cerr << "Debug stack walk is null.";
    return E_INVALIDARG;
  }

  CComPtr<ICorDebugFrame> frame;
  CComPtr<ICorDebugILFrame> il_frame;
  HRESULT hr = S_OK;

  // Walks through the stack and populates stack_frames_ vector.
  while (SUCCEEDED(hr)) {
    hr = debug_stack_walk->GetFrame(&frame);
    // No more stacks.
    if (hr == S_FALSE) {
      return S_OK;
    }

    if (FAILED(hr)) {
      cerr << "Failed to get active frame.";
      return hr;
    }

    hr = frame->QueryInterface(__uuidof(ICorDebugILFrame),
                               reinterpret_cast<void **>(&il_frame));
    if (FAILED(hr)) {
      cerr << "Failed to get ILFrame";
      return hr;
    }

    // Gets ICorDebugFunction that corresponds to the function at this frame.
    CComPtr<ICorDebugFunction> frame_function;
    hr = il_frame->GetFunction(&frame_function);
    if (FAILED(hr)) {
      cerr << "Failed to get ICorDebugFunction from IL Frame.";
      return hr;
    }

    // Gets the token of the function above.
    mdMethodDef target_function_token;
    hr = frame_function->GetToken(&target_function_token);
    if (FAILED(hr)) {
      cerr << "Failed to extract token from debug function.";
      return hr;
    }

    // Gets the ICorDebugModule of the module at this frame.
    CComPtr<ICorDebugModule> frame_module;
    hr = frame_function->GetModule(&frame_module);
    if (FAILED(hr)) {
      cerr << "Failed to get ICorDebugModule from ICorDebugFunction.";
      return hr;
    }

    // Gets the token of the module above.
    mdModule target_module_token;
    hr = frame_module->GetToken(&target_module_token);
    if (FAILED(hr)) {
      cerr << "Failed to extract token from ICorDebugModule.";
      return hr;
    }

    DbgStackFrame stack_frame;

    for (auto &&pdb_file : pdb_files) {
      // Extracts out the token that corresponds to the module of the PDB file
      // and only proceeds if it is the same as target_module_token.
      CComPtr<ICorDebugModule> pdb_debug_module;
      // TODO(quoct): Possible performance improvement by caching the pdb_file
      // based on token.
      hr = pdb_file.GetDebugModule(&pdb_debug_module);
      if (FAILED(hr)) {
        cerr << "Failed to extract debug module from pdb file.";
        return hr;
      }

      mdModule pdb_module_token;
      hr = pdb_debug_module->GetToken(&pdb_module_token);
      if (FAILED(hr)) {
        cerr << "Failed to extract token from ICorDebugModule.";
        return hr;
      }

      if (target_module_token != pdb_module_token) {
        continue;
      }

      // Tries to populate local variables and method arguments of this frame.
      hr = PopulateLocalVarsAndMethodArgs(target_function_token, &stack_frame,
                                          il_frame, pdb_file);

      // S_FALSE is if the method is not found.
      if (hr == S_FALSE) {
        continue;
      }

      // Otherwise, some errors probably happened.
      if (FAILED(hr)) {
        cerr << "Failed to populate stack frame information.";
        return hr;
      }

      stack_frames_.push_back(std::move(stack_frame));
      break;
    }

    hr = debug_stack_walk->Next();
  }

  if (FAILED(hr)) {
    cerr << "Failed to get stack frame's information.";
  }

  return hr;
}

HRESULT StackFrameCollection::PrintStackFrames(
    Breakpoint *breakpoint, EvalCoordinator *eval_coordinator) {
  if (!breakpoint || !eval_coordinator) {
    cerr << "Null breakpoint or eval coordinator.";
    return E_INVALIDARG;
  }

  HRESULT hr = S_OK;
  eval_coordinator->WaitForReadySignal();

  for (auto &&dbg_stack_frame : stack_frames_) {
    StackFrame *frame = breakpoint->add_stack_frames();
    frame->set_method_name(dbg_stack_frame.GetClass() + "." +
                           dbg_stack_frame.GetMethod());
    SourceLocation *frame_location = frame->mutable_location();
    if (!frame_location) {
      cerr << "Mutable location returns null.";
      continue;
    }

    frame_location->set_line(dbg_stack_frame.GetLineNumber());
    frame_location->set_path(dbg_stack_frame.GetFile());

    hr = dbg_stack_frame.PopulateStackFrame(frame, eval_coordinator);
    if (FAILED(hr)) {
      return hr;
    }
  }

  eval_coordinator->SignalFinishedPrintingVariable();
  return S_OK;
}

HRESULT StackFrameCollection::PopulateLocalVarsAndMethodArgs(
    mdMethodDef target_function_token, DbgStackFrame *dbg_stack_frame,
    ICorDebugILFrame *il_frame,
    const google_cloud_debugger_portable_pdb::PortablePdbFile &pdb_file) {
  if (!dbg_stack_frame || !il_frame) {
    return E_INVALIDARG;
  }

  // Now we try to get the method that corresponds to frame_debug_function.
  CComPtr<IMetaDataImport> metadata_import;
  HRESULT hr = pdb_file.GetMetaDataImport(&metadata_import);

  mdTypeDef type_def = 0;
  ULONG method_name_length = 0;
  DWORD flags1 = 0;
  ULONG signature_blob = 0;
  ULONG rva = 0;
  DWORD flags2 = 0;
  PCCOR_SIGNATURE target_method_signature = 0;

  // Retrieves the name of the method that this stack frame is at.
  hr = metadata_import->GetMethodProps(
      target_function_token, &type_def, nullptr, 0, &method_name_length,
      &flags1, &target_method_signature, &signature_blob, &rva, &flags2);
  if (FAILED(hr)) {
    cerr << "Failed to get length of name of method for stack frame.";
    return hr;
  }

  std::vector<WCHAR> method_name(method_name_length, 0);

  hr = metadata_import->GetMethodProps(
      target_function_token, &type_def, method_name.data(), method_name.size(),
      &method_name_length, &flags1, &target_method_signature, &signature_blob,
      &rva, &flags2);
  if (FAILED(hr)) {
    cerr << "Failed to get name of method for stack frame.";
    return hr;
  }

  // Loops through all methods in all the documents of the pdb file to find
  // a MethodInfo object that corresponds with the method at this breakpoint.
  for (auto &&document_index : pdb_file.GetDocumentIndexTable()) {
    for (auto &&method : document_index.GetMethods()) {
      PCCOR_SIGNATURE current_method_signature = 0;

      // TODO(quoct): Cache the method signature in MethodInfo method so we
      // don't have to keep calling this.
      hr = metadata_import->GetMethodProps(
          method.method_def, &type_def, nullptr, 0, &method_name_length,
          &flags1, &current_method_signature, &signature_blob, &rva, &flags2);
      if (FAILED(hr)) {
        cerr << "Failed to extract method info from method "
             << method.method_def;
        return hr;
      }

      if (current_method_signature != target_method_signature) {
        continue;
      }

      // Retrieves the class name.
      // TODO(quoct): Cache the class name in MethodInfo method.
      mdToken extends_token;
      DWORD class_flags;
      ULONG class_name_length;
      hr = metadata_import->GetTypeDefProps(type_def, nullptr, 0,
                                            &class_name_length, &class_flags,
                                            &extends_token);
      if (FAILED(hr)) {
        cerr << "Failed to get length of name of class type for stack frame.";
        return hr;
      }

      std::vector<WCHAR> class_name(class_name_length, 0);
      hr = metadata_import->GetTypeDefProps(
          type_def, class_name.data(), class_name.size(), &class_name_length,
          &class_flags, &extends_token);
      if (FAILED(hr)) {
        cerr << "Failed to get name of class type for stack frame.";
        return hr;
      }

      dbg_stack_frame->SetFile(document_index.GetFilePath());
      dbg_stack_frame->SetMethod(method_name);
      dbg_stack_frame->SetClass(class_name);

      // Retrieves the IP offset in the function that corresponds to this stack
      // frame.
      ULONG32 ip_offset;
      CorDebugMappingResult mapping_result;
      hr = il_frame->GetIP(&ip_offset, &mapping_result);
      if (FAILED(hr)) {
        cerr << "Failed to get instruction pointer offset from ICorDebugFrame.";
        return hr;
      }

      // Can't show this stack frame as the mapping is not good.
      if (mapping_result == CorDebugMappingResult::MAPPING_NO_INFO ||
          mapping_result == CorDebugMappingResult::MAPPING_UNMAPPED_ADDRESS) {
        return S_FALSE;
      }

      long matching_sequence_point_position = -1;

      // We find the first non-hidden sequence point that is just larger than
      // the ip offset.
      for (long index = 0; index < method.sequence_points.size(); index++) {
        if (!method.sequence_points[index].is_hidden &&
            method.sequence_points[index].il_offset <= ip_offset) {
          matching_sequence_point_position =
              std::max(matching_sequence_point_position, index);
        }
      }

      // If we find the matching sequence point, populates the list of local
      // variables in dbg_stack_frame from the local variable's vector of the
      // matching sequence point.
      if (matching_sequence_point_position != -1) {
        SequencePoint sequence_point =
            method.sequence_points[matching_sequence_point_position];

        dbg_stack_frame->SetLineNumber(sequence_point.start_line);
        vector<LocalVariableInfo> local_variables;
        for (auto &&local_scope : method.local_scope) {
          if (local_scope.start_offset > sequence_point.il_offset ||
              local_scope.start_offset + local_scope.length <
                  sequence_point.il_offset) {
            continue;
          }

          local_variables.insert(local_variables.end(),
                                 local_scope.local_variables.begin(),
                                 local_scope.local_variables.end());
        }

        hr = dbg_stack_frame->Initialize(
            il_frame, local_variables, target_function_token, metadata_import);
      }

      return S_OK;
    }
  }

  return S_FALSE;
}

}  //  namespace google_cloud_debugger
