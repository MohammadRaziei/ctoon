function ctoon_build(buildDir, force)
%CTOON_BUILD  Compile MEX and export the CToon MATLAB package.
%
%   SYNTAX:
%     ctoon_build(buildDir)
%     ctoon_build(buildDir, force)
%
%   DESCRIPTION:
%     This script compiles the 'ctoon_mex.c' gateway (located in the root)
%     and bundles it with the MATLAB wrappers from the 'ctoon/' directory
%     into a functional '+ctoon' package inside the build directory.
%
%   INPUTS:
%     buildDir - (string) Target directory for the build artifacts.
%     force    - (logical) If true, forces re-compilation of the MEX binary.

% ---- 1. Setup Paths ------------------------------------------------------
here = fileparts(mfilename('fullpath'));

% Source folders
srcMDir = fullfile(here, 'ctoon'); % MATLAB wrappers folder
mexGateway = fullfile(here, 'ctoon_mex.c'); % MEX Gateway in root

% ---- 2. Resolve Arguments ------------------------------------------------
if nargin < 1 || isempty(buildDir)
    buildDir = fullfile(here, 'build');
end
if nargin < 2, force = false; end

% Standardize buildDir
if ~isfolder(buildDir), mkdir(buildDir); end
[~, info] = fileattrib(buildDir);
buildDir = info.Name;

% Define Destination Package Structure
isOctave      = exist('OCTAVE_VERSION', 'builtin') ~= 0;
dstPkgDir     = fullfile(buildDir, '+ctoon');
dstPrivateDir = fullfile(dstPkgDir, 'private');
mexFileName   = ['ctoon_mex.' mexext];
if isOctave
    % Octave does not resolve a private/ folder inside a +package folder,
    % so the gateway lives next to the package, in buildDir itself:
    %   <buildDir>/ctoon_mex.mex   <buildDir>/+ctoon/*.m
    % and addpath(buildDir) alone is enough, same as on MATLAB.
    mexOutDir = buildDir;
else
    % MATLAB: <buildDir>/+ctoon/private/ctoon_mex.<mexext>
    mexOutDir = dstPrivateDir;
end
targetMexPath = fullfile(mexOutDir, mexFileName);

% ---- 3. Export Package Structure (Source -> Build) -----------------------
fprintf('  [Build] Creating package structure in: %s\n', dstPkgDir);
if ~isfolder(mexOutDir), mkdir(mexOutDir); end
if ~isfolder(dstPkgDir), mkdir(dstPkgDir); end

% Copy .m files from ctoon/ to +ctoon/
if isfolder(srcMDir)
    mFiles = dir(fullfile(srcMDir, '*.m'));
    for i = 1:numel(mFiles)
        copyfile(fullfile(srcMDir, mFiles(i).name), dstPkgDir, 'f');
    end
else
    error('Build:SourceNotFound', 'MATLAB source folder "ctoon/" not found.');
end

% ---- 4. Smart Core Source Detection --------------------------------------
% Find core library ctoon.c / ctoon.h
% Check first if they are in the current root, otherwise look in project src
if isfile(fullfile(here, 'ctoon.c'))
    coreSrcDir = here;
    coreIncDir = here;
else
    coreSrcDir = fullfile(here, '..', '..', '..', 'src');
    coreIncDir = fullfile(here, '..', '..', '..', 'include');
end
ctoonCoreSrc = fullfile(coreSrcDir, 'ctoon.c');

% ---- 5. MEX Compilation --------------------------------------------------
if isfile(targetMexPath) && ~force
    fprintf('  [Build] %s already exists. Skipping compilation.\n', mexFileName);
else
    if ~isfile(mexGateway), error('Build:NotFound', 'MEX Gateway not found: %s', mexGateway); end
    if ~isfile(ctoonCoreSrc), error('Build:NotFound', 'Core source not found: %s', ctoonCoreSrc); end

    fprintf('  [Build] Compiling MEX binary into %s ...\n', mexOutDir);

    if isOctave
        % GNU Octave: its mex wrapper has no -outdir/-output, so call
        % mkoctfile directly (needs the octave-dev / liboctave-dev package;
        % only the build needs it -- the resulting binary does not).
        [st, ~] = system('mkoctfile --version');
        if st ~= 0
            error('Build:NoMkoctfile', ...
                  'mkoctfile not found. Install octave-dev (Debian/Ubuntu) or liboctave-dev.');
        end
        cmd = sprintf('mkoctfile --mex -I"%s" -o "%s" "%s" "%s"', ...
                      coreIncDir, fullfile(mexOutDir, 'ctoon_mex'), ...
                      mexGateway, ctoonCoreSrc);
        [st, out] = system(cmd);
        if st ~= 0
            error('Build:CompileFailed', 'mkoctfile failed:\n%s', out);
        end
    else
        mex(mexGateway, ctoonCoreSrc, ...
            ['-I', coreIncDir], ...
            '-outdir', mexOutDir, ...
            '-output', 'ctoon_mex');
    end

    fprintf('  [Build] Compilation successful.\n');
end

% ---- 6. Finalize ---------------------------------------------------------
if isOctave
    % Octave's built-in load() is shadowed by ctoon.load; silence the notice.
    warning('off', 'Octave:shadowed-function');
end
addpath(buildDir);
rehash;
fprintf('  [Build] Ready. You can now use "ctoon.version".\n');

end