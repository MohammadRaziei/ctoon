function str = dumps(value)
%DUMPS  Encode a MATLAB value to a TOON-format string (alias for ENCODE).
%
%   STR = DUMPS(VALUE)
%
%   Serialises VALUE into the TOON format and returns the result as a
%   MATLAB character array.  This is a Python-style alias for ctoon.encode.
%
%   Example:
%     s = ctoon.dumps(struct('x', 1.5));
%
%   See also: ctoon.encode, ctoon.loads, ctoon.dump.

str = ctoon.encode(value);
end
