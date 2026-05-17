function plan = buildfile
%BUILDFILE  MATLAB Build Tool plan for the CToon MATLAB MEX binding.
%
%   buildtool                        % default: build
%   buildtool build                  % compile ctoon_mex
%   buildtool build('/tmp/ctoon')    % compile into custom dir
%   buildtool build([], true)        % force recompile
%   buildtool test                   % build + run tests
%   buildtool coverage               % build + tests + HTML + lcov
%   buildtool install                % build + install to MATLAB path
%   buildtool install('/opt/ctoon')  % install to custom dir
%   buildtool clean                  % remove BUILD_DIR
%   buildtool setting                % show current settings
%   buildtool setting(BuildDir='/tmp/ctoon')           % set build dir
%   buildtool setting(CoverageOutputDir='/tmp/cov')    % set coverage dir
%   buildtool -tasks                 % list tasks
%
%   Settings are persisted in .buildtool/settings.ini next to this file.
%   Global variables (BUILD_DIR, COVERAGE_OUTPUT_DIR) take precedence
%   over buildtool.ini when set.

import matlab.buildtool.tasks.TestTask

global BUILD_DIR COVERAGE_OUTPUT_DIR

here     = fileparts(mfilename('fullpath'));
repoRoot = fullfile(here, '..', '..', '..');
testDir  = fullfile(repoRoot, 'tests', 'matlab');

buildDir = resolve(BUILD_DIR, 'build_dir', here);
covDir   = resolve(COVERAGE_OUTPUT_DIR, 'coverage_output_dir', fullfile(buildDir, 'coverage'));

% ---- plan ----------------------------------------------------------------
plan = buildplan(localfunctions);

if isfolder(testDir) && ~isempty(dir(fullfile(testDir, 'test_*.m')))
    plan("test") = TestTask(testDir, SourceFiles=buildDir) ...
        .addCodeCoverage(fullfile(covDir, 'html', 'index.html'));
    plan("test").Dependencies     = "build";
    plan("coverage").Dependencies = "test";
else
    plan.Tasks = rmfield(plan.Tasks, ["test" "coverage"]);
end

plan("install").Dependencies = "build";
plan("clean").Dependencies   = {};
plan("setting").Dependencies = {};
plan.DefaultTasks            = "build";

end


% =========================================================================
% Tasks
% =========================================================================

function buildTask(~, buildDir, force)
%BUILD  Compile ctoon_mex.
arguments
    ~
    buildDir (1,1) string  = ""
    force    (1,1) logical = false
end
global BUILD_DIR
here = fileparts(mfilename('fullpath'));
if buildDir == ""
    buildDir = resolve(BUILD_DIR, 'build_dir', here);
end
ctoon_build(char(buildDir), force);
addpath(char(buildDir));
end


function coverageTask(~)
%COVERAGE  Write lcov tracefile and HTML report.
global BUILD_DIR COVERAGE_OUTPUT_DIR
here     = fileparts(mfilename('fullpath'));
repoRoot = fullfile(here, '..', '..', '..');
buildDir = resolve(BUILD_DIR, 'build_dir', here);
covDir   = resolve(COVERAGE_OUTPUT_DIR, 'coverage_output_dir', fullfile(buildDir, 'coverage'));
testDir  = fullfile(repoRoot, 'tests', 'matlab');

covLcovFile = fullfile(covDir, 'coverage.lcov');
covHtmlDir  = fullfile(covDir, 'html');
covMatFile  = fullfile(covDir, 'coverage.mat');

if ~isfolder(covDir),     mkdir(covDir);     end
if ~isfolder(covHtmlDir), mkdir(covHtmlDir); end

suite = testsuite(testDir);

runner = matlab.unittest.TestRunner.withNoPlugins;
runner.addPlugin(matlab.unittest.plugins.CodeCoveragePlugin.forFolder( ...
    buildDir, ...
    Producing=matlab.unittest.plugins.codecoverage.CoverageReport(covHtmlDir)));
results = runner.run(suite);
if any([results.Failed])
    error('coverageTask:testsFailed', 'Tests failed during coverage run.');
end
fprintf('  HTML report -> %s\n', fullfile(covHtmlDir, 'index.html'));

try
    runner2 = matlab.unittest.TestRunner.withNoPlugins;
    runner2.addPlugin(matlab.unittest.plugins.CodeCoveragePlugin.forFolder( ...
        buildDir, ...
        Producing=matlab.unittest.plugins.codecoverage.CoverageResult(covMatFile)));
    runner2.run(suite);
    result = load(covMatFile).result;
    write(result, covLcovFile);
    if isfile(covMatFile), delete(covMatFile); end
    fprintf('  lcov -> %s\n', covLcovFile);
catch
    fid = fopen(covLcovFile, 'w');
    if fid ~= -1
        fprintf(fid, 'TN:\nend_of_record\n');
        fclose(fid);
    end
    fprintf('  lcov stub -> %s (R2024b+ for full lcov)\n', covLcovFile);
end
end


