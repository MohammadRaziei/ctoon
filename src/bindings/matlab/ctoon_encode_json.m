function str = ctoon_encode_json(value)
%CTOON_ENCODE_JSON  Encode a MATLAB value to a JSON string via CToon.
%
%   STR = CTOON_ENCODE_JSON(VALUE)
%
%   Converts VALUE to a JSON character array using the CToon JSON writer.
%   Requires that the MEX was compiled with CTOON_ENABLE_JSON=1
%   (the default CMake build does this automatically).
%
%   Type mapping:
%     []            -> null
%     logical       -> true / false
%     double scalar -> number (floating-point)
%     int64  scalar -> number (integer)
%     uint64 scalar -> number (integer)
%     char / string -> "string"
%     cell array    -> [ ... ]
%     struct        -> { "key": value, ... }
%
%   Example:
%     s = ctoon_encode_json(struct('x', 1.0, 'y', -2.5, 'label', 'pt'));
%     % s ≈ '{"x":1,"y":-2.5,"label":"pt"}'
%
%     s = ctoon_encode_json({true, 42.0, 'hello'});
%     % s ≈ '[true,42,"hello"]'
%
%   Errors:
%     Throws ctoon:noJson    if JSON support was not compiled in.
%     Throws ctoon:encodeError on serialisation failure.
%
%   See also: CTOON_DECODE_JSON, CTOON_ENCODE, CTOON_DECODE.

str = ctoon_mex('encode_json', value);
end
