function buildfile(varargin)
%BUILDFILE  Build the CToon MEX gateway from within MATLAB.
%
%   BUILDFILE()
%   BUILDFILE('json', true|false)
%   BUILDFILE('outdir', '/path/to/dir')
%   BUILDFILE('verbose', true|false)
%
%   Compiles ctoon_mex.c (and the ctoon C library) into a MEX binary and
%   places it — together with the public .m wrappers — in the output
%   directory.  After buildfile() completes, a single addpath() call is
%   enough to use the binding.
%
%   Options (name-value pairs):
%     'json'     true  (default) — compile with CTOON_ENABLE_JSON=1
%                false           — omit JSON reader/writer
%     'outdir'   output directory for the MEX binary and .m files.
%                Default: same directory as this script.
%     'verbose'  true  — print the full mex command before running it
%                false (default) — silent
%
%   Examples:
%     % Build in-place (MEX ends up next to buildfile.m)
%     buildfile
%
%     % Build without JSON support
%     buildfile('json', false)
%
%     % Build into a specific directory
%     buildfile('outdir', fullfile(getenv('HOME'), 'matlab', 'ctoon'))
%
%   Requirements:
%     - A C compiler configured for MATLAB  (run |mex -setup C| once)
%     - MATLAB R2018a or newer
%
%   See also: mex, ctoon_encode, ctoon_decode, ctoon_read, ctoon_write,
%             ctoon_encode_json, ctoon_decode_json.

% -------------------------------------------------------------------------
% Parse options
% -------------------------------------------------------------------------
p = inputParser();
p.addParameter('json',    true,  @(x) islogical(x) && isscalar(x));
p.addParameter('outdir',  '',    @ischar);
p.addParameter('verbose', false, @(x) islogical(x) && isscalar(x));
p.parse(varargin{:});

opt_json    = p.Results.json;
opt_outdir  = p.Results.outdir;
opt_verbose = p.Results.verbose;

% -------------------------------------------------------------------------
% Locate repository root relative to this script
% -------------------------------------------------------------------------
here    = fileparts(mfilename('fullpath'));          % …/src/bindings/matlab
repoRoot = fullfile(here, '..', '..', '..');         % project root
repoRoot = GetFullPath(repoRoot);                    % resolve ..

srcDir  = fullfile(repoRoot, 'src');
incDir  = fullfile(repoRoot, 'include');

mexSrc  = fullfile(here,   'ctoon_mex.c');
libSrc  = fullfile(srcDir, 'ctoon.c');

% -------------------------------------------------------------------------
% Validate sources exist
% -------------------------------------------------------------------------
assert(exist(mexSrc, 'file') == 2, ...
    'buildfile: cannot find ctoon_mex.c at %s', mexSrc);
assert(exist(libSrc, 'file') == 2, ...
    'buildfile: cannot find ctoon.c at %s', libSrc);
assert(exist(incDir, 'dir')  == 7, ...
    'buildfile: include directory not found at %s', incDir);

% -------------------------------------------------------------------------
% Output directory
% -------------------------------------------------------------------------
if isempty(opt_outdir)
    opt_outdir = here;
end
if ~exist(opt_outdir, 'dir')
    mkdir(opt_outdir);
    fprintf('buildfile: created output directory: %s\n', opt_outdir);
end

% -------------------------------------------------------------------------
% Assemble mex() arguments
% -------------------------------------------------------------------------
args = { ...
    mexSrc, ...
    libSrc, ...
    ['-I', incDir], ...
    '-outdir', opt_outdir, ...
    '-output', 'ctoon_mex', ...
};

if opt_json
    if ispc()
        args{end+1} = 'COMPFLAGS=$COMPFLAGS /DCTOON_ENABLE_JSON=1';
    else
        args{end+1} = 'CFLAGS=$CFLAGS -DCTOON_ENABLE_JSON=1';
    end
end

% -------------------------------------------------------------------------
% Build
% -------------------------------------------------------------------------
if opt_verbose
    fprintf('buildfile: running mex with args:\n');
    for k = 1:numel(args)
        fprintf('  %s\n', args{k});
    end
end

fprintf('buildfile: compiling ctoon_mex ... ');
try
    mex(args{:});
catch e
    fprintf('FAILED\n');
    rethrow(e);
end
fprintf('OK\n');

% -------------------------------------------------------------------------
% Copy .m wrappers to outdir (skip if already there)
% -------------------------------------------------------------------------
wrappers = { ...
    'ctoon_encode.m', ...
    'ctoon_decode.m', ...
    'ctoon_read.m',   ...
    'ctoon_write.m',  ...
    'ctoon_encode_json.m', ...
    'ctoon_decode_json.m', ...
};

for k = 1:numel(wrappers)
    src = fullfile(here, wrappers{k});
    dst = fullfile(opt_outdir, wrappers{k});
    if ~strcmp(src, dst)
        copyfile(src, dst, 'f');
    end
end

% -------------------------------------------------------------------------
% Done
% -------------------------------------------------------------------------
fprintf('buildfile: done.\n');
fprintf('buildfile: add to path with:\n');
fprintf('    addpath(''%s'')\n', opt_outdir);
fprintf('buildfile: verify with:\n');
fprintf('    ctoon_encode(struct(''hello'', ''world''))\n');

end % buildfile


% =========================================================================
% Helper: resolve a path with .. components (no Java, no system calls)
% =========================================================================
function out = GetFullPath(p)
    % Split on separators, resolve '..', reassemble.
    if ispc()
        sep = '\';
    else
        sep = '/';
    end
    parts = strsplit(p, {'/','\'});
    out_parts = {};
    for k = 1:numel(parts)
        tok = parts{k};
        if strcmp(tok, '..')
            if ~isempty(out_parts)
                out_parts(end) = [];
            end
        elseif ~isempty(tok) && ~strcmp(tok, '.')
            out_parts{end+1} = tok; %#ok<AGROW>
        end
    end
    if ispc()
        out = strjoin(out_parts, sep);
    else
        out = ['/', strjoin(out_parts, sep)];
    end
end
