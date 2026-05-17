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
dstPkgDir     = fullfile(buildDir, '+ctoon');
dstPrivateDir = fullfile(dstPkgDir, 'private');
mexFileName   = ['ctoon_mex.' mexext];
targetMexPath = fullfile(dstPrivateDir, mexFileName);

% ---- 3. Export Package Structure (Source -> Build) -----------------------
fprintf('  [Build] Creating package structure in: %s\n', dstPkgDir);
if ~isfolder(dstPrivateDir), mkdir(dstPrivateDir); end

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

    fprintf('  [Build] Compiling MEX binary directly to private/...\n');
    
    % Compile directly into build/+ctoon/private/
    mex(mexGateway, ctoonCoreSrc, ...
        ['-I', coreIncDir], ...
        '-outdir', dstPrivateDir, ...
        '-output', 'ctoon_mex');
        
    fprintf('  [Build] Compilation successful.\n');
end

% ---- 6. Finalize ---------------------------------------------------------
addpath(buildDir);
rehash;
fprintf('  [Build] Ready. You can now use "ctoon.version".\n');

end