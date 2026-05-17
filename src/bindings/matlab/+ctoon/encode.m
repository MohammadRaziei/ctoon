function str = ctoon.encode(value)
%ENCODE  Encode a MATLAB value to a TOON-format string.
%
%   STR = ENCODE(VALUE)
%
%   Serialises VALUE into the TOON binary-text format and returns the
%   result as a MATLAB character array.
%
%   Type mapping:
%     []            -> null
%     logical       -> bool
%     double scalar -> real
%     int64  scalar -> signed integer
%     uint64 scalar -> unsigned integer
%     char / string -> str
%     cell array    -> array
%     struct        -> object  (field names become keys)
%
%   Example:
%     s = ctoon.encode(struct('name','Alice','age',30,'active',true));
%     % s ≈ '{name:Alice,age:30,active:true}'
%
%     s = ctoon.encode({1.0, 'hello', false});
%     % s ≈ '[1,hello,false]'
%
%   See also: ctoon.decode, ctoon.write, ENCODE_JSON.

str = ctoon_mex('encode', value);
end
