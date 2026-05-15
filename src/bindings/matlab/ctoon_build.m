%CTOON_BUILD  Compile the CToon MEX gateway.
%
%   ctoon_build()
%
%   Compiles ctoon_mex.c into a MEX binary and places it in BUILD_DIR.
%   Meant to be called from buildfile.m (mexTask) or directly from MATLAB.
%
%   Configurable via global variables (set before calling):
%
%     global MEX_SOURCES_PATH   directory containing ctoon.c
%                               (default: <repo>/src)
%     global MEX_INCLUDE_DIR    directory containing ctoon.h
%                               (default: <repo>/include)
%     global BUILD_DIR          output directory for MEX binary and .m wrappers
%                               (default: same directory as this file)
%
%   CMake / CI usage:
%
%     matlab -sd src/bindings/matlab -batch \
%       "global BUILD_DIR MEX_SOURCES_PATH MEX_INCLUDE_DIR; \
%        BUILD_DIR='<bindir>'; MEX_SOURCES_PATH='<src>'; \
%        MEX_INCLUDE_DIR='<inc>'; buildtool mex"
%
%   Interactive usage (no variables needed - paths resolved automatically):
%
%     buildtool mex
%
%   See also: ctoon_install, buildfile.

global MEX_SOURCES_PATH MEX_INCLUDE_DIR BUILD_DIR

here     = fileparts(mfilename('fullpath'));
repoRoot = fullfile(here, '..', '..', '..');

% ---- resolve paths -------------------------------------------------------
if isempty(MEX_SOURCES_PATH)
    mexSourcesPath = fullfile(repoRoot, 'src');
else
    mexSourcesPath = strtrim(MEX_SOURCES_PATH);
end

if isempty(MEX_INCLUDE_DIR)
    mexIncludeDir = fullfile(repoRoot, 'include');
else
    mexIncludeDir = strtrim(MEX_INCLUDE_DIR);
end

if isempty(BUILD_DIR)
    buildDir = here;
else
    buildDir = strtrim(BUILD_DIR);
end

% ---- validate sources ----------------------------------------------------
mexGateway = fullfile(here, 'ctoon_mex.c');
ctoonSrc   = fullfile(mexSourcesPath, 'ctoon.c');

if ~isfile(mexGateway)
    error('ctoon_build:notFound', 'ctoon_mex.c not found: %s', mexGateway);
end
if ~isfile(ctoonSrc)
    error('ctoon_build:notFound', 'ctoon.c not found: %s', ctoonSrc);
end
if ~isfolder(mexIncludeDir)
    error('ctoon_build:notFound', 'include dir not found: %s', mexIncludeDir);
end

% ---- create output dir if needed -----------------------------------------
if ~isfolder(buildDir)
    mkdir(buildDir);
end

% ---- compile -------------------------------------------------------------
fprintf('  MEX_SOURCES_PATH : %s\n', mexSourcesPath);
fprintf('  MEX_INCLUDE_DIR  : %s\n', mexIncludeDir);
fprintf('  BUILD_DIR        : %s\n', buildDir);
fprintf('  Compiling ctoon_mex ...\n');

mex(mexGateway, ctoonSrc, ...
    ['-I', mexIncludeDir], ...
    '-outdir', buildDir,   ...
    '-output', 'ctoon_mex');

% ---- copy wrappers when buildDir differs from here -----------------------
if ~strcmp(here, buildDir)
    wrappers = { ...
        'ctoon_encode.m',  ...
        'ctoon_decode.m',  ...
        'ctoon_read.m',    ...
        'ctoon_write.m',   ...
        'ctoon_build.m',   ...
        'ctoon_install.m'  ...
    };
    for k = 1:numel(wrappers)
        src = fullfile(here, wrappers{k});
        if isfile(src)
            copyfile(src, fullfile(buildDir, wrappers{k}), 'f');
        end
    end
    fprintf('  Wrappers copied  -> %s\n', buildDir);
end

fprintf('  Done.\n');
