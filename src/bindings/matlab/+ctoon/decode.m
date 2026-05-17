function value = ctoon.decode(str)
%DECODE  Decode a TOON-format string into a MATLAB value.
%
%   VALUE = DECODE(STR)
%
%   Parses the TOON character array STR and returns the corresponding
%   MATLAB representation.
%
%   Type mapping:
%     null          -> []  (empty double)
%     bool          -> logical scalar
%     unsigned int  -> uint64 scalar
%     signed int    -> int64  scalar
%     real          -> double scalar
%     str           -> char array
%     array         -> cell array  (column, n×1)
%     object        -> struct      (scalar)
%
%   Example:
%     v = ctoon.decode('{name:Alice,age:30,active:true}');
%     v.name    % -> 'Alice'
%     v.age     % -> uint64(30)
%     v.active  % -> true
%
%     v = ctoon.decode('[1,2,3]');
%     % -> {uint64(1); uint64(2); uint64(3)}
%
%   Errors:
%     Throws ctoon:decodeError when STR is not valid TOON.
%
%   See also: ctoon.encode, ctoon.read, DECODE_JSON.

if ~ischar(str) && ~isstring(str)
    error('ctoon:badArg', 'ctoon.decode: input must be a character array or string.');
end
value = ctoon_mex('decode', char(str));
end
