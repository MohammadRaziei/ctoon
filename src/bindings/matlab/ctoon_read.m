function value = ctoon_read(filepath)
%CTOON_READ  Read and decode a TOON file into a MATLAB value.
%
%   VALUE = CTOON_READ(FILEPATH)
%
%   Opens the file at FILEPATH, parses its TOON content, and returns the
%   decoded MATLAB value.  The file must be UTF-8 encoded.
%
%   FILEPATH may be an absolute path or a path relative to the current
%   working directory.
%
%   Type mapping: same as CTOON_DECODE.
%
%   Example:
%     % Read a TOON config file
%     cfg = ctoon_read('config.toon');
%     cfg.host   % -> 'localhost'
%     cfg.port   % -> uint64(8080)
%
%     % Read from an absolute path
%     data = ctoon_read('/data/records.toon');
%
%   Errors:
%     Throws ctoon:readError when the file cannot be opened or parsed.
%
%   See also: CTOON_WRITE, CTOON_DECODE, CTOON_ENCODE.

if ~ischar(filepath) && ~isstring(filepath)
    error('ctoon:badArg', 'ctoon_read: filepath must be a character array or string.');
end
value = ctoon_mex('read', char(filepath));
end
