function plan = buildfile
%BUILDFILE  MATLAB Build Tool plan for the CToon project.
%
%   This file defines automated tasks for compiling, testing, and 
%   configuring the CToon MATLAB bindings. 
%
%   USAGE:
%     buildtool <task>              % Run a specific task
%     buildtool                     % Run the default task (build)
%     buildtool config              % View/Update project configurations
%
%   TASKS:
%     build      - Compiles MEX and prepares the +ctoon package.
%     test       - Executes the unit test suite.
%     coverage   - Runs tests with a dark-themed HTML and LCOV report.
%     install    - Builds and adds the package to the permanent MATLAB Path.
%     clean      - Deletes build artifacts and removes directory from Path.
%     config     - Manages persistent settings in .buildtool/config.ini.

here = fileparts(mfilename('fullpath'));

% ---- Configuration & Plan Initialization --------------------------------
plan = buildplan(localfunctions);

% Resolve Directories
repoRoot = fullfile(here, '..', '..', '..');
testDir  = fullfile(repoRoot, 'tests', 'matlab');

[buildDir, ~] = get_config_val('build_dir', here);

% ---- Task Dependencies --------------------------------------------------

% 1. Test Task (requires tests folder)
if isfolder(testDir) && ~isempty(dir(fullfile(testDir, 'test_*.m')))
    import matlab.buildtool.tasks.TestTask
    plan("test") = TestTask(testDir, SourceFiles=fullfile(buildDir, '+ctoon'));
    plan("test").Dependencies = "build";
    
    % 2. Coverage Task
    plan("coverage").Dependencies = "test";
end

plan("install").Dependencies = "build";
plan("clean").Dependencies   = {};
plan("config").Dependencies  = {};
plan.DefaultTasks            = "build";

end

% =========================================================================
% TASK IMPLEMENTATIONS
% =========================================================================

function buildTask(~, force)
%BUILD  Compile CToon MEX gateway and export the package.
%   ARGUMENTS:
%     force (logical) - If true, re-compiles even if the binary exists.
    arguments
        ~
        force (1,1) = false
    end
    force = asLogical(force);
    here = fileparts(mfilename('fullpath'));
    [buildDir, ~] = get_config_val('build_dir', here);

    ctoon_build(char(buildDir), force);
    addpath(char(buildDir));
end

function coverageTask(~)
%COVERAGE  Generate dark-themed HTML report and LCOV tracefile.
    here = fileparts(mfilename('fullpath'));
    [buildDir, ~] = get_config_val('build_dir', here);
    [covDir, ~]   = get_config_val('coverage_output_dir', fullfile(buildDir, 'coverage'));
    testDir       = fullfile(here, '..', '..', '..', 'tests', 'matlab');

    covLcovFile = fullfile(covDir, 'coverage.lcov');
    covHtmlDir  = fullfile(covDir, 'html');
    
    % Add the built package to path so tests can find ctoon.*
    addpath(char(buildDir));

    if ~isfolder(covDir), mkdir(covDir); end

    % 1. Setup Test Suite and Runner
    suite = testsuite(testDir);
    runner = matlab.unittest.TestRunner.withNoPlugins;

    % 2. Define Output Formats
    % We create one formatter for HTML and one for raw data collection
    htmlFormat = matlab.unittest.plugins.codecoverage.CoverageReport(covHtmlDir);
    lcovCollector = matlab.unittest.plugins.codecoverage.CoverageResult;

    % 3. Add ONE plugin with BOTH formatters (using an array)
    % This avoids the "Overlapping Sources" error
    combinedPlugin = matlab.unittest.plugins.CodeCoveragePlugin.forFolder( ...
        fullfile(buildDir, '+ctoon'), ...
        'Producing', [htmlFormat, lcovCollector]);
    runner.addPlugin(combinedPlugin);

    % 4. Run Tests
    fprintf('  [Coverage] Running tests and collecting combined data...\n');
    results = runner.run(suite);
    
    if any([results.Failed])
        error('coverageTask:testsFailed', 'Tests failed. Coverage generation aborted.');
    end

    % 5. Post-Processing: Dark Theme for HTML
    inject_dark_theme(covHtmlDir);

    % 6. Post-Processing: Generate LCOV
    % Access the collected data through the 'Result' property of the collector
    if ~isempty(lcovCollector.Result)
        generate_lcov_report(lcovCollector.Result, covLcovFile);
        fprintf('  [Coverage] LCOV tracefile generated.\n');
    else
        warning('coverageTask:noData', 'No coverage data was captured.');
    end

    fprintf('\n✅ Coverage Complete:\n  HTML (Dark Mode): %s\n  LCOV Tracefile:  %s\n', ...
        fullfile(covHtmlDir, 'index.html'), covLcovFile);
