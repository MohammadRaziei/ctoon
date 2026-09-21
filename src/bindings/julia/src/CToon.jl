module CToon

using OrderedCollections: OrderedDict

# Loaded by build.jl into a shared lib containing ctoon.c + shim.c (the
# same ctoon_rs_* exported wrappers the Rust and Zig bindings use for
# ctoon's `static inline` API — see ../shim.c for why those wrappers
# exist). Covers reading (parse/parse_toon) and writing (dumps/to_json)
# for the most common ctoon_type cases.

const depsjl = joinpath(@__DIR__, "..", "deps", "deps.jl")
isfile(depsjl) || error(
    "CToon is not built. Run `import Pkg; Pkg.build(\"CToon\")` " *
    "(or `julia deps/build.jl` from this binding's directory) first.",
)
include(depsjl)

# --- ctoon_type (see include/ctoon.h) ---
const TYPE_NONE = 0
const TYPE_RAW = 1
const TYPE_NULL = 2
const TYPE_BOOL = 3
const TYPE_NUM = 4
const TYPE_STR = 5
const TYPE_ARR = 6
const TYPE_OBJ = 7

# --- opaque handles ---
struct Doc
    ptr::Ptr{Cvoid}
end

struct Val
    ptr::Ptr{Cvoid}
end

function doc_free(doc::Doc)
    doc.ptr == C_NULL || @ccall libctoon_jl.ctoon_rs_doc_free(doc.ptr::Ptr{Cvoid})::Cvoid
    nothing
end

# Mirrors ctoon_read_err's C layout (include/ctoon.h): { ctoon_read_code
# code (uint32_t); const char *msg; size_t pos; } — used so parse
# failures report the real reason instead of a generic message.
struct ReadErr
    code::UInt32
    _pad::UInt32
    msg::Ptr{UInt8}
    pos::UInt64
end

"""
    CToon.parse(str::AbstractString) -> Any

Parse `str` (TOON/JSON per ctoon's reader) and return it as native Julia
data: `OrderedDict{String,Any}` for objects (order-preserving — see
`_to_julia`'s own comment), `Vector{Any}` for arrays, `String`,
`Int64`/`Float64`, `Bool`, or `nothing`.
"""
function parse(str::AbstractString)
    # ctoon_read()/ctoon_read_opts() parse ctoon's *native* TOON syntax
    # (tabular/pipe-delimited arrays — see the C++ binding's
    # arrays_tabular tests) — comma/bracket JSON input isn't valid there
    # ("malformed delimiter marker in bracket segment" is exactly what
    # ctoon_read_opts says about `[1, 2, 3]`). For JSON-style input, call
    # ctoon_read_json() instead — a real exported symbol (no shim needed,
    # unlike ctoon_read() which is `ctoon_api_inline`) — same as the Go
    # binding's bridge.c does for its `asJSON` path. Wants a mutable
    # buffer; NULL for the allocator (4th arg) is fine, but pass a real
    # ctoon_read_err (5th arg) so a failure reports why.
    buf = Vector{UInt8}(str)
    err = Ref(ReadErr(0, 0, C_NULL, 0))
    doc = Doc(@ccall libctoon_jl.ctoon_read_json(
        buf::Ptr{UInt8}, sizeof(buf)::Csize_t, 0::Cuint, C_NULL::Ptr{Cvoid}, err::Ptr{ReadErr},
    )::Ptr{Cvoid})
    if doc.ptr == C_NULL
        e = err[]
        msg = e.msg == C_NULL ? "(no message)" : unsafe_string(e.msg)
        error("CToon: failed to parse input: $msg (code=$(e.code), pos=$(e.pos))")
    end
    try
        root_ptr = @ccall libctoon_jl.ctoon_rs_doc_get_root(doc.ptr::Ptr{Cvoid})::Ptr{Cvoid}
        return _to_julia(Val(root_ptr))
    finally
        doc_free(doc)
    end
end

function _get_type(v::Val)
    @ccall libctoon_jl.ctoon_rs_get_type(v.ptr::Ptr{Cvoid})::UInt8
end

