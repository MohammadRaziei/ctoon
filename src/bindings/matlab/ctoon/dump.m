function dump(value, fid)
%DUMP  Encode a MATLAB value and write it to an open file (Python-style).
%
%   DUMP(VALUE, FID)
%
%   Serialises VALUE to TOON format and writes the result to the file
%   identified by FID.  FID must be a valid file identifier obtained from
%   FOPEN; the caller is responsible for opening and closing the file.
%
%   Example:
%     fid = fopen('data.toon', 'w');
%     ctoon.dump(struct('x', 1.5), fid);
%     fclose(fid);
%
%   See also: ctoon.load, ctoon.dumps, ctoon.write.

if nargin < 2
    error('ctoon:badArg', 'ctoon.dump: both VALUE and FID are required.');
end
if ~isnumeric(fid) || ~isscalar(fid) || fid < 1
    error('ctoon:badArg', 'ctoon.dump: FID must be a valid file identifier from fopen.');
end

str = ctoon.encode(value);
fwrite(fid, str, 'char');
end
