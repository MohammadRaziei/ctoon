function plan = buildfile
%BUILDFILE  MATLAB Build Tool plan for the CToon project.
%
%   This file defines a set of automated tasks for compiling, testing, 
%   and installing the CToon MATLAB bindings. It uses the MATLAB Build 
%   Tool (introduced in R2022b).
%
%   USAGE:
%     buildtool <task>              % Run a specific task
%     buildtool                     % Run the default task (build)
%     buildtool -tasks              % List all available tasks
%
%   TASKS:
%     build      - Compiles MEX binary and prepares the +ctoon package.
%     test       - Executes the unit test suite (requires tests/matlab).
%     coverage   - Runs tests and generates a dark-themed coverage report.
%     install    - Builds and permanently adds the package to MATLAB Path.
%     clean      - Deletes build artifacts and removes paths.
%     setting    - Configures persistent project settings (INI-based).
%
%   CONFIGURATION:
%     Settings are persisted in '.buildtool/settings.ini'. You can view or
%     change them using the 'setting' task. Global variables are no longer
%     used to ensure thread-safety and persistence across sessions.

here = fileparts(mfilename('fullpath'));

% ---- Configuration & Plan Initialization --------------------------------
plan = buildplan(localfunctions);

% Local path resolution
repoRoot = fullfile(here, '..', '..', '..');
testDir  = fullfile(repoRoot, 'tests', 'matlab');
buildDir = get_setting('build_dir', here);
covDir   = get_setting('coverage_output_dir', fullfile(buildDir, 'coverage'));

% ---- Task Definitions & Dependencies ------------------------------------

% 1. Test Task: Depends on Build
if isfolder(testDir) && ~isempty(dir(fullfile(testDir, 'test_*.m')))
    import matlab.buildtool.tasks.TestTask
    plan("test") = TestTask(testDir, SourceFiles=fullfile(buildDir, '+ctoon'));
    plan("test").Dependencies = "build";
    
    % 2. Coverage Task: Runs after Test
    plan("coverage").Dependencies = "test";
end

% 3. Install Task: Builds the latest before installing
plan("install").Dependencies = "build";

% 4. Clean & Setting: Independent tasks
plan("clean").Dependencies   = {};
plan("setting").Dependencies = {};

% Default entry point
plan.DefaultTasks = "build";

end

% =========================================================================
% TASK IMPLEMENTATIONS
% =========================================================================

function buildTask(~, force)
%BUILD  Compile the CToon MEX gateway and export the MATLAB package.
%
%   SYNTAX:
%     buildtool build
%     buildtool build(force=true)
%
%   DESCRIPTION:
%     1. Locates the C source files (ctoon.c, ctoon_mex.c).
%     2. Invokes the MEX compiler to generate the binary.
%     3. Exports the +ctoon package to the designated build directory.
%     4. Adds the build directory to the current MATLAB session path.
%
%   ARGUMENTS:
%     force (logical) - If true, re-compiles even if the binary exists.

arguments
    ~
    force (1,1) logical = false
end
buildDir = get_setting('build_dir', fileparts(mfilename('fullpath')));
ctoon_build(char(buildDir), force);
addpath(char(buildDir));
end


function coverageTask(~)
%COVERAGE  Run unit tests and generate comprehensive coverage reports.
%
%   DESCRIPTION:
%     This task executes all tests in the 'tests/matlab' directory and
%     monitors code execution within the '+ctoon' package. 
%     It produces two types of output:
%       1. HTML Report: A visual, dark-themed report (scoverage.css hacked).
%       2. LCOV File: A standard 'coverage.lcov' file for CI/CD integration.
%
%   OUTPUTS:
%     - <buildDir>/coverage/html/index.html
%     - <buildDir>/coverage/coverage.lcov

here     = fileparts(mfilename('fullpath'));
buildDir = get_setting('build_dir', here);
covDir   = get_setting('coverage_output_dir', fullfile(buildDir, 'coverage'));
testDir  = fullfile(here, '..', '..', '..', 'tests', 'matlab');

covLcovFile = fullfile(covDir, 'coverage.lcov');
covHtmlDir  = fullfile(covDir, 'html');
covMatFile  = fullfile(covDir, 'coverage.mat');

if ~isfolder(covDir), mkdir(covDir); end

% Setup Test Runner with Coverage Plugins
suite = testsuite(testDir);
runner = matlab.unittest.TestRunner.withNoPlugins;

% Plugin for HTML report
runner.addPlugin(matlab.unittest.plugins.CodeCoveragePlugin.forFolder( ...
    fullfile(buildDir, '+ctoon'), ...
    Producing=matlab.unittest.plugins.codecoverage.CoverageReport(covHtmlDir)));

% Plugin for Raw Data (to generate LCOV)
runner.addPlugin(matlab.unittest.plugins.CodeCoveragePlugin.forFolder( ...
    fullfile(buildDir, '+ctoon'), ...
    Producing=matlab.unittest.plugins.codecoverage.CoverageResult(covMatFile)));

results = runner.run(suite);
if any([results.Failed]), error('coverageTask:failure', 'Tests failed.'); end

% Post-processing: Inject Dark Theme CSS
inject_dark_theme(covHtmlDir);

% Post-processing: Generate LCOV from MATLAB results
if isfile(covMatFile)
    data = load(covMatFile);
    generate_lcov(data.result, covLcovFile);
    delete(covMatFile);
