function [verStr, info] = version()
%VERSION  Display or return CToon library version information.
%
%   v = ctoon.version()          % Returns version string (e.g., "1.0.0")
%   [v, info] = ctoon.version()  % Returns version string and metadata struct
%   ctoon.version()              % Prints "About" information to the console
%
%   This function detects if MATLAB is running in Desktop (GUI) or 
%   Terminal (CLI) mode to provide optimized formatting.
%
%   See also: ctoon.encode, ctoon.decode, ctoon.read, ctoon.write.

    % --- Metadata ---
    vInfo.Name        = 'CToon for MATLAB';
    vInfo.Description = 'High-performance C-based TOON Serialiser/Deserialiser';
    vInfo.Author      = 'Mohammad Raziei';
    vInfo.URL         = 'https://github.com/mohammadraziei/ctoon';
    
    % --- Get version string from MEX gateway ---
    try
        vInfo.Version = ctoon_mex('version');
    catch
        vInfo.Version = '__unknown__ (MEX not built)';
    end

    if nargout == 0
        % --- Console Output Logic ---
        isGUI = usejava('desktop');
        
        if isGUI
            % Rich text formatting for MATLAB Desktop
            fprintf('\n  <strong>%s</strong> (v%s)\n', vInfo.Name, vInfo.Version);
            fprintf('  %s\n', vInfo.Description);
            fprintf('  Author: %s\n', vInfo.Author);
            fprintf('  URL:    <a href="matlab:web(''%s'')">%s</a>\n\n', vInfo.URL, vInfo.URL);
        else
            % Plain text formatting for Terminal / CLI / Batch mode
            fprintf('\n  %s (v%s)\n', vInfo.Name, vInfo.Version);
            fprintf('  %s\n', vInfo.Description);
            fprintf('  Author: %s\n', vInfo.Author);
            fprintf('  URL:    %s\n\n', vInfo.URL);
        end
    else
        % --- Return Outputs ---
        verStr = vInfo.Version;
        if nargout > 1
            info = vInfo;
        end
    end
end