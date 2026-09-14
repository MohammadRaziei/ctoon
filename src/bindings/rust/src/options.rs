//! Write options — the safe wrapper around C's `ctoon_write_options`
//! struct (`flag`, `delimiter`, `indent`; see ctoon.h), used by
//! [`crate::dumps_opts`] and [`crate::dumps_json_opts`].

use crate::ffi;

/// Array value delimiter used when encoding arrays of primitives — TOON
/// output only, has no effect on JSON output. Mirrors C's
/// `ctoon_delimiter` enum.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum Delimiter {
    /// `,` — the default.
    #[default]
    Comma,
    /// `\t`
    Tab,
    /// `|`
    Pipe,
}

impl Delimiter {
    pub(crate) fn to_ffi(self) -> ffi::ctoon_delimiter {
        match self {
            Delimiter::Comma => ffi::ctoon_delimiter::CTOON_DELIMITER_COMMA,
            Delimiter::Tab => ffi::ctoon_delimiter::CTOON_DELIMITER_TAB,
            Delimiter::Pipe => ffi::ctoon_delimiter::CTOON_DELIMITER_PIPE,
        }
    }
}

/// `CTOON_WRITE_*` bitflags from ctoon.h — OR these together for
/// [`WriteOptions::flags`]. Most apply to both TOON and JSON output; see
/// each constant's doc comment for TOON-only exceptions.
pub mod flags {
    /// No flags set (the default).
    pub const NOFLAG: u32 = 0;
    /// Escape non-ASCII characters as `\uXXXX`.
    pub const ESCAPE_UNICODE: u32 = 1 << 1;
    /// Escape `/` as `\/`.
    pub const ESCAPE_SLASHES: u32 = 1 << 2;
    /// Allow `Infinity`/`NaN` in the output instead of erroring.
    pub const ALLOW_INF_AND_NAN: u32 = 1 << 3;
    /// Write `Infinity`/`NaN` as `null` instead of erroring.
    pub const INF_AND_NAN_AS_NULL: u32 = 1 << 4;
    /// Allow invalid Unicode instead of erroring.
    pub const ALLOW_INVALID_UNICODE: u32 = 1 << 5;
    /// TOON-only: prefix arrays with a `[N]` length marker.
    pub const LENGTH_MARKER: u32 = 1 << 6;
    /// TOON-only: end the output with a trailing newline.
    pub const NEWLINE_AT_END: u32 = 1 << 7;
}

/// Options for [`crate::dumps_opts`]/[`crate::dumps_json_opts`]. Defaults
/// match what [`crate::dumps`]/[`crate::dumps_json`] use internally:
/// indent 2, comma delimiter, no extra flags.
#[derive(Debug, Clone, Copy)]
pub struct WriteOptions {
    /// Spaces per indent level. `0` minifies (TOON) / compacts (JSON).
    pub indent: i32,
    /// Array value delimiter — TOON output only.
    pub delimiter: Delimiter,
    /// `CTOON_WRITE_*` flags, OR'd together — see the [`flags`] module.
    pub flags: u32,
}

impl Default for WriteOptions {
    fn default() -> Self {
        Self {
            indent: 2,
            delimiter: Delimiter::default(),
            flags: flags::NOFLAG,
        }
    }
}

impl WriteOptions {
    pub(crate) fn to_ffi(self) -> ffi::ctoon_write_options {
        ffi::ctoon_write_options {
            flag: self.flags,
            delimiter: self.delimiter.to_ffi(),
            indent: self.indent,
        }
    }
}