end

fprintf('\n✅ Coverage Reports Generated:\n');
fprintf('  🌐 HTML (Dark Mode): %s\n', fullfile(covHtmlDir, 'index.html'));
fprintf('  📊 LCOV Tracefile:  %s\n', covLcovFile);
end


function installTask(~, force, verify)
%INSTALL  Compile and permanently install CToon to the MATLAB Path.
%
%   SYNTAX:
%     buildtool install
%     buildtool install(force=true)
%
%   DESCRIPTION:
%     Runs the 'build' task and then uses 'savepath' to ensure the 
%     package is available in future MATLAB sessions. It also runs
%     a verification check to ensure the MEX is functional.

arguments
    ~
    force (1,1) logical = false
    verify (1,1) logical = true
end
buildDir = get_setting('build_dir', fileparts(mfilename('fullpath')));
ctoon_install(char(buildDir), force, verify);
end


function cleanTask(~)
%CLEAN  Remove build artifacts and purge project from MATLAB path.
%
%   DESCRIPTION:
%     1. Removes the Build directory from the disk.
%     2. Removes the Coverage output directory.
%     3. Detects if the Build directory is in the MATLAB path and removes 
%        it permanently using 'rmpath' and 'savepath'.

here     = fileparts(mfilename('fullpath'));
buildDir = get_setting('build_dir', here);

% Path Cleanup
p = split(path, pathsep);
if any(strcmpi(buildDir, p))
    rmpath(buildDir);
    savepath;
    fprintf('  [Clean] Removed build directory from MATLAB path.\n');
end

% Disk Cleanup
if isfolder(buildDir) && ~strcmp(buildDir, here)
    rmdir(buildDir, 's');
    fprintf('  [Clean] Deleted directory: %s\n', buildDir);
end
end


function settingTask(~, options)
%SETTING  Display or update persistent project configurations.
%
%   SYNTAX:
%     buildtool setting                          % View current config
%     buildtool setting(BuildDir='/tmp/out')     % Update Build path
%
%   DESCRIPTION:
%     Settings are saved in an INI file. Changes are persistent across 
%     MATLAB restarts. Paths are automatically converted to absolute.

arguments
    ~
    options.BuildDir (1,1) string = ""
    options.CoverageOutputDir (1,1) string = ""
end

here = fileparts(mfilename('fullpath'));
iniFile = fullfile(here, '.buildtool', 'settings.ini');
ini = ini_read(iniFile);

% Update logic
if options.BuildDir ~= ""
    ini.build_dir = char(absolutepath(options.BuildDir));
end
if options.CoverageOutputDir ~= ""
    ini.coverage_output_dir = char(absolutepath(options.CoverageOutputDir));
end

if ~isempty(fieldnames(options))
    ini_write(iniFile, ini);
    fprintf('  [Settings] Updated configurations saved.\n');
end

% Summary Display
fprintf('\n--- CToon Project Settings ---\n');
fprintf('  Build Directory:    %s\n', get_setting('build_dir', here));
fprintf('  Coverage Directory: %s\n', get_setting('coverage_output_dir', 'default'));
fprintf('  Settings File:      %s\n\n', iniFile);
end

% =========================================================================
% HELPER FUNCTIONS
% =========================================================================

function val = get_setting(key, default)
% GET_SETTING  Retrieve a value from the persistent INI file.
here = fileparts(mfilename('fullpath'));
ini = ini_read(fullfile(here, '.buildtool', 'settings.ini'));
if isfield(ini, key), val = ini.(key); else, val = default; end
end

function generate_lcov(covResult, outputFile)
% GENERATE_LCOV  Convert MATLAB CoverageResult to standard LCOV format.
%
%   LCOV format is required for tools like Codecov or SonarQube.
%   This function manually maps the MATLAB LineHitCount properties to 
%   standard LCOV tracefile fields (SF, DA, LF, LH).
fid = fopen(outputFile, 'w');
if fid == -1, return; end
for i = 1:numel(covResult)
    res = covResult(i);
    fprintf(fid, 'TN:\nSF:%s\n', res.FileName);
    lines = find(res.LineExecutable);
    for j = 1:numel(lines)
        ln = lines(j);
        fprintf(fid, 'DA:%d,%d\n', ln, res.LineHitCount(ln));
    end
    fprintf(fid, 'LF:%d\nLH:%d\nend_of_record\n', ...
        sum(res.LineExecutable), sum(res.LineHitCount(res.LineExecutable) > 0));
end
fclose(fid);
end

function inject_dark_theme(htmlDir)
% INJECT_DARK_THEME  CSS Hack to turn the default HTML report dark.
%
%   Appends dark-mode CSS variables and overrides to 'scoverage.css'.
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
% INI_READ  Basic parser for [section] key=value files.
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
% INI_WRITE  Basic writer for project configurations.
d = fileparts(f); if ~isfolder(d), mkdir(d); end
fid = fopen(f, 'w'); fprintf(fid, '[buildtool]\n');
fns = fieldnames(s);
for i = 1:numel(fns), fprintf(fid, '%s = %s\n', fns{i}, s.(fns{i})); end
fclose(fid);
end

function absPath = absolutepath(p)
% ABSOLUTEPATH  Ensures cross-platform absolute path resolution.
[s, info] = fileattrib(char(p));
if s, absPath = info.Name; else, absPath = char(p); end
end