function str = toon_to_json(toon, indent)
%TOON_TO_JSON  Convert TOON text directly to JSON text.
%
%   STR = TOON_TO_JSON(TOON)
%   STR = TOON_TO_JSON(TOON, INDENT)
%
%   One call into the C core: no MATLAB struct/cell is built in between
%   (contrast jsonencode(ctoon.decode(TOON))). INDENT is spaces per level
%   (default 0 = compact).
%
%   Example:
%     j = ctoon.toon_to_json(sprintf('a: 1\nb: 2'));   % '{"a":1,"b":2}'
%
%   See also: ctoon.json_to_toon, ctoon.toon_to_json_file, ctoon.decode.

if nargin < 2
    str = ctoon_mex('toon_to_json', toon);
else
    str = ctoon_mex('toon_to_json', toon, indent);
end
end
