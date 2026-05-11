function value = ctoon_decode_json(str)
%CTOON_DECODE_JSON  Decode a JSON string into a MATLAB value via CToon.
%
%   VALUE = CTOON_DECODE_JSON(STR)
%
%   Parses the JSON character array STR using the CToon JSON reader and
%   returns the corresponding MATLAB value.
%   Requires that the MEX was compiled with CTOON_ENABLE_JSON=1
%   (the default CMake build does this automatically).
%
%   Type mapping:
%     null          -> []  (empty double)
%     true / false  -> logical scalar
%     integer       -> uint64 or int64 scalar
%     float         -> double scalar
%     "string"      -> char array
%     [ ... ]       -> cell array  (column, n×1)
%     { ... }       -> struct      (scalar)
%
%   Example:
%     v = ctoon_decode_json('{"name":"Alice","score":9.5,"pass":true}');
%     v.name   % → 'Alice'
%     v.score  % → 9.5
%     v.pass   % → true
%
%     v = ctoon_decode_json('[1,2,3]');
%     % → {uint64(1); uint64(2); uint64(3)}
%
%   Errors:
%     Throws ctoon:noJson      if JSON support was not compiled in.
%     Throws ctoon:decodeError when STR is not valid JSON.
%
%   See also: CTOON_ENCODE_JSON, CTOON_DECODE, CTOON_ENCODE.

if ~ischar(str) && ~isstring(str)
    error('ctoon:badArg', 'ctoon_decode_json: input must be a character array or string.');
end
value = ctoon_mex('decode_json', char(str));
end
