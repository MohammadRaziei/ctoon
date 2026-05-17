function write(value, filepath)
%WRITE  Encode a MATLAB value and write it to a TOON file.
%
%   WRITE(VALUE, FILEPATH)
%
%   Serialises VALUE to TOON format (see ctoon.encode) and writes the
%   result to the file at FILEPATH, creating or overwriting it.
%
%   FILEPATH may be an absolute path or a path relative to the current
%   working directory.  The file is written as UTF-8 text.
%
%   Type mapping: same as ctoon.encode.
%
%   Example:
%     cfg.host    = 'localhost';
%     cfg.port    = uint64(8080);
%     cfg.debug   = false;
%     ctoon.write(cfg, 'config.toon');
%
%     % Round-trip check
%     v = ctoon.read('config.toon');
%     isequal(v.host, cfg.host)   % -> true
%
%   Errors:
%     Throws ctoon:writeError when the file cannot be written.
%
%   See also: ctoon.read, ctoon.encode, ctoon.decode.

if nargin ~= 2
    error('ctoon:badArg', 'ctoon.write: exactly 2 arguments required (value, filepath).');
end
if ~ischar(filepath) && ~isstring(filepath)
    error('ctoon:badArg', 'ctoon.write: filepath must be a character array or string.');
end
ctoon_mex('write', value, char(filepath));
end
