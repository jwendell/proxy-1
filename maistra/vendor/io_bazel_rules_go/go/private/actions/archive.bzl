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
    "as_tuple",
    "split_srcs",
)
load(
    "@io_bazel_rules_go//go/private:mode.bzl",
    "LINKMODE_C_ARCHIVE",
    "LINKMODE_C_SHARED",
    "mode_string",
)
load(
    "@io_bazel_rules_go//go/private:providers.bzl",
    "GoArchive",
    "GoArchiveData",
    "effective_importpath_pkgpath",
    "get_archive",
)
load(
    "@io_bazel_rules_go//go/private:rules/cgo.bzl",
    "cgo_configure",
)
load(
    "@io_bazel_rules_go//go/private:actions/compilepkg.bzl",
    "emit_compilepkg",
)

def emit_archive(go, source = None):
    """See go/toolchains.rst#archive for full documentation."""

    if source == None:
        fail("source is a required parameter")

    split = split_srcs(source.srcs)
    lib_name = source.library.importmap + ".a"
    out_lib = go.declare_file(go, path = lib_name)
    if go.nogo:
        # TODO(#1847): write nogo data into a new section in the .a file instead
        # of writing a separate file.
        out_export = go.declare_file(go, path = lib_name[:-len(".a")] + ".x")
    else:
        out_export = None
    searchpath = out_lib.path[:-len(lib_name)]
    testfilter = getattr(source.library, "testfilter", None)
    out_cgo_export_h = None  # set if cgo used in c-shared or c-archive mode

    direct = [get_archive(dep) for dep in source.deps]
    runfiles = source.runfiles
    data_files = runfiles.files
    for a in direct:
        runfiles = runfiles.merge(a.runfiles)
        if a.source.mode != go.mode:
            fail("Archive mode does not match {} is {} expected {}".format(a.data.label, mode_string(a.source.mode), mode_string(go.mode)))

    importmap = "main" if source.library.is_main else source.library.importmap
    importpath, _ = effective_importpath_pkgpath(source.library)

    if source.cgo and not go.mode.pure:
        # TODO(jayconrod): do we need to do full Bourne tokenization here?
        cppopts = [f for fs in source.cppopts for f in fs.split(" ")]
        copts = [f for fs in source.copts for f in fs.split(" ")]
        cxxopts = [f for fs in source.cxxopts for f in fs.split(" ")]
        clinkopts = [f for fs in source.clinkopts for f in fs.split(" ")]
        cgo = cgo_configure(
            go,
            srcs = split.go + split.c + split.asm + split.cxx + split.objc + split.headers,
            cdeps = source.cdeps,
            cppopts = cppopts,
            copts = copts,
            cxxopts = cxxopts,
            clinkopts = clinkopts,
        )
        if go.mode.link in (LINKMODE_C_SHARED, LINKMODE_C_ARCHIVE):
            out_cgo_export_h = go.declare_file(go, path = "_cgo_install.h")
        cgo_deps = cgo.deps
        runfiles = runfiles.merge(cgo.runfiles)
        emit_compilepkg(
            go,
            sources = split.go + split.c + split.asm + split.cxx + split.objc + split.headers,
            cover = source.cover,
            importpath = importpath,
            importmap = importmap,
            archives = direct,
            out_lib = out_lib,
            out_export = out_export,
            out_cgo_export_h = out_cgo_export_h,
            gc_goopts = source.gc_goopts,
            cgo = True,
            cgo_inputs = cgo.inputs,
            cppopts = cgo.cppopts,
            copts = cgo.copts,
            cxxopts = cgo.cxxopts,
            objcopts = cgo.objcopts,
            objcxxopts = cgo.objcxxopts,
            clinkopts = cgo.clinkopts,
            testfilter = testfilter,
        )
    else:
        cgo_deps = depset()
        emit_compilepkg(
            go,
            sources = split.go + split.c + split.asm + split.cxx + split.objc + split.headers,
            cover = source.cover,
            importpath = importpath,
            importmap = importmap,
            archives = direct,
            out_lib = out_lib,
            out_export = out_export,
            gc_goopts = source.gc_goopts,
            cgo = False,
            testfilter = testfilter,
        )

    data = GoArchiveData(
        name = source.library.name,
        label = source.library.label,
        importpath = source.library.importpath,
        importmap = source.library.importmap,
        importpath_aliases = source.library.importpath_aliases,
        pathtype = source.library.pathtype,
        file = out_lib,
        export_file = out_export,
        srcs = as_tuple(source.srcs),
        orig_srcs = as_tuple(source.orig_srcs),
        data_files = as_tuple(data_files),
        searchpath = searchpath,
    )
    x_defs = dict(source.x_defs)
    for a in direct:
        x_defs.update(a.x_defs)
    cgo_exports_direct = list(source.cgo_exports)
    if out_cgo_export_h:
        cgo_exports_direct.append(out_cgo_export_h)
    cgo_exports = depset(direct = cgo_exports_direct, transitive = [a.cgo_exports for a in direct])
    return GoArchive(
        source = source,
        data = data,
        direct = direct,
        searchpaths = depset(direct = [searchpath], transitive = [a.searchpaths for a in direct]),
        libs = depset(direct = [out_lib], transitive = [a.libs for a in direct]),
        transitive = depset([data], transitive = [a.transitive for a in direct]),
        x_defs = x_defs,
        cgo_deps = depset(transitive = [cgo_deps] + [a.cgo_deps for a in direct]),
        cgo_exports = cgo_exports,
        runfiles = runfiles,
        mode = go.mode,
    )
