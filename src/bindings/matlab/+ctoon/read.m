function value = ctoon.read(filepath)
%READ  Read and decode a TOON file into a MATLAB value.
%
%   VALUE = READ(FILEPATH)
%
%   Opens the file at FILEPATH, parses its TOON content, and returns the
%   decoded MATLAB value.  The file must be UTF-8 encoded.
%
%   FILEPATH may be an absolute path or a path relative to the current
%   working directory.
%
%   Type mapping: same as ctoon.decode.
%
%   Example:
%     % Read a TOON config file
%     cfg = ctoon.read('config.toon');
%     cfg.host   % -> 'localhost'
%     cfg.port   % -> uint64(8080)
%
%     % Read from an absolute path
%     data = ctoon.read('/data/records.toon');
%
%   Errors:
%     Throws ctoon:readError when the file cannot be opened or parsed.
%
%   See also: ctoon.write, ctoon.decode, ctoon.encode.

if ~ischar(filepath) && ~isstring(filepath)
    error('ctoon:badArg', 'ctoon.read: filepath must be a character array or string.');
end
value = ctoon_mex('read', char(filepath));
end
