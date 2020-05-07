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
    "@io_bazel_rules_go//go/private:common.bzl",
    "has_simple_shared_lib_extension",
    "has_versioned_shared_lib_extension",
)
load(
    "@io_bazel_rules_go//go/private:mode.bzl",
    "LINKMODE_C_ARCHIVE",
    "LINKMODE_C_SHARED",
    "LINKMODE_NORMAL",
    "extldflags_from_cc_toolchain",
)
load(
    "@io_bazel_rules_go_compat//:compat.bzl",
    "cc_defines",
    "cc_includes",
    "cc_libs",
    "cc_link_flags",
    "cc_quote_includes",
    "cc_system_includes",
    "cc_transitive_headers",
    "has_cc",
)

def cgo_configure(go, srcs, cdeps, cppopts, copts, cxxopts, clinkopts):
    """cgo_configure returns the inputs and compile / link options
    that are required to build a cgo archive.

    Args:
        go: a GoContext.
        srcs: list of source files being compiled. Include options are added
            for the headers.
        cdeps: list of Targets for C++ dependencies. Include and link options
            may be added.
        cppopts: list of C preprocessor options for the library.
        copts: list of C compiler options for the library.
        cxxopts: list of C++ compiler options for the library.
        clinkopts: list of linker options for the library.

    Returns: a struct containing:
        inputs: depset of files that must be available for the build.
        deps: depset of files for dynamic libraries.
        runfiles: runfiles object for the C/C++ dependencies.
        cppopts: complete list of preprocessor options
        copts: complete list of C compiler options.
        cxxopts: complete list of C++ compiler options.
        objcopts: complete list of Objective-C compiler options.
        objcxxopts: complete list of Objective-C++ compiler options.
        clinkopts: complete list of linker options.
    """
    if not go.cgo_tools:
        fail("Go toolchain does not support cgo")

    cppopts = list(cppopts)
    base_dir, _, _ = go._ctx.build_file_path.rpartition("/")
    if base_dir:
        cppopts.extend(["-I", base_dir])
    copts = go.cgo_tools.c_compile_options + copts
    cxxopts = go.cgo_tools.cxx_compile_options + cxxopts
    objcopts = go.cgo_tools.objc_compile_options + copts
    objcxxopts = go.cgo_tools.objcxx_compile_options + cxxopts
    clinkopts = extldflags_from_cc_toolchain(go) + clinkopts
    if go.mode != LINKMODE_NORMAL:
        for opt_list in (copts, cxxopts, objcopts, objcxxopts):
            if "-fPIC" not in opt_list:
                opt_list.append("-fPIC")

    seen_includes = {}
    seen_quote_includes = {}
    seen_system_includes = {}
    for f in srcs:
        if f.basename.endswith(".h"):
            _include_unique(cppopts, "-iquote", f.dirname, seen_quote_includes)

    inputs_direct = []
    inputs_transitive = []
    deps_direct = []
    lib_opts = []
    runfiles = go._ctx.runfiles(collect_data = True)
    # Always include the sandbox as part of the build. Bazel does this, but it
    # doesn't appear in the CompilationContext.
    _include_unique(cppopts, "-iquote", ".", seen_quote_includes)
    for d in cdeps:
        runfiles = runfiles.merge(d.data_runfiles)
        if has_cc(d):
            inputs_transitive.append(cc_transitive_headers(d))
            inputs_direct.extend(cc_libs(d))
            deps_direct.extend(cc_libs(d))
            cppopts.extend(["-D" + define for define in cc_defines(d)])
            for inc in cc_includes(d):
                _include_unique(cppopts, "-I", inc, seen_includes)
            for inc in cc_quote_includes(d):
                _include_unique(cppopts, "-iquote", inc, seen_quote_includes)
            for inc in cc_system_includes(d):
                _include_unique(cppopts, "-isystem", inc, seen_system_includes)
            for lib in cc_libs(d):
                # If both static and dynamic variants are available, Bazel will only give
                # us the static variant. We'll get one file for each transitive dependency,
                # so the same file may appear more than once.
                if (lib.basename.startswith("lib") and
                    has_simple_shared_lib_extension(lib.basename)):
                    # If the loader would be able to find the library using rpaths,
                    # use -L and -l instead of hard coding the path to the library in
                    # the binary. This gives users more flexibility. The linker will add
                    # rpaths later. We can't add them here because they are relative to
                    # the binary location, and we don't know where that is.
                    libname = lib.basename[len("lib"):lib.basename.rindex(".")]
                    clinkopts.extend(["-L", lib.dirname, "-l", libname])
                    inputs_direct.append(lib)
                elif (lib.basename.startswith("lib") and
                      has_versioned_shared_lib_extension(lib.basename)):
                    # With a versioned shared library, we must use the full filename,
                    # otherwise the library will not be found by the linker.
                    libname = ":%s" % lib.basename
                    clinkopts.extend(["-L", lib.dirname, "-l", libname])
                    inputs_direct.append(lib)
                else:
                    lib_opts.append(lib.path)
            clinkopts.extend(cc_link_flags(d))

        elif hasattr(d, "objc"):
            cppopts.extend(["-D" + define for define in d.objc.define.to_list()])
            for inc in d.objc.include.to_list():
                _include_unique(cppopts, "-I", inc, seen_includes)
            for inc in d.objc.iquote.to_list():
                _include_unique(cppopts, "-iquote", inc, seen_quote_includes)
            for inc in d.objc.include_system.to_list():
                _include_unique(cppopts, "-isystem", inc, seen_system_includes)

            # TODO(jayconrod): do we need to link against dynamic libraries or
            # frameworks? We link against *_fully_linked.a, so maybe not?

        else:
            fail("unknown library has neither cc nor objc providers: %s" % d.label)

    inputs = depset(direct = inputs_direct, transitive = inputs_transitive)
    deps = depset(direct = deps_direct)

    # HACK: some C/C++ toolchains will ignore libraries (including dynamic libs
    # specified with -l flags) unless they appear after .o or .a files with
    # undefined symbols they provide. Put all the .a files from cdeps first,
    # so that we actually link with -lstdc++ and others.
    clinkopts = lib_opts + clinkopts

    return struct(
        inputs = inputs,
        deps = deps,
        runfiles = runfiles,
        cppopts = cppopts,
        copts = copts,
        cxxopts = cxxopts,
        objcopts = objcopts,
        objcxxopts = objcxxopts,
        clinkopts = clinkopts,
    )