function _to_julia(v::Val)
    v.ptr == C_NULL && return nothing
    t = _get_type(v)
    if t == TYPE_NULL
        return nothing
    elseif t == TYPE_BOOL
        return @ccall libctoon_jl.ctoon_rs_get_bool(v.ptr::Ptr{Cvoid})::Bool
    elseif t == TYPE_NUM
        if @ccall(libctoon_jl.ctoon_rs_is_sint(v.ptr::Ptr{Cvoid})::Bool)
            return @ccall libctoon_jl.ctoon_rs_get_sint(v.ptr::Ptr{Cvoid})::Int64
        elseif @ccall(libctoon_jl.ctoon_rs_is_uint(v.ptr::Ptr{Cvoid})::Bool)
            return @ccall libctoon_jl.ctoon_rs_get_uint(v.ptr::Ptr{Cvoid})::UInt64
        else
            return @ccall libctoon_jl.ctoon_rs_get_real(v.ptr::Ptr{Cvoid})::Cdouble
        end
    elseif t == TYPE_STR
        cptr = @ccall libctoon_jl.ctoon_rs_get_str(v.ptr::Ptr{Cvoid})::Ptr{UInt8}
        len = @ccall libctoon_jl.ctoon_rs_get_len(v.ptr::Ptr{Cvoid})::Csize_t
        return unsafe_string(cptr, len)
    elseif t == TYPE_ARR
        n = @ccall libctoon_jl.ctoon_rs_arr_size(v.ptr::Ptr{Cvoid})::Csize_t
        out = Vector{Any}(undef, n)
        for i in 0:(n - 1)
            item_ptr = @ccall libctoon_jl.ctoon_rs_arr_get(v.ptr::Ptr{Cvoid}, i::Csize_t)::Ptr{Cvoid}
            out[i + 1] = _to_julia(Val(item_ptr))
        end
        return out
    elseif t == TYPE_OBJ
        # OrderedDict, not Base.Dict: unlike Python's dict (insertion-
        # ordered since 3.7), Base.Dict does NOT preserve insertion
        # order on iteration -- which would silently scramble key order
        # on every parse -> dumps/to_json round trip through this
        # binding, unlike every other language binding in this repo.
        # OrderedDict (OrderedCollections.jl) preserves it, matching
        # ctoon's own ctoon_val tree (order-preserving via
        # ctoon_obj_iter) and every other language binding here.
        out = OrderedDict{String,Any}()
        # ctoon_obj_iter is a small fixed-layout struct; the shim exposes
        # it opaquely via a heap-free stack buffer here rather than
        # binding its C layout directly — good enough for a skeleton,
        # worth revisiting (a real `ctoon_obj_iter` Julia struct mirroring
        # the C one) once the rest of the shape is settled.
        iter_buf = zeros(UInt8, 64) # oversized scratch space for ctoon_obj_iter
        GC.@preserve iter_buf begin
            iter_ptr = pointer(iter_buf)
            ok = @ccall libctoon_jl.ctoon_rs_obj_iter_init(v.ptr::Ptr{Cvoid}, iter_ptr::Ptr{Cvoid})::Bool
            ok || return out
            while @ccall(libctoon_jl.ctoon_rs_obj_iter_has_next(iter_ptr::Ptr{Cvoid})::Bool)
                key_ptr = @ccall libctoon_jl.ctoon_rs_obj_iter_next(iter_ptr::Ptr{Cvoid})::Ptr{Cvoid}
                key_ptr == C_NULL && break
                val_ptr = @ccall libctoon_jl.ctoon_rs_obj_iter_get_val(key_ptr::Ptr{Cvoid})::Ptr{Cvoid}
                key = _to_julia(Val(key_ptr))
                out[key] = _to_julia(Val(val_ptr))
            end
        end
        return out
    else
        return nothing
    end
end

export parse

# ------------------------------------------------------------- writing --
# Julia value -> ctoon_mut_doc (via the ctoon_rs_mut_* shims), then
# written out as either TOON (ctoon_rs_mut_write) or JSON
# (ctoon_mut_doc_to_json -- a real exported symbol, no shim needed,
# same as ctoon_read_json() above). Mirrors _to_julia's structure in
# reverse: one recursive function building up ctoon_mut_val nodes
# instead of tearing a ctoon_val tree down into Julia values.

function _mut_string(doc::Ptr{Cvoid}, s::AbstractString)
    b = codeunits(s)
    GC.@preserve b begin
        @ccall libctoon_jl.ctoon_rs_mut_strncpy(
            doc::Ptr{Cvoid}, pointer(b)::Ptr{UInt8}, length(b)::Csize_t,
        )::Ptr{Cvoid}
    end
end

function _from_julia(doc::Ptr{Cvoid}, v::Nothing)
    @ccall libctoon_jl.ctoon_rs_mut_null(doc::Ptr{Cvoid})::Ptr{Cvoid}
end
function _from_julia(doc::Ptr{Cvoid}, v::Bool)
    v ? (@ccall libctoon_jl.ctoon_rs_mut_true(doc::Ptr{Cvoid})::Ptr{Cvoid}) :
        (@ccall libctoon_jl.ctoon_rs_mut_false(doc::Ptr{Cvoid})::Ptr{Cvoid})
end
function _from_julia(doc::Ptr{Cvoid}, v::Signed)
    @ccall libctoon_jl.ctoon_rs_mut_sint(doc::Ptr{Cvoid}, Int64(v)::Int64)::Ptr{Cvoid}
end
function _from_julia(doc::Ptr{Cvoid}, v::Unsigned)
    @ccall libctoon_jl.ctoon_rs_mut_uint(doc::Ptr{Cvoid}, UInt64(v)::UInt64)::Ptr{Cvoid}
end
function _from_julia(doc::Ptr{Cvoid}, v::AbstractFloat)
    @ccall libctoon_jl.ctoon_rs_mut_real(doc::Ptr{Cvoid}, Float64(v)::Cdouble)::Ptr{Cvoid}
