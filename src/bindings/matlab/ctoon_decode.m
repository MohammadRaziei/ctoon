function value = ctoon_decode(str)
%CTOON_DECODE  Decode a TOON-format string into a MATLAB value.
%
%   VALUE = CTOON_DECODE(STR)
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
%     v = ctoon_decode('{name:Alice,age:30,active:true}');
%     v.name    % → 'Alice'
%     v.age     % → uint64(30)
%     v.active  % → true
%
%     v = ctoon_decode('[1,2,3]');
%     % → {uint64(1); uint64(2); uint64(3)}
%
%   Errors:
%     Throws ctoon:decodeError when STR is not valid TOON.
%
%   See also: CTOON_ENCODE, CTOON_READ.

if ~ischar(str) && ~isstring(str)
    error('ctoon:badArg', 'ctoon_decode: input must be a character array or string.');
end
value = ctoon_mex('decode', char(str));
end
