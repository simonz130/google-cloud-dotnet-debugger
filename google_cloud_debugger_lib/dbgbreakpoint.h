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

#ifndef DBG_BREAKPOINT_H_
#define DBG_BREAKPOINT_H_

#include <cstdint>
#include <string>
#include <vector>

#include "cor.h"
#include "documentindex.h"
#include "metadatatables.h"
#include "portablepdbfile.h"

namespace google_cloud_debugger {

class EvalCoordinator;
class StackFrameCollection;

// This class represents a breakpoint in the Debugger.
// To use the class, call the Initialize method to populate the
// file name, the id of the breakpoint, the line and column number.
// To actually set the breakpoint, the TrySetBreakpoint method must be called.
class DbgBreakpoint {
 public:
  // Populate this breakpoint with all the contents of breakpoint other.
  void Initialize(const DbgBreakpoint &other);

  // Populate this breakpoint's file name, id, line and column.
  void Initialize(const std::string &file_name, const std::string &id,
                  uint32_t line, uint32_t column);

  // Given a PortablePdbFile, try to see whether we can set this breakpoint.
  // We do this by searching the PortablePdbFile for the file name and
  // line number that matches the breakpoint. We then try to get the
  // sequence point that corresponds to this breakpoint.
  bool TrySetBreakpoint(
      const google_cloud_debugger_portable_pdb::PortablePdbFile &pdb_file);

  // Returns the IL Offset that corresponds to this breakpoint location.
  uint32_t GetILOffset() { return il_offset_; }

  // Returns the method definition of the method this breakpoint is in.
  // This must be set through TrySetBreakpoint.
  uint32_t GetMethodDef() { return method_def_; }

  // Returns the method token of the method this breakpoint is in.
  mdMethodDef GetMethodToken() { return method_token_; }

  // Sets the method token.
  void SetMethodToken(mdMethodDef method_token) {
    method_token_ = method_token;
  }

  // Returns the name of the file this breakpoint is in.
  const std::string &GetFileName() const { return file_name_; }

  // Returns the name of the method this breakpoint is in.
  const std::vector<WCHAR> &GetMethodName() const { return method_name_; }

  // Sets the name of the method this breakpoint is in.
  void SetMethodName(std::vector<WCHAR> method_name) {
    method_name_ = method_name;
  }

  // Gets the line number of this breakpoint.
  uint32_t GetLine() const { return line_; }

  // Returns true if this breakpoint is set.
  // When a breakpoint is set, its il_off_set is set.
  bool IsSet() const { return set_; }

  // Returns the unique ID of this breakpoint.
  const std::string &GetId() const { return id_; }

  // Sets the ICorDebugBreakpoint that corresponds with this breakpoint.
  void SetCorDebugBreakpoint(ICorDebugBreakpoint *debug_breakpoint) {
    debug_breakpoint_ = debug_breakpoint;
  }
  // Gets the ICorDebugBreakpoint that corresponds with this breakpoint.
  HRESULT GetCorDebugBreakpoint(ICorDebugBreakpoint **debug_breakpoint) const;

  // Sets whether this breakpoint is activated or not.
  void SetActivated(bool activated) { activated_ = activated; };

  // Returns whether this breakpoint is activated or not.
  bool Activated() const { return activated_; }

  // Creates a Breakpoint proto using this breakpoint information.
  // StackFrameCollection stack_frames and EvalCoordinator eval_coordinator
  // are used to evaluate and fill up the stack frames of the breakpoint.
  // This function then outputs the breakpoint to the named pipe of
  // BreakpointCollection.
  HRESULT PrintBreakpoint(StackFrameCollection *stack_frames,
                          EvalCoordinator *eval_coordinator);

 private:
  // Given a method, try to see whether we can set this breakpoint in
  // the method.
  bool TrySetBreakpointInMethod(
      const google_cloud_debugger_portable_pdb::MethodInfo &method);

  // True if this breakpoint is set (through TryGetBreakpoint).
  bool set_;

  // The line number of the breakpoint.
  uint32_t line_;

  // The column number of the breakpoint.
  uint32_t column_;

  // The file name of the breakpoint.
  std::string file_name_;

  // The unique ID of the breakpoint.
  std::string id_;

  // The IL Offset of this breakpoint.
  uint32_t il_offset_;

  // The method definition of the method this breakpoint is in.
  uint32_t method_def_;

  // The method token of the method this breakpoint is in.
  mdMethodDef method_token_;

  // The name of the method this breakpoint is in.
  std::vector<WCHAR> method_name_;

  // True if this breakpoint is activated.
  bool activated_;

  // The ICorDebugBreakpoint that corresponds with this breakpoint.
  CComPtr<ICorDebugBreakpoint> debug_breakpoint_;
};

}  // namespace google_cloud_debugger

#endif