end


function installTask(~, force, verify)
%INSTALL  Build and permanently add CToon to MATLAB Path.
    arguments
        ~
        force (1,1) = false
        verify (1,1) = true
    end
    force = asLogical(force);
    verify = asLogical(verify);
    here = fileparts(mfilename('fullpath'));
    [buildDir, ~] = get_config_val('build_dir', here);
    ctoon_install(char(buildDir), force, verify);
end

function cleanTask(~)
%CLEAN  Deep clean build artifacts and reset settings.
%
%   DESCRIPTION:
%     1. Invokes ctoon_clean to remove packages and binaries.
%     2. Deletes the .buildtool/ folder to reset config.ini to defaults.

    here = fileparts(mfilename('fullpath'));

    % 1. Get the current build directory from config
    [currentBuildDir, isDefault] = get_config_val('build_dir', here);

    % 2. Clean Artifacts
    ctoon_clean(currentBuildDir); % Clean custom dir

    % 3. Reset Configuration (Task-specific responsibility)
    configFolder = fullfile(here, '.buildtool');
    if isfolder(configFolder)
        rmdir(configFolder, 's');
        fprintf('  [Clean] Reset configurations (deleted .buildtool/)\n');
    end

    fprintf('  [Clean] Project is now in a fresh state.\n');
    end

function configTask(~, options)
%CONFIG  View or update persistent project configurations.
%
%   SYNTAX:
%     buildtool config                          % Show settings
%     buildtool config(buildDir='/tmp/out')     % Update buildDir
    arguments
        ~
        options.buildDir (1,1) string = ""
        options.coverageOutputDir (1,1) string = ""
    end

    here = fileparts(mfilename('fullpath'));
    iniFile = fullfile(here, '.buildtool', 'config.ini');
    ini = ini_read(iniFile);

    % Update values
    if options.buildDir ~= ""
        ini.build_dir = char(absolutepath(options.buildDir));
    end
    if options.coverageOutputDir ~= ""
        ini.coverage_output_dir = char(absolutepath(options.coverageOutputDir));
    end

    if ~isempty(fieldnames(options))
        ini_write(iniFile, ini);
        fprintf('  [Config] Settings saved to %s\n', iniFile);
    end

    % Display with Default detection
    [bPath, bDef] = get_config_val('build_dir', here);
    [cPath, cDef] = get_config_val('coverage_output_dir', fullfile(bPath, 'coverage'));

    fprintf('\n--- CToon Project Configuration ---\n');
    if bDef, bSuffix = " (default)"; else, bSuffix = ""; end
    if cDef, cSuffix = " (default)"; else, cSuffix = ""; end

    fprintf('  build_dir           = %s%s\n', bPath, bSuffix);
    fprintf('  coverage_output_dir = %s%s\n', cPath, cSuffix);
    fprintf('  config_file         = %s\n\n', iniFile);
end

% =========================================================================
% HELPERS
% =========================================================================

function [val, isDefault] = get_config_val(key, default)
% Returns value and a boolean indicating if it's a default value.
    here = fileparts(mfilename('fullpath'));
    ini = ini_read(fullfile(here, '.buildtool', 'config.ini'));
    if isfield(ini, key)
        val = ini.(key);
        isDefault = false;
    else
        val = default;
        isDefault = true;
    end
end

