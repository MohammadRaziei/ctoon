function ctoon_build(buildDir, force)
%CTOON_BUILD  Compile MEX and prepare the +ctoon package.
%
%   ctoon_build()                % Build in-place
%   ctoon_build(buildDir)        % Build and export to buildDir
%   ctoon_build(buildDir, force) % Build with option to force recompile
%
%   Arguments:
%     buildDir   Directory to export the package. If provided, the +ctoon 
%                folder (without .c files) will be copied there.
%     force      Logical. true = force rebuild. Default: false.

% ---- Setup Source Paths (Development Root) -------------------------------
here = fileparts(mfilename('fullpath'));
srcPkgDir = fullfile(here, '+ctoon');
srcPrivateDir = fullfile(srcPkgDir, 'private');

% ---- Resolve Arguments ---------------------------------------------------
if nargin < 1, buildDir = []; end
if nargin < 2, force = false; end

% ---- Smart Core Source Detection -----------------------------------------
if isfile(fullfile(srcPrivateDir, 'ctoon.c'))
    mexSourcesPath = srcPrivateDir;
    mexIncludeDir  = srcPrivateDir;
else
    mexSourcesPath = fullfile(here, '..', '..', '..', 'src');
    mexIncludeDir  = fullfile(here, '..', '..', '..', 'include');
end

% ---- Compilation (Always performed in the source +ctoon/private) ---------
mexBinaryName = 'ctoon_mex';
mexFileName = [mexBinaryName '.' mexext];
targetMexPath = fullfile(srcPrivateDir, mexFileName);

if isfile(targetMexPath) && ~force
    fprintf('  [Build] %s already exists. Skipping.\n', mexFileName);
else
    mexGateway = fullfile(srcPrivateDir, 'ctoon_mex.c');
    ctoonSrc   = fullfile(mexSourcesPath, 'ctoon.c');
    
    fprintf('  [Build] Compiling MEX...\n');
    if ~isfolder(srcPrivateDir), mkdir(srcPrivateDir); end
    
    mex(mexGateway, ctoonSrc, ...
        ['-I', mexIncludeDir], ...
        '-outdir', srcPrivateDir, ...
        '-output', mexBinaryName);
    fprintf('  [Build] MEX compiled successfully.\n');
end

% ---- Export Logic (If buildDir is provided and different) ----------------
if ~isempty(buildDir)
    absBuildDir = uint8(0); % Dummy for conversion
    % Resolve to absolute path
    if ~isfolder(buildDir), mkdir(buildDir); end
    currentDir = pwd;
    cd(buildDir); buildDir = pwd; cd(currentDir);
    
    % Only export if buildDir is not the current source directory
    if ~strcmpi(here, buildDir)
        fprintf('  [Export] Preparing package in: %s\n', buildDir);
        
        dstPkgDir = fullfile(buildDir, '+ctoon');
        dstPrivateDir = fullfile(dstPkgDir, 'private');
        
        if ~isfolder(dstPrivateDir), mkdir(dstPrivateDir); end
        
        % 1. Copy all .m files from +ctoon
        mFiles = dir(fullfile(srcPkgDir, '*.m'));
        for i = 1:numel(mFiles)
            copyfile(fullfile(srcPkgDir, mFiles(i).name), dstPkgDir, 'f');
        end
        
        % 2. Copy the compiled MEX from private (Skip .c files)
        if isfile(targetMexPath)
            copyfile(targetMexPath, dstPrivateDir, 'f');
        end
        
        % 3. Copy any .m files in private (if any exist)
        privateMFiles = dir(fullfile(srcPrivateDir, '*.m'));
        for i = 1:numel(privateMFiles)
            copyfile(fullfile(srcPrivateDir, privateMFiles(i).name), dstPrivateDir, 'f');
        end
        
        fprintf('  [Export] Done. (Sources excluded)\n');
    end
end

fprintf('  Ready.\n');
end