module CToon

# Loaded by build.jl into a shared lib containing ctoon.c + shim.c (the
# same ctoon_rs_* exported wrappers the Rust and Zig bindings use for
# ctoon's `static inline` API — see ../shim.c for why those wrappers
# exist). This is a skeleton: read-only, covering the most common
# ctoon_type cases. Mutable-document building (the ctoon_mut_* side that
# shim.c also exports) is left for a follow-up once this shape is
# confirmed to be the right one.

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
data: `Dict{String,Any}` for objects, `Vector{Any}` for arrays, `String`,
`Int64`/`Float64`, `Bool`, or `nothing`.
"""
function parse(str::AbstractString)
    # ctoon_read() itself is `ctoon_api_inline` (header-only, no real
    # symbol in the compiled lib — see shim.c's docstring for why that
    # matters for FFI) so, like the Rust and Zig bindings, call the real
    # exported ctoon_read_opts() it forwards to instead. It wants a
    # mutable buffer; NULL for the allocator (4th arg) is fine (default
    # allocator), but pass a real ctoon_read_err (5th arg) so a failure
    # reports why, instead of just NULL.
    buf = Vector{UInt8}(str)
    err = Ref(ReadErr(0, 0, C_NULL, 0))
    doc = Doc(@ccall libctoon_jl.ctoon_read_opts(
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
        out = Dict{String,Any}()
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

end # module CToon