function installTask(~, buildDir, force, verify)
%INSTALL  Build and permanently add CToon to MATLAB path.
arguments
    ~
    buildDir (1,1) string  = ""
    force    (1,1) logical = false
    verify   (1,1) logical = true
end
global BUILD_DIR
here = fileparts(mfilename('fullpath'));
if buildDir == ""
    buildDir = resolve(BUILD_DIR, 'build_dir', here);
end
ctoon_install(char(buildDir), force, verify);
end


function cleanTask(~)
%CLEAN  Remove BUILD_DIR and COVERAGE_OUTPUT_DIR.
global BUILD_DIR COVERAGE_OUTPUT_DIR
here     = fileparts(mfilename('fullpath'));
buildDir = resolve(BUILD_DIR, 'build_dir', here);
covDir   = resolve(COVERAGE_OUTPUT_DIR, 'coverage_output_dir', fullfile(buildDir, 'coverage'));

if isfolder(covDir) && ~strcmp(covDir, here)
    rmdir(covDir, 's');
    fprintf('  Removed: %s\n', covDir);
end
if isfolder(buildDir) && ~strcmp(buildDir, here)
    rmdir(buildDir, 's');
    fprintf('  Removed: %s\n', buildDir);
else
    mexFile = fullfile(here, ['ctoon_mex.' mexext]);
    if isfile(mexFile)
        delete(mexFile);
        fprintf('  Removed: %s\n', mexFile);
    end
end
end


function settingTask(~, options)
%SETTING  Show or update build settings (persisted in buildtool.ini).
%
%   buildtool setting                              % show current settings
%   buildtool setting(BuildDir='/tmp/ctoon')       % set build dir
%   buildtool setting(CoverageOutputDir='/tmp/c')  % set coverage dir
arguments
    ~
    options.BuildDir          (1,1) string = ""
    options.CoverageOutputDir (1,1) string = ""
end

here    = fileparts(mfilename('fullpath'));
iniFile = fullfile(here, '.buildtool', 'settings.ini');
ini     = ini_read(iniFile);

% Apply updates
changed = false;
if options.BuildDir ~= ""
    ini.build_dir = char(absolutepath(options.BuildDir));
    changed = true;
end
if options.CoverageOutputDir ~= ""
    ini.coverage_output_dir = char(absolutepath(options.CoverageOutputDir));
    changed = true;
end

if changed
    ini_write(iniFile, ini);
    fprintf('Settings saved to %s\n', iniFile);
end

% Show current effective settings
global BUILD_DIR COVERAGE_OUTPUT_DIR
here2    = fileparts(mfilename('fullpath'));
buildDir = resolve(BUILD_DIR, 'build_dir', here2);
covDir   = resolve(COVERAGE_OUTPUT_DIR, 'coverage_output_dir', fullfile(buildDir, 'coverage'));
fprintf('\n  build_dir           = %s\n', buildDir);
fprintf('  coverage_output_dir = %s\n\n', covDir);
if isfile(iniFile)
    fprintf('  (from buildtool.ini — override with global BUILD_DIR / COVERAGE_OUTPUT_DIR)\n\n');
end
end


% =========================================================================
% INI helpers
% =========================================================================

function ini = ini_read(iniFile)
% Parse [section] key=value INI file into a flat struct.
ini = struct();
if ~isfile(iniFile), return; end
lines = splitlines(strtrim(fileread(iniFile)));
for k = 1:numel(lines)
    ln = strtrim(lines{k});
    if isempty(ln) || ln(1) == '#' || ln(1) == ';' || ln(1) == '['
        continue
    end
    eq = strfind(ln, '=');
    if isempty(eq), continue; end
    key = strtrim(ln(1:eq(1)-1));
    val = strtrim(ln(eq(1)+1:end));
    ini.(strrep(key, '-', '_')) = val;
end
end


function ini_write(iniFile, ini)
% Write struct fields as key = value under [buildtool] section.
iniDir = fileparts(iniFile);
if ~isfolder(iniDir)
    mkdir(iniDir);
end
fid = fopen(iniFile, 'w');
if fid == -1
    error('buildfile:iniWrite', 'Cannot write: %s', iniFile);
end
fprintf(fid, '[buildtool]\n');
for k = fieldnames(ini)'
    fprintf(fid, '%s = %s\n', k{1}, ini.(k{1}));
end
fclose(fid);
end


% =========================================================================
% Path resolver: global var > buildtool.ini > default
% =========================================================================

function val = resolve(globalVar, iniKey, default)
% 1. explicit global variable
if ~isempty(globalVar) && ~(isstring(globalVar) && globalVar == "")
    val = strtrim(char(globalVar));
    return;
end
% 2. buildtool.ini
here = fileparts(mfilename('fullpath'));
ini  = ini_read(fullfile(here, '.buildtool', 'settings.ini'));
if isfield(ini, iniKey) && ~isempty(ini.(iniKey))
    val = ini.(iniKey);
    return;
end
% 3. default
val = default;
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
