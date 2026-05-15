function ctoon_write(value, filepath)
%CTOON_WRITE  Encode a MATLAB value and write it to a TOON file.
%
%   CTOON_WRITE(VALUE, FILEPATH)
%
%   Serialises VALUE to TOON format (see CTOON_ENCODE) and writes the
%   result to the file at FILEPATH, creating or overwriting it.
%
%   FILEPATH may be an absolute path or a path relative to the current
%   working directory.  The file is written as UTF-8 text.
%
%   Type mapping: same as CTOON_ENCODE.
%
%   Example:
%     cfg.host    = 'localhost';
%     cfg.port    = uint64(8080);
%     cfg.debug   = false;
%     ctoon_write(cfg, 'config.toon');
%
%     % Round-trip check
%     v = ctoon_read('config.toon');
%     isequal(v.host, cfg.host)   % -> true
%
%   Errors:
%     Throws ctoon:writeError when the file cannot be written.
%
%   See also: CTOON_READ, CTOON_ENCODE, CTOON_DECODE.

if nargin ~= 2
    error('ctoon:badArg', 'ctoon_write: exactly 2 arguments required (value, filepath).');
end
if ~ischar(filepath) && ~isstring(filepath)
    error('ctoon:badArg', 'ctoon_write: filepath must be a character array or string.');
end
ctoon_mex('write', value, char(filepath));
end
