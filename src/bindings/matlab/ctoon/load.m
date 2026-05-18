function value = load(fid)
%LOAD  Read TOON data from an open file and decode it (Python-style).
%
%   VALUE = LOAD(FID)
%
%   Reads all remaining content from the file identified by FID and
%   decodes it as a TOON-format document.  FID must be a valid file
%   identifier obtained from FOPEN; the caller is responsible for
%   opening and closing the file.
%
%   Example:
%     fid = fopen('data.toon', 'r');
%     v = ctoon.load(fid);
%     fclose(fid);
%
%   See also: ctoon.dump, ctoon.loads, ctoon.read.

if nargin < 1
    error('ctoon:badArg', 'ctoon.load: FID is required.');
end
if ~isnumeric(fid) || ~isscalar(fid) || fid < 1
    error('ctoon:badArg', 'ctoon.load: FID must be a valid file identifier from fopen.');
end

raw = fread(fid, Inf, '*char')';
if isempty(raw)
    error('ctoon:readError', 'ctoon.load: file is empty or nothing left to read.');
end
value = ctoon.decode(raw);
end
