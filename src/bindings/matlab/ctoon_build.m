%CTOON_BUILD  Compile the CToon MEX gateway.
%
%   ctoon_build()
%
%   Compiles ctoon_mex.c into a MEX binary and places it in BUILD_DIR.
%   Meant to be called from buildfile.m (mexTask) or directly from MATLAB.
%
%   Configurable variables — set in the base workspace or via -batch:
%
%     MEX_SOURCES_PATH   directory containing ctoon.c
%                        (default: same directory as this file)
%     MEX_INCLUDE_DIR    directory containing ctoon.h
%                        (default: same directory as this file)
%     BUILD_DIR          output directory for MEX binary and .m wrappers
%                        (default: same directory as this file)
%
%   CMake / CI usage:
%
%     matlab -sd src/bindings/matlab -batch \
%       "BUILD_DIR='<bindir>'; MEX_SOURCES_PATH='<src>'; \
%        MEX_INCLUDE_DIR='<inc>'; buildtool mex"
%
%   See also: ctoon_install, buildfile.

here = fileparts(mfilename('fullpath'));

% ---- resolve workspace variables ----------------------------------------
function val = ws_get(name, default)
    if evalin('base', ['exist(''' name ''', ''var'')'])
        val = strtrim(evalin('base', name));
        if isempty(val), val = default; end
    else
        val = default;
    end
end

mexSourcesPath = ws_get('MEX_SOURCES_PATH', here);
mexIncludeDir  = ws_get('MEX_INCLUDE_DIR',  here);
buildDir       = ws_get('BUILD_DIR',        here);

% ---- validate sources ---------------------------------------------------
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

% ---- create output dir if needed ----------------------------------------
if ~isfolder(buildDir)
    mkdir(buildDir);
end

% ---- compile ------------------------------------------------------------
fprintf('  MEX_SOURCES_PATH : %s\n', mexSourcesPath);
fprintf('  MEX_INCLUDE_DIR  : %s\n', mexIncludeDir);
fprintf('  BUILD_DIR        : %s\n', buildDir);
fprintf('  Compiling ctoon_mex ...\n');

mex(mexGateway, ctoonSrc, ...
    ['-I', mexIncludeDir], ...
    '-outdir', buildDir,   ...
    '-output', 'ctoon_mex');

% ---- copy wrappers when buildDir differs from here ----------------------
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
    fprintf('  Wrappers copied  → %s\n', buildDir);
end

fprintf('  Done.\n');
