function ctoon_clean(buildDir)
%CTOON_CLEAN  Remove build artifacts (package and MEX binaries).
%
%   SYNTAX:
%     ctoon_clean()           % Cleans the current directory (+ctoon and MEX)
%     ctoon_clean(buildDir)   % Cleans a specific build directory
%
%   DESCRIPTION:
%     - Clears MEX functions from memory.
%     - Removes the target directory from the MATLAB Path.
%     - Deletes the '+ctoon' package and MEX binaries.
%     - Does NOT modify configuration files (.buildtool/).

    here = fileparts(mfilename('fullpath'));

    % 1. Resolve Target (Defaults to 'here')
    if nargin < 1 || isempty(buildDir)
        buildDir = here;
    end

    fprintf('  [Clean] Cleaning CToon artifacts...\n');

    % 2. Safety: Clear MEX binaries from memory
    clear('mex');

    % 3. Path Management: Remove from MATLAB Path
    p = split(path, pathsep);
    if any(strcmpi(buildDir, p))
        rmpath(buildDir);
        savepath;
        fprintf('  [Clean] Removed from Path: %s\n', buildDir);
    end

    % 4. Artifact Removal
    if ~strcmpi(buildDir, here)
        % Case A: Separate build folder -> Delete the whole folder
        if isfolder(buildDir)
            rmdir(buildDir, 's');
            fprintf('  [Clean] Deleted directory: %s\n', buildDir);
        end
    else
        % Case B: Built in root -> Delete only generated files
        generatedPkg = fullfile(here, '+ctoon');
        if isfolder(generatedPkg)
            rmdir(generatedPkg, 's');
            fprintf('  [Clean] Deleted package: %s\n', generatedPkg);
        end
    end
    fprintf('  [Clean] Artifacts cleared.\n');
end
