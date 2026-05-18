function value = loads(str)
%LOADS  Decode a TOON-format string into a MATLAB value (alias for DECODE).
%
%   VALUE = LOADS(STR)
%
%   Parses the TOON character array STR and returns the corresponding
%   MATLAB representation.  This is a Python-style alias for ctoon.decode.
%
%   Example:
%     v = ctoon.loads('{x:1.5}');
%
%   See also: ctoon.decode, ctoon.dumps, ctoon.load.

value = ctoon.decode(str);
end
