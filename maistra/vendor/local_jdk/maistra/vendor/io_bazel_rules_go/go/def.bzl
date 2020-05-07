# Copyright 2014 The Bazel Authors. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load(
    "@io_bazel_rules_go//go/private:context.bzl",
    _go_context = "go_context",
)
load(
    "@io_bazel_rules_go//go/private:providers.bzl",
    _GoArchive = "GoArchive",
    _GoArchiveData = "GoArchiveData",
    _GoLibrary = "GoLibrary",
    _GoPath = "GoPath",
    _GoSDK = "GoSDK",
    _GoSource = "GoSource",
)
load(
    "@io_bazel_rules_go//go/private:rules/sdk.bzl",
    _go_sdk = "go_sdk",
)
load(
    "@io_bazel_rules_go//go/private:go_toolchain.bzl",
    _declare_toolchains = "declare_toolchains",
    _go_toolchain = "go_toolchain",
)
load(
    "@io_bazel_rules_go//go/private:rules/wrappers.bzl",
    _go_binary_macro = "go_binary_macro",
    _go_library_macro = "go_library_macro",
    _go_test_macro = "go_test_macro",
)
load(
    "@io_bazel_rules_go//go/private:rules/source.bzl",
    _go_source = "go_source",
)
load(
    "@io_bazel_rules_go//extras:embed_data.bzl",
    _go_embed_data = "go_embed_data",
)
load(
    "@io_bazel_rules_go//go/private:tools/path.bzl",
    _go_path = "go_path",
)
load(
    "@io_bazel_rules_go//go/private:rules/rule.bzl",
    _go_rule = "go_rule",
)
load(
    "@io_bazel_rules_go//go/private:rules/library.bzl",
    _go_tool_library = "go_tool_library",
)
load(
    "@io_bazel_rules_go//go/private:rules/nogo.bzl",
    _nogo = "nogo_wrapper",
)

# Current version or next version to be tagged. Gazelle and other tools may
# check this to determine compatibility.
RULES_GO_VERSION = "0.20.1"

declare_toolchains = _declare_toolchains
go_context = _go_context
go_embed_data = _go_embed_data
go_sdk = _go_sdk
go_tool_library = _go_tool_library
go_toolchain = _go_toolchain
nogo = _nogo

GoLibrary = _GoLibrary
"""See go/providers.rst#GoLibrary for full documentation."""

GoSource = _GoSource
"""See go/providers.rst#GoSource for full documentation."""

GoPath = _GoPath
"""See go/providers.rst#GoPath for full documentation."""

GoArchive = _GoArchive
"""See go/providers.rst#GoArchive for full documentation."""

GoArchiveData = _GoArchiveData
"""See go/providers.rst#GoArchiveData for full documentation."""

GoSDK = _GoSDK
"""See go/providers.rst#GoSDK for full documentation."""

go_library = _go_library_macro
"""See go/core.rst#go_library for full documentation."""

go_binary = _go_binary_macro
"""See go/core.rst#go_binary for full documentation."""

go_test = _go_test_macro
"""See go/core.rst#go_test for full documentation."""

go_source = _go_source
"""See go/core.rst#go_test for full documentation."""

go_rule = _go_rule
"""See go/core.rst#go_rule for full documentation."""

go_path = _go_path
"""
    go_path is a rule for creating `go build` compatible file layouts from a set of Bazel.
    targets.
        "deps": attr.label_list(providers=[GoLibrary]), # The set of go libraries to include the export
        "mode": attr.string(default="link", values=["link", "copy"]) # Whether to copy files or produce soft links
"""

def go_vet_test(*args, **kwargs):
    fail("The go_vet_test rule has been removed. Please migrate to nogo instead, which supports vet tests.")

def go_rules_dependencies():
    _moved("go_rules_dependencies")

def go_register_toolchains(**kwargs):
    _moved("go_register_toolchains")

def go_download_sdk(**kwargs):
    _moved("go_download_sdk")

def go_host_sdk(**kwargs):
    _moved("go_host_sdk")

def go_local_sdk(**kwargs):
    _moved("go_local_sdk")

def go_wrap_sdk(**kwargs):
    _moved("go_wrap_sdK")

def _moved(name):
    fail(name + " has moved. Please load from " +
         " @io_bazel_rules_go//go:deps.bzl instead of def.bzl.")
