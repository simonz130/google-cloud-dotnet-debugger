﻿// Copyright 2015-2016 Google Inc. All Rights Reserved.
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

using CommandLine;
using Google.Api.Gax;
using System.IO;

namespace Google.Cloud.Diagnostics.Debug
{
    /// <summary>
    /// Options for starting a <see cref="Debuglet"/>.
    /// </summary>
    internal class AgentOptions
    {
        // If given this option, the debugger will not perform property evaluation.
        public const string PropertyEvaluationOption = "--property-evaluation";

        [Option("module", Required = true, HelpText = "The name of the application to debug.")]
        public string Module { get; set; }

        [Option("version", Required = true, HelpText = "The version of the application to debug.")]
        public string Version { get; set; }

        [Option("debugger", Required = true, HelpText = "A path to the debugger to use.")]
        public string Debugger { get; set; }

        [Option("application", Required = true,
            HelpText = "A path to the .NET CORE application dll to be debugged.")]
        public string Application { get; set; }

        [Option("project-id",
            HelpText = "The Google Cloud Console project the debuggee is associated with.")]
        public string ProjectId { get; set; }

        [Option("property-evaluation",
            HelpText = "If set, the debugger will evaluate object's properties.")]
        public bool PropertyEvaluation { get; set; }

        [Option("wait-time", Default = 2, 
            HelpText = "The amount of time to wait before checking for new breakpoints in seconds.")]
        public int WaitTime { get; set; }

        // Returns the processed arguments to pass to the debugger. The argument will be separated
        // by white space. The first argument is the application name, the second argument is
        // whether property will be evaluated.
        public string DebuggerArguments
        {
            get
            {
                return PropertyEvaluation ? Application + PropertyEvaluationOption : Application;
            }
        }

        /// <summary>
        /// Parse a <see cref="AgentOptions"/> from command line arguments.
        /// </summary>
        public static AgentOptions Parse(string[] args)
        {
            var result = Parser.Default.ParseArguments<AgentOptions>(args);
            var options = new AgentOptions();
            result.WithParsed((o) => 
            {
                GaxPreconditions.CheckNotNullOrEmpty(o.Module, nameof(o.Module));
                GaxPreconditions.CheckNotNullOrEmpty(o.Version, nameof(o.Version));
                GaxPreconditions.CheckNotNullOrEmpty(o.Debugger, nameof(o.Debugger));
                GaxPreconditions.CheckNotNullOrEmpty(o.Application, nameof(o.Application));
                GaxPreconditions.CheckNotNullOrEmpty(o.ProjectId ?? Common.Platform.ProjectId, nameof(o.ProjectId));
                GaxPreconditions.CheckArgumentRange(o.WaitTime, nameof(o.WaitTime), 0, int.MaxValue);

                if (!File.Exists(o.Debugger))
                {
                    throw new FileNotFoundException($"Debugger file not found: '{o.Debugger}'");
                }

                o.Application = Path.GetFullPath(o.Application);
                if (!File.Exists(o.Application))
                {
                    throw new FileNotFoundException($"Application file not found: '{o.Application}'");
                }

                options = o;
            });
            return options;
        }
    }
}
