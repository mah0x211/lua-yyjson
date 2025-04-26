rockspec_format = "3.0"
package = "yyjson"
version = "scm-1"
source = {
    url = "git+https://github.com/mah0x211/lua-yyjson.git",
}
description = {
    summary = "lua bindings for yyjson.",
    detailed = "YYJSON is a high performance JSON library written in ANSI C.",
    homepage = "https://github.com/mah0x211/lua-yyjson",
    license = "MIT/X11",
    maintainer = "Masatoshi Fukunaga",
}
dependencies = {
    "lua >= 5.1",
    "lauxhlib >= 0.3.1",
    "errno >= 0.5.0",
    "error >= 0.14.0",
}
build = {
    type = "make",
    build_variables = {
        LIB_EXTENSION = "$(LIB_EXTENSION)",
        CFLAGS = "$(CFLAGS)",
        WARNINGS = "-Wall -Wno-trigraphs -Wmissing-field-initializers -Wreturn-type -Wmissing-braces -Wparentheses -Wno-switch -Wunused-function -Wunused-label -Wunused-parameter -Wunused-variable -Wunused-value -Wuninitialized -Wunknown-pragmas -Wshadow -Wsign-compare",
        CPPFLAGS = "-I$(LUA_INCDIR) -I./deps/yyjson/src/",
        LDFLAGS = "$(LIBFLAG)",
        YYJSON_COVERAGE = "$(YYJSON_COVERAGE)",
    },
    install_variables = {
        LIB_EXTENSION = "$(LIB_EXTENSION)",
        INST_LIBDIR = "$(LIBDIR)/yyjson",
        INST_LUADIR = "$(LUADIR)",
        LUA_INCDIR = "$(LUA_INCDIR)",
    },
}