_DEFAULT_PLATFORM_COPTS = select({
    "@io_bazel_rules_go//go/platform:darwin": [],
    "@io_bazel_rules_go//go/platform:windows_amd64": ["-mthreads"],
    "//conditions:default": ["-pthread"],
})

def _include_unique(opts, flag, include, seen):
    if include in seen:
        return
    seen[include] = True
    opts.extend([flag, include])

# Sets up the cc_ targets when a go_binary is built in either c-archive or
# c-shared mode.
def go_binary_c_archive_shared(name, kwargs):
    linkmode = kwargs.get("linkmode")
    if linkmode not in [LINKMODE_C_SHARED, LINKMODE_C_ARCHIVE]:
        return
    cgo_exports = name + ".cgo_exports"
    c_hdrs = name + ".c_hdrs"
    cc_import_name = name + ".cc_import"
    cc_library_name = name + ".cc"
    tags = kwargs.get("tags", ["manual"])
    if "manual" not in tags:
        # These archives can't be built on all platforms, so use "manual" tags.
        tags.append("manual")
    native.filegroup(
        name = cgo_exports,
        srcs = [name],
        output_group = "cgo_exports",
        visibility = ["//visibility:private"],
        tags = tags,
    )
    native.genrule(
        name = c_hdrs,
        srcs = [cgo_exports],
        outs = ["%s.h" % name],
        cmd = "cat $(SRCS) > $(@)",
        visibility = ["//visibility:private"],
        tags = tags,
    )
    cc_import_kwargs = {}
    if linkmode == LINKMODE_C_SHARED:
        cc_import_kwargs["shared_library"] = name
    elif linkmode == LINKMODE_C_ARCHIVE:
        cc_import_kwargs["static_library"] = name
        cc_import_kwargs["alwayslink"] = 1
    native.cc_import(
        name = cc_import_name,
        visibility = ["//visibility:private"],
        tags = tags,
        **cc_import_kwargs
    )
    native.cc_library(
        name = cc_library_name,
        hdrs = [c_hdrs],
        deps = [cc_import_name],
        alwayslink = 1,
        linkstatic = (linkmode == LINKMODE_C_ARCHIVE and 1 or 0),
        copts = _DEFAULT_PLATFORM_COPTS,
        linkopts = _DEFAULT_PLATFORM_COPTS,
        visibility = ["//visibility:public"],
        tags = tags,
    )
