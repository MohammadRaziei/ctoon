function ctoon_install(buildDir, force, verify)
%CTOON_INSTALL  Build and permanently install the CToon MEX binding.
%
%   ctoon_install()
%   ctoon_install(buildDir)
%   ctoon_install(buildDir, force)
%   ctoon_install(buildDir, force, verify)
%
%   Compiles ctoon_mex into buildDir (via ctoon_build) and permanently
%   adds that directory to the MATLAB search path (savepath).
%
%   Arguments:
%     buildDir   directory for the MEX binary and .m wrappers.
%                If omitted, ctoon_build decides the default.
%     force      logical. true = recompile even if MEX already exists.
%                If omitted, ctoon_build decides the default.
%     verify     logical. true = quick smoke test after install.
%                Default: true.
%
%   Examples:
%     ctoon_install                           % all defaults
%     ctoon_install('/opt/matlab/ctoon')      % custom dir
%     ctoon_install([], true)                 % force rebuild
%     ctoon_install([], [], false)            % skip verify
%
%   See also: ctoon_build, addpath, savepath.

% ---- resolve arguments ---------------------------------------------------
if nargin < 1, buildDir = []; end
if nargin < 2, force    = []; end
if nargin < 3 || isempty(verify)
    verify = true;
end

% ---- build ---------------------------------------------------------------
ctoon_build(buildDir, force);

% ---- resolve actual buildDir for addpath ---------------------------------
% Re-use the same resolution logic as ctoon_build
if ~isempty(buildDir)
    resolvedDir = strtrim(buildDir);
else
    global BUILD_DIR
    if ~isempty(BUILD_DIR)
        resolvedDir = strtrim(BUILD_DIR);
    else
        resolvedDir = fileparts(mfilename('fullpath'));
    end
end

fprintf('CToon install -> %s\n', resolvedDir);

% ---- add to permanent path -----------------------------------------------
if ~any(strcmp(strsplit(path, pathsep), resolvedDir))
    addpath(resolvedDir);
end
savepath();
fprintf('CToon added to MATLAB path permanently.\n');

% ---- verify --------------------------------------------------------------
if verify
    try
        v = ctoon_decode(ctoon_encode(struct('x', 1.5)));
        assert(isstruct(v) && abs(double(v.x) - 1.5) < 1e-10);
        fprintf('CToon verification passed.\n');
    catch e
        warning('ctoon_install:verifyFailed', 'Verification failed: %s', e.message);
    end
end
end