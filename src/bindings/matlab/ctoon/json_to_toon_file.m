function json_to_toon_file(jsonPath, toonPath, indent)
%JSON_TO_TOON_FILE  Convert a JSON file to a TOON file.
%
%   JSON_TO_TOON_FILE(JSONPATH, TOONPATH)
%   JSON_TO_TOON_FILE(JSONPATH, TOONPATH, INDENT)
%
%   The output file is only created if the conversion succeeds.
%
%   See also: ctoon.toon_to_json_file, ctoon.json_to_toon.

if nargin < 3
    ctoon_mex('json_to_toon_file', jsonPath, toonPath);
else
    ctoon_mex('json_to_toon_file', jsonPath, toonPath, indent);
end
end
