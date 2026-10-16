function toon_to_json_file(toonPath, jsonPath, indent)
%TOON_TO_JSON_FILE  Convert a TOON file to a JSON file.
%
%   TOON_TO_JSON_FILE(TOONPATH, JSONPATH)
%   TOON_TO_JSON_FILE(TOONPATH, JSONPATH, INDENT)
%
%   The output file is only created if the conversion succeeds.
%
%   See also: ctoon.json_to_toon_file, ctoon.toon_to_json.

if nargin < 3
    ctoon_mex('toon_to_json_file', toonPath, jsonPath);
else
    ctoon_mex('toon_to_json_file', toonPath, jsonPath, indent);
end
end
