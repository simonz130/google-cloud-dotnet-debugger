﻿// Copyright 2017 Google Inc. All Rights Reserved.
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

using Google.Protobuf;
using Moq;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Xunit;

namespace Google.Cloud.Diagnostics.Debug.Tests
{
    public class BreakpointServerTests
    {
        private readonly BreakpointServer _server;
        private readonly Mock<INamedPipeServer> _pipeMock;
        private readonly CancellationTokenSource _cts;

        public BreakpointServerTests()
        {
            _pipeMock = new Mock<INamedPipeServer>();
            _server = new BreakpointServer(_pipeMock.Object);
            _cts = new CancellationTokenSource();
        }

        [Fact]
        public async Task ReadBreakpointAsync()
        {
            var breakpoint = new Breakpoint
            {
                Id = "some-id"
            };

            var breakpointMessage = CreateBreakpointMessage(breakpoint);
            _pipeMock.Setup(p => p.ReadAsync(_cts.Token)).Returns(Task.FromResult(breakpointMessage));

            var resultBreakpoint = await _server.ReadBreakpointAsync(_cts.Token);
            Assert.Equal(breakpoint, resultBreakpoint);
            _pipeMock.Verify(p => p.ReadAsync(_cts.Token), Times.Once());
        }

        [Fact]
        public async Task ReadBreakpointAsync_MultipleReads()
        {
            var breakpoint = new Breakpoint
            {
                Id = "some-id"
            };

            var breakpointMessage = CreateBreakpointMessage(breakpoint);

            _pipeMock.SetupSequence(p => p.ReadAsync(_cts.Token))
                .Returns(Task.FromResult(breakpointMessage.Skip(0).Take(5).ToArray()))
                .Returns(Task.FromResult(breakpointMessage.Skip(5).Take(5).ToArray()))
                .Returns(Task.FromResult(breakpointMessage.Skip(10).Take(5).ToArray()))
                .Returns(Task.FromResult(breakpointMessage.Skip(15).ToArray()));

            var resultBreakpoint = await _server.ReadBreakpointAsync(_cts.Token);
            Assert.Equal(breakpoint, resultBreakpoint);
            _pipeMock.Verify(p => p.ReadAsync(_cts.Token), Times.Exactly(4));
        }

        [Fact]
        public async Task ReadBreakpointAsync_MultipleBreakpoints()
        {
            var breakpoint1 = new Breakpoint
            {
                Id = "some-id-1"
            };
            var breakpoint2 = new Breakpoint
            {
                Id = "some-id-2"
            };
            var breakpoint3 = new Breakpoint
            {
                Id = "some-id-3"
            };

            var breakpointMessages = new List<byte>()
                .Concat(CreateBreakpointMessage(breakpoint1))
                .Concat(CreateBreakpointMessage(breakpoint2))
                .Concat(CreateBreakpointMessage(breakpoint3));

            _pipeMock.Setup(p => p.ReadAsync(_cts.Token))
                .Returns(Task.FromResult(breakpointMessages.ToArray()));

            Assert.Equal(breakpoint1, await _server.ReadBreakpointAsync(_cts.Token));
            Assert.Equal(breakpoint2, await _server.ReadBreakpointAsync(_cts.Token));
            Assert.Equal(breakpoint3, await _server.ReadBreakpointAsync(_cts.Token));
            _pipeMock.Verify(p => p.ReadAsync(_cts.Token), Times.Once());
        }

        [Fact]
        public async Task ReadBreakpointAsync_PartialBreakpoint()
        {
            var breakpoint1 = new Breakpoint
            {
                Id = "some-id-1"
            };
            var breakpoint2 = new Breakpoint
            {
                Id = "some-id-2"
            };

            var breakpointMessage1 = CreateBreakpointMessage(breakpoint1);
            var breakpointMessage2 = CreateBreakpointMessage(breakpoint2);
            var tempbreakpointMessage1 = breakpointMessage1.ToList();
            tempbreakpointMessage1.AddRange(breakpointMessage2.Take(10));
            breakpointMessage1 = tempbreakpointMessage1.ToArray();
            breakpointMessage2 = breakpointMessage2.Skip(10).ToArray();

            _pipeMock.SetupSequence(p => p.ReadAsync(_cts.Token))
                .Returns(Task.FromResult(breakpointMessage1))
                .Returns(Task.FromResult(breakpointMessage2));

            Assert.Equal(breakpoint1, await _server.ReadBreakpointAsync(_cts.Token));
            Assert.Equal(breakpoint2, await _server.ReadBreakpointAsync(_cts.Token));
            _pipeMock.Verify(p => p.ReadAsync(_cts.Token), Times.Exactly(2));
        }

        [Fact]
        public async Task ReadBreakpointAsync_InvalidBreakpoint()
        {
            var breakpoint = new Breakpoint
            {
                Id = "some-id-1"
            };
            var breakpointMessage = breakpoint.ToByteArray().ToList();
            breakpointMessage.AddRange(Constants.EndBreakpointMessage);

            _pipeMock.Setup(p => p.ReadAsync(_cts.Token)).Returns(Task.FromResult(breakpointMessage.ToArray()));

            await Assert.ThrowsAsync<InvalidOperationException>
                (async () => await _server.ReadBreakpointAsync(_cts.Token));
            _pipeMock.Verify(p => p.ReadAsync(_cts.Token), Times.Exactly(1));
        }

        [Fact]
        public void WriteBreakpointAsync()
        {
            var breakpoint = new Breakpoint
            {
                Id = "some-id"
            };

            Predicate<byte[]> matcher = (bytesArr) =>
            {
                var bytes = bytesArr.Take(Constants.StartBreakpointMessage.Length);
                Assert.Equal(bytes, Constants.StartBreakpointMessage);

                bytes = bytesArr.Skip(bytesArr.Length - Constants.EndBreakpointMessage.Length);
                Assert.Equal(bytes, Constants.EndBreakpointMessage);

                bytes = bytesArr.Skip(Constants.StartBreakpointMessage.Length).ToArray();
                bytes = bytes.Take(bytes.Count() - Constants.EndBreakpointMessage.Length);
                Assert.Equal(bytes, breakpoint.ToByteArray());

                return true;
            };

            _pipeMock.Setup(p => p.WriteAsync(Match.Create(matcher), _cts.Token));
            _server.WriteBreakpointAsync(breakpoint, _cts.Token);
            _pipeMock.VerifyAll();
            _pipeMock.Verify(p => p.WriteAsync(It.IsAny<byte[]>(), _cts.Token), Times.Once());
        }

        private byte[] CreateBreakpointMessage(Breakpoint breakpoint)
        {
            List<byte> bytes = new List<byte>();
            bytes.AddRange(Constants.StartBreakpointMessage);
            bytes.AddRange(breakpoint.ToByteArray());
            bytes.AddRange(Constants.EndBreakpointMessage);
            return bytes.ToArray();
        }
    }
}
