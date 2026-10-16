function str = json_to_toon(json, indent)
%JSON_TO_TOON  Convert JSON text directly to TOON text.
%
%   STR = JSON_TO_TOON(JSON)
%   STR = JSON_TO_TOON(JSON, INDENT)
%
%   One call into the C core: no MATLAB struct/cell is built in between
%   (contrast ctoon.encode(jsondecode(JSON)), which goes through jsondecode).
%   INDENT is spaces per level (default 2).
%
%   Example:
%     s = ctoon.json_to_toon('{"name":"Alice","age":30}');
%     % s = "name: Alice" + newline + "age: 30"
%
%   See also: ctoon.toon_to_json, ctoon.json_to_toon_file, ctoon.encode.

if nargin < 2
    str = ctoon_mex('json_to_toon', json);
else
    str = ctoon_mex('json_to_toon', json, indent);
end
end