end
function _from_julia(doc::Ptr{Cvoid}, v::AbstractString)
    _mut_string(doc, v)
end
function _from_julia(doc::Ptr{Cvoid}, v::AbstractVector)
    arr = @ccall libctoon_jl.ctoon_rs_mut_arr(doc::Ptr{Cvoid})::Ptr{Cvoid}
    for item in v
        item_val = _from_julia(doc, item)
        @ccall libctoon_jl.ctoon_rs_mut_arr_append(arr::Ptr{Cvoid}, item_val::Ptr{Cvoid})::Bool
    end
    return arr
end
function _from_julia(doc::Ptr{Cvoid}, v::AbstractDict)
    obj = @ccall libctoon_jl.ctoon_rs_mut_obj(doc::Ptr{Cvoid})::Ptr{Cvoid}
    for (k, val) in v
        key_val = _mut_string(doc, string(k))
        item_val = _from_julia(doc, val)
        @ccall libctoon_jl.ctoon_rs_mut_obj_put(obj::Ptr{Cvoid}, key_val::Ptr{Cvoid}, item_val::Ptr{Cvoid})::Bool
    end
    return obj
end

function _new_mut_doc(v)
    doc = @ccall libctoon_jl.ctoon_mut_doc_new(C_NULL::Ptr{Cvoid})::Ptr{Cvoid}
    doc == C_NULL && error("CToon: ctoon_mut_doc_new failed (out of memory)")
    root = _from_julia(doc, v)
    @ccall libctoon_jl.ctoon_rs_mut_doc_set_root(doc::Ptr{Cvoid}, root::Ptr{Cvoid})::Cvoid
    return doc
end

function _mut_doc_free(doc::Ptr{Cvoid})
    doc == C_NULL || @ccall libctoon_jl.ctoon_mut_doc_free(doc::Ptr{Cvoid})::Cvoid
    nothing
end

"""
    CToon.dumps(value) -> String

Encode a native Julia value (as `CToon.parse` would produce: `AbstractDict`,
`Vector`, `AbstractString`, `Bool`, `Integer`, `AbstractFloat`, or
`nothing`) as TOON text.
"""
function dumps(v)
    doc = _new_mut_doc(v)
    try
        len = Ref{Csize_t}(0)
        cptr = @ccall libctoon_jl.ctoon_rs_mut_write(doc::Ptr{Cvoid}, len::Ptr{Csize_t})::Ptr{UInt8}
        cptr == C_NULL && error("CToon: ctoon_mut_write failed")
        return unsafe_string(cptr, len[])
    finally
        _mut_doc_free(doc)
    end
end

"""
    CToon.to_json(value) -> String

Encode a native Julia value the same way `CToon.dumps` does, but as
JSON text instead of TOON.
"""
function to_json(v)
    doc = _new_mut_doc(v)
    try
        len = Ref{Csize_t}(0)
        cptr = @ccall libctoon_jl.ctoon_mut_doc_to_json(
            doc::Ptr{Cvoid}, 2::Cint, 0::Cuint, C_NULL::Ptr{Cvoid}, len::Ptr{Csize_t}, C_NULL::Ptr{Cvoid},
        )::Ptr{UInt8}
        cptr == C_NULL && error("CToon: ctoon_mut_doc_to_json failed")
        return unsafe_string(cptr, len[])
    finally
        _mut_doc_free(doc)
    end
end

# ------------------------------------------------------------- TOON read --
# ctoon_read_opts() parses ctoon's *native* TOON syntax (as opposed to
# ctoon_read_json(), which parse() above uses for JSON input) -- a real
# exported symbol, no shim needed, same as ctoon_read_json().

"""
    CToon.parse_toon(str::AbstractString) -> Any

Parse `str` as TOON (not JSON -- see `CToon.parse` for that) and return
it as native Julia data, same value shapes as `CToon.parse`.
"""
function parse_toon(str::AbstractString)
    buf = Vector{UInt8}(str)
    err = Ref(ReadErr(0, 0, C_NULL, 0))
    doc = Doc(@ccall libctoon_jl.ctoon_read_opts(
        buf::Ptr{UInt8}, sizeof(buf)::Csize_t, 0::Cuint, C_NULL::Ptr{Cvoid}, err::Ptr{ReadErr},
    )::Ptr{Cvoid})
    if doc.ptr == C_NULL
        e = err[]
        msg = e.msg == C_NULL ? "(no message)" : unsafe_string(e.msg)
        error("CToon: failed to parse TOON input: $msg (code=$(e.code), pos=$(e.pos))")
    end
    try
        root_ptr = @ccall libctoon_jl.ctoon_rs_doc_get_root(doc.ptr::Ptr{Cvoid})::Ptr{Cvoid}
        return _to_julia(Val(root_ptr))
    finally
        doc_free(doc)
    end
end

export dumps, to_json, parse_toon

end # module CToon
