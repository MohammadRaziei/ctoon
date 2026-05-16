function ctoon_build(buildDir, force)
%CTOON_BUILD  Compile the CToon MEX gateway.
%
%   ctoon_build()
%   ctoon_build(buildDir)
%   ctoon_build(buildDir, force)
%
%   Compiles ctoon_mex.c into a MEX binary and places it in buildDir.
%   If the MEX binary already exists and force is false, compilation is
%   skipped and a message is printed.
%
%   Arguments:
%     buildDir   output directory for MEX binary and .m wrappers.
%                If omitted, uses global BUILD_DIR; if that is also
%                empty, defaults to the directory of this file.
%     force      logical scalar. true = always recompile even if the MEX
%                binary already exists. Default: false.
%
%   Examples:
%     ctoon_build                    % skip if already built
%     ctoon_build('/tmp/ctoon')      % skip if already built, custom dir
%     ctoon_build([], true)          % force rebuild, default dir
%     ctoon_build('/tmp/ctoon', true)% force rebuild, custom dir
%
%   See also: ctoon_install, buildfile.

here = fileparts(mfilename('fullpath'));

% ---- source paths (patched by CToonMatlabExport.cmake on export) ---------
mexSourcesPath = fullfile(here, '..', '..', '..', 'src');
mexIncludeDir  = fullfile(here, '..', '..', '..', 'include');

% ---- resolve buildDir ----------------------------------------------------
if nargin >= 1 && ~isempty(buildDir)
    buildDir = strtrim(buildDir);
else
    global BUILD_DIR
    if ~isempty(BUILD_DIR)
        buildDir = strtrim(BUILD_DIR);
    else
        buildDir = here;
    end
end

% ---- resolve force -------------------------------------------------------
if nargin < 2 || isempty(force)
    force = false;
end

% ---- compile if needed ---------------------------------------------------
mexBinary = fullfile(buildDir, ['ctoon_mex.' mexext]);
if isfile(mexBinary) && ~force
    fprintf('  ctoon_mex already built: %s\n', mexBinary);
    fprintf('  Skipping compilation (pass force=true to recompile).\n');
else
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
    if ~isfolder(buildDir)
        mkdir(buildDir);
    end

    fprintf('  MEX_SOURCES_PATH : %s\n', mexSourcesPath);
    fprintf('  MEX_INCLUDE_DIR  : %s\n', mexIncludeDir);
    fprintf('  BUILD_DIR        : %s\n', buildDir);
    fprintf('  Compiling ctoon_mex ...\n');

    mex(mexGateway, ctoonSrc, ...
        ['-I', mexIncludeDir], ...
        '-outdir', buildDir,   ...
        '-output', 'ctoon_mex');
end

% ---- copy wrappers (skip existing) when buildDir differs from here -------
if ~strcmp(here, buildDir)
    wrappers = { ...
        'ctoon_encode.m',  ...
        'ctoon_decode.m',  ...
        'ctoon_read.m',    ...
        'ctoon_write.m',   ...
    };
    for k = 1:numel(wrappers)
        src = fullfile(here, wrappers{k});
        dst = fullfile(buildDir, wrappers{k});
        if isfile(src) && ~isfile(dst)
            copyfile(src, dst, 'f');
        end
    end
end

fprintf('  Done.\n');
end