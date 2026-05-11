function str = ctoon_encode(value)
%CTOON_ENCODE  Encode a MATLAB value to a TOON-format string.
%
%   STR = CTOON_ENCODE(VALUE)
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
%     s = ctoon_encode(struct('name','Alice','age',30,'active',true));
%     % s ≈ '{name:Alice,age:30,active:true}'
%
%     s = ctoon_encode({1.0, 'hello', false});
%     % s ≈ '[1,hello,false]'
%
%   See also: CTOON_DECODE, CTOON_WRITE.

str = ctoon_mex('encode', value);
end
