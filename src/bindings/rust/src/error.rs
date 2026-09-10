use std::fmt;

/// Error type returned by every fallible function in this crate.
#[derive(Debug)]
pub enum Error {
    /// The input could not be parsed as TOON or JSON.
    Parse { message: String, pos: usize, code: u32 },
    /// A [`Value`](crate::Value) could not be serialised.
    Write { message: String, code: u32 },
    /// Building the ctoon value tree failed (e.g. an internal allocation
    /// failure) — not reachable in ordinary use.
    UnsupportedValue(String),
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Error::Parse { message, pos, code } => {
                write!(f, "ctoon: {} (pos {}, code {})", message, pos, code)
            }
            Error::Write { message, code } => write!(f, "ctoon: {} (code {})", message, code),
            Error::UnsupportedValue(msg) => write!(f, "ctoon: {}", msg),
        }
    }
}

impl std::error::Error for Error {}