function generate_lcov_report(covResults, outputFile)
% GENERATE_LCOV_REPORT  Robust LCOV generator for MATLAB R2024a+
% This version uses dynamic field detection to avoid "Unrecognized method/property" errors.
    fid = fopen(outputFile, 'w');
    if fid == -1, return; end
    
    for i = 1:numel(covResults)
        res = covResults(i);
        props = properties(res);
        
        % --- 1. Find Filename (Case-insensitive search) ---
        idx = find(strcmpi(props, 'Filename') | strcmpi(props, 'File') | strcmpi(props, 'Source'), 1);
        if ~isempty(idx)
            filePath = res.(props{idx});
        else
            filePath = 'unknown_file';
        end
        
        fprintf(fid, 'TN:\nSF:%s\n', filePath);
        
        % --- 2. Find Line Data (Search for LineData, Lines, or LineCoverage) ---
        % In R2024a, the standard is usually 'LineData'
        idxData = find(strcmpi(props, 'LineData') | strcmpi(props, 'LineCoverage') | strcmpi(props, 'Lines'), 1);
        
        if ~isempty(idxData)
            lc = res.(props{idxData});
            
            % Each lc object (LineData) has: Line, Executable, HitCount
            % We use dynamic access here too just in case
            lns = lc.Line;
            exe = lc.Executable;
            hits = lc.HitCount;
            
            for j = 1:numel(lns)
                if exe(j)
                    % DA:<line_number>,<hit_count>
                    fprintf(fid, 'DA:%d,%d\n', lns(j), hits(j));
                end
            end
            
            % LF: Lines Found (Executable), LH: Lines Hit
            fprintf(fid, 'LF:%d\n', sum(exe));
            fprintf(fid, 'LH:%d\n', sum(hits(exe) > 0));
        end
        
        fprintf(fid, 'end_of_record\n');
    end
    fclose(fid);
end

function inject_dark_theme(htmlDir)
% CSS Injection for Dark Mode Coverage Reports.
cssFile = fullfile(htmlDir, 'scoverage.css');
if isfile(cssFile)
    darkStyles = [ ...
        'body, html { background: #1a1a1a !important; color: #e0e0e0 !important; } ' ...
        '.directory-table, .file-table { background: #2d2d2d !important; color: #e0e0e0 !important; } ' ...
        'tr:nth-child(even) { background: #252525 !important; } ' ...
        'a { color: #4db8ff !important; } ' ...
        '.gray { background: #444 !important; } ' ...
        '.green { background: #1b4d1b !important; border: 1px solid #2e7d32; } ' ...
        '.red { background: #4d1b1b !important; border: 1px solid #c62828; } ' ...
        'h1, h2, .footer { color: #ffffff !important; }'];
    fid = fopen(cssFile, 'a');
    if fid ~= -1
        fprintf(fid, '\n/* CToon Dark Mode Patch */\n%s', darkStyles);
        fclose(fid);
    end
end
end

function ini = ini_read(f)
    % Basic INI parser.
    ini = struct(); if ~isfile(f), return; end
    lines = splitlines(fileread(f));
    for i = 1:numel(lines)
        ln = strtrim(lines{i});
        if isempty(ln) || any(ln(1) == '#;['), continue; end
        parts = split(ln, '=');
        if numel(parts) < 2, continue; end
        key = strrep(strtrim(parts{1}), '-', '_');
        ini.(key) = strtrim(parts{2});
    end
end

function ini_write(f, s)
    % Basic INI writer.
    d = fileparts(f); if ~isfolder(d), mkdir(d); end
    fid = fopen(f, 'w'); fprintf(fid, '[buildtool]\n');
    fns = fieldnames(s);
    for i = 1:numel(fns), fprintf(fid, '%s = %s\n', fns{i}, s.(fns{i})); end
    fclose(fid);
end

function absPath = absolutepath(inputPath)
    % 1. Try to resolve directly via native file attributes
    [status, info] = fileattrib(char(inputPath));
    if status, absPath = info.Name; return; end

    % 2. Fallback if path doesn't exist on disk yet
    [parent, name, ext] = fileparts(char(inputPath));
    [pStatus, pInfo] = fileattrib(parent);

    if pStatus
        absPath = fullfile(pInfo.Name, [name, ext]);
    else
        absPath = fullfile(pwd, inputPath); % Ultimate fallback
    end

    % 3. Standardize system slashes
    absPath = strrep(strrep(absPath, '/', filesep), '\', filesep);
end

function out = asLogical(val)
    % ASLOGICAL  Convert string, numeric or logical to actual logical.
    %   Useful for buildtool tasks where CLI arguments arrive as strings.
    if islogical(val)
        out = val;
    elseif isnumeric(val)
        out = logical(val);
    elseif ischar(val) || isstring(val)
        % Handles "true", "false", "1", "0" from CLI
        out = strcmpi(val, "true") || val == "1";
    else
        out = logical(val);
    end
end

